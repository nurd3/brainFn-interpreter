// brainFn-interpreter/main.c
// author    Dante Davis
// from      2023 April 20th
// to        2023 April 22nd
// GNU GPL 3.0 COPYLEFT LICENSE
// i apologize for the rather disgusting mess that is this code.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stacks.h"

#ifdef _WIN32
	#include <io.h>
	#define CLEAR			system("cls")
	#define EXISTS(path)	_access(path, 0)
#endif
#ifdef __unix__
	#include <unistd.h>
	#define CLEAR			system("clear")
	#define EXISTS(path)	access(path, F_OK)
#endif
#define RESET				"\033[0m"
#define RED					"\033[31m"
#define YELW				"\033[0;33m"
#define ERR(x...)			{fprintf(DEBUG_STREAM, "bfn err: " x); errs++;}
#define EXCEPTION(x...)		{fprintf(DEBUG_STREAM, "bfn exception at line %d: ", line + 1); fprintf(DEBUG_STREAM, x); errs++; exit(0);}
#define PARSER_ERR(x...)	{fprintf(DEBUG_STREAM, "bfn parse error at line %d: ", line + 1); fprintf(DEBUG_STREAM, x); errs++;}
#define WARN(x...)			{fprintf(DEBUG_STREAM, "\nbfn warning at line %d: ", line); fprintf(DEBUG_STREAM, x); errs++; break;}
#define DEBUG(x...)			{if (debugMode) fprintf(DEBUG_STREAM, x);}
#define IS_ABCNUM(c)		((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))

struct LABEL {
	char name[33];
	u_int gto;
	struct LABEL *next;
};
struct loop_stack {
	u_int gto;
	struct loop_stack *next;
};
// used for passing arguments to functions
struct stack *inp;
// used for returning values from functions
struct stack *out;
struct loop_stack *loopStack;
struct LABEL *labels;
FILE *DEBUG_STREAM;
int debugMode = 0;

u_int tSize = 65536;
u_int len = 0;
u_int fnCalls = 0;			// the amount of times a function has been called
u_int scope = 0;			// how deep we are in scope

// helper function that checks if two strings are equal
int eq(char a[], char b[], int len);
// helper function that checks what line a given index is on in a string
u_int getLine(u_int index, char *str);
// the function that interprets the bfn code
int run(u_int index, u_int len, char *src, char *tape);

// main function, sets things up
int main(int argc, char **argv) {
	CLEAR;
	u_int errs = 0;
	DEBUG_STREAM = stdout;
	// initialize the stacks
	inp = new_stack();
	out = new_stack();
	
	if (argc < 2)
		ERR("no script provided\n");
	if (argc > 2) {
		int mult = 1;
		for (int i = strlen(argv[2]) - 1; i + 1; i--) {
			u_int digit = argv[2][i] - '0';
			if (digit > 9 || digit < 0)
				ERR("invalid tape size\n");
			tSize += digit * mult;
			mult *= 10;
		}	
	}
	DEBUG_STREAM = stdout;
	// read flags
	if (argc > 3) {
		for (u_int i = 3; i < argc; ++i){
			if (!strcmp("-log", argv[i])) {
				if (argc == i + 1) {
					ERR("expected location after \"-log\"\n");
				}
				else {
					DEBUG_STREAM = fopen(argv[i + 1], "w");
					if (DEBUG_STREAM == NULL)
						ERR("couldn't use \"%s\" for logging", argv[i + 1]);
				}
				i++;
			}
			if (!strcmp("-debug", argv[i])) {
				debugMode = 1;
			}
		}
	}
	// if the tape is too small then the program will most likely
			// encounter a segfault when trying to edit the tape
	if (tSize < 3)
		ERR("tape size too small\n");

	// open the script
	FILE *fp = fopen(argv[1], "r");
	if (fp == NULL)		// if we cant access the file or it doesnt exist
		ERR("couldn't find file %s\n", argv[1]);

	// initialize the label array (used for labels (duh))
	labels = calloc(1, sizeof(struct LABEL));
	// initiallize the loop stack
	loopStack = calloc(1, sizeof(struct loop_stack));

	// read the file contents into a string
	char *src;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	src = calloc(len, sizeof(char));
	if (src) {
		fread(src, 1, len, fp);
	} else {
		ERR("script is empty\n");
	}
	fclose(fp);
	
	// exit if there are any errors
	if (errs)
		exit(0);

	// run the script
	run(0, len, src, NULL);

	DEBUG("program terminated naturally with %d function calls in total\n", fnCalls);
	return 0;
}

// helper function that checks if two strings are equal
int eq(char a[], char b[], int len) {
	for (int i = len; i; --i)
		if (a[i] != b[i])
			return 0;
	return 1;
}
// helper function that checks what line a given index is on in a string
u_int getLine(u_int index, char *str) {
	if (index == 0)
		return 0;
	u_int line = 0;
	u_int len = strlen(str);
	for (int i = 0; i < index && i < len; i++) {
		if (str[i] == '\n')
			line++;
	}
	return line;
}
// the function that interprets the bfn code
int run(u_int index, u_int len, char *src, char *tape) {
	u_int errs = 0;
	if (tape == NULL)
		tape = calloc(tSize, sizeof(char));
	u_int tp = 0;
	u_int line = getLine(index, src);
	// label variable is used to store
	// the name of a label
	// before it is used by the program
	char label[33];
	DEBUG("length of script: %d, indexing from: %d\n", len, index);
	for (u_int i = index; i < len; i++) {
		DEBUG("%d %c\n", i, src[i]);
		if (src[i] == '\n')
			line++;
		else if (src[i] == '=')		// reset pointer value
			tape[tp] = 0;
		else if (src[i] == '+')		// increment
			++tape[tp];
		else if (src[i] == '-')		// decrement
			--tape[tp];
		else if (src[i] == '.')		// print
			putc(tape[tp], stdout);
		else if (src[i] == ',')		// input
			tape[tp] = getc(stdin);
		else if (src[i] == '@')		// reset pointer position
			tp = 0;
		else if (src[i] == '>')		// shift pointer to the right
			++tp;
		else if (src[i] == '<')		// shift pointer to the left
			--tp;
		else if (src[i] == '$')		// push to input stack
			push(inp, tape[tp]);
		else if (src[i] == '&')		// push to return stack
			push(out, tape[tp]);
		else if (src[i] == '%')		// pop from input stack
			tape[tp] = pop(inp);
		else if (src[i] == ':')		// pop from return stack
			tape[tp] = pop(out);
		else if (src[i] == '[') {	// while loop
			struct loop_stack *x = calloc(1, sizeof(struct stack));
			x->next = loopStack;
			x->gto = i;
			loopStack = x;
		} else if (src[i] == ']') {
			if (loopStack->next == NULL) {
				WARN("unexpected ']'\n");
				return 0;
			} else if (tape[tp])
				i = loopStack->gto;
			else {
				struct loop_stack *x = loopStack;
				loopStack->gto = loopStack->next->gto;
				struct stack *y = loopStack->next->next;
				free(loopStack->next);
				loopStack->next = y;
			}
		} else if (src[i] == '\'') {
			int j = 0;
			int stop = 0;
			i++;
			while (i < len && stop == 0) {
				label[j++] = src[i++];
				if (j > 31) {
					EXCEPTION("label reference too long\n");
				}
				if (src[i] == '\'') {
					label[j] = 0;
					break;
				}
				else if (!IS_ABCNUM(src[i]) && src[i] != '_') {
					if (src[i] == '\n') {
						EXCEPTION("illegal label character '\\n'\n")
					}
					else if (src[i] == '\t') {
						EXCEPTION("illegal label character '\\t'\n")
					}
					else if (src[i] == '\b') {
						EXCEPTION("illegal label character '\\b'\n")
					}
					else if (src[i] == '\0') {
						EXCEPTION("illegal label character '\\0'\n")
					}
					else {
						EXCEPTION("illegal label character '%c'\n", src[i])
					}
				}
			}
			// some debug code that prints the label
			DEBUG("%d \"%s\"\n", i, label);
		} else if (src[i] == '{') {		// function declaration
			// create the label
			struct LABEL *l = calloc(1, sizeof(struct LABEL));
			l->gto = i;		// set the position to go to when called
			
			for (int j = 0; j < 32; ++j) {	// set the name of the label
				l->name[j] = label[j];
				label[j] = 0;
			}
			struct LABEL *x = labels;
			
			while (x->next != NULL)	// get the last label
				x = x->next;

			x->next = l;	// append the label
			// we don't want to run anything inside the function
					// until it is called
					// so we skip until the function is closed
			int nestCount = 1;		// used to count the number of curly brackets
			int extraLines = 0;		// counts the amount the line breaks are encountered while skipping the function
			// we don't count directly to the line variable because
			// if the function is ever closed
			// then the error will tell the user the line number of where the function
			// was declared rather than just saying what the last line number is
			do {
				i++;
				if (src[i] == '{')
					nestCount++;
				if (src[i] == '}')
					nestCount--;
				if (src[i] == '\n')
					extraLines++;
				if (i >= len)
					EXCEPTION("function never closed\n");
			} while (nestCount);
			// add up the line counts
			line += extraLines;
		} else if (src[i] == ';') {			// function call
			fnCalls++;						// increase fnCalls counter
			int labelLen = strlen(label);	// label length
			int labelExists = 0;			// boolean to check if the label exists
			u_int go_to = 0;				// the index to go to

			// get the label
			for (struct LABEL *x = labels; x != NULL; x = x->next) {
				if (x->name[0] == label[0])
					if (labelLen == strlen(x->name))
						if (eq(x->name, label, labelLen)) {
							go_to = x->gto;
							labelExists = 1;
							break;
						}
			}
			// if the label wasn't found then yell at the user
			if (!labelExists)
				EXCEPTION("reference of undeclared label \"%s\"\n", label);
			
			// reset the label variable
			for (int j = 32; j; --j)
				label[j] = 0;
			scope++;			// increase the scope counter
			run(go_to + 1, len, src, NULL);		// run the function
			scope--;		// decrease the scope counter
		} else if (src[i] == '}') {
			if (scope < 1)
				WARN("unexpected '}'");
			return 0;
		}
		
		tp %= tSize;
		if (tp < 0)
			tp += tSize;
	}
	if (scope)
		return 1;
	return 0;
}
