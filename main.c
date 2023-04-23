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
#define LOG(x...)			{if (echo) printf(x); if (logMode) fprintf(DEBUG_STREAM, x);}
#define ERR(x...)			{LOG( "bfn err: " x); errs++;}
#define EXCEPTION(x...)		{LOG("bfn exception at line %d: ", line + 1); LOG(x); fclose(DEBUG_STREAM); exit(0);}
#define WARN(x...)			{LOG("\nbfn warning at line %d: ", line); LOG(x);}
#define DEBUG(x...)			{if (debugMode) LOG(x);}
#define IS_ABCNUM(c)		((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))

struct index_stack {
	u_int index;
	struct index_stack *next;
};
struct LABEL {
	char name[33];
	struct index_stack *gto;
	struct LABEL *next;
};
// used for passing arguments to functions
struct stack *arg;
// used for returning values from functions
struct stack *ret;
struct index_stack *loopStack;
struct LABEL *labels;
FILE *DEBUG_STREAM;
int debugMode = 0;
int logMode = 0;
int echo = 1;

char *src;
u_int len = 0;
u_int tSize = 65536;
u_int fnCalls = 0;			// the amount of times a function has been called
u_int scope = 0;			// how deep we are in scope

// helper function that runs a bfn script
struct stack *bfn_runFile(char *filename, struct stack *args);
// helper function that checks if two strings are equal
int eq(char a[], char b[], int len);
// helper function that checks what line a given index is on in a string
u_int getLine(u_int index, char *str);
// the function that interprets the bfn code
struct stack *run(u_int index, u_int len, char *src, struct stack *args);

// main function, sets things up
int main(int argc, char **argv) {
	CLEAR;
	u_int errs = 0;
	DEBUG_STREAM = stdout;
	
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
		for (u_int i = 3; i < argc; ++i) {
			if (!strcmp("-log", argv[i])) {
				logMode = 1;
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
			if (!strcmp("-echo", argv[i])) {
				if (argc == i + 1) {
					ERR("expected \"on\" or \"off\" after \"-log\"\n");
				}
				else {
					if (!strcmp("on", argv[i + 1]))
						echo = 1;
					else if (!strcmp("off", argv[i + 1]))
						echo = 0;
					else {
						ERR("expected \"on\" or \"off\" after \"-log\"\n");
					}
				}
				i++;
			}
		}
	}
	// if the tape is too small then the program will most likely
			// encounter a segfault when trying to edit the tape
	if (tSize < 3)
		ERR("tape size too small\n");
	// exit if there are any errors
	if (errs) {
		exit(0);
		fclose(DEBUG_STREAM);
	}
	
	src = calloc(len + 1, sizeof(char));
	
	bfn_runFile(argv[1], NULL);

	LOG("program terminated naturally with %d function calls in total\n", fnCalls);
	fclose(DEBUG_STREAM);
	return 0;
}

// helper function that runs a bfn script
struct stack *bfn_runFile(char *filename, struct stack *args) {
	u_int errs = 0;
	// open the script
	FILE *fp;
	fp = fopen(filename, "r");
	u_int xlen = 0;
	struct stack *x;
	if (fp == NULL)	{	// if we cant access the file or it doesnt exist
		ERR("couldn't find file %s\n", filename);
		fclose(DEBUG_STREAM);
		exit(0);
	}

	// initialize the label array (used for labels (duh))
	labels = calloc(1, sizeof(struct LABEL));
	// initiallize the loop stack
	loopStack = calloc(1, sizeof(struct index_stack));

	// read the file contents into a string
	char *str;
	fseek(fp, 0, SEEK_END);
	xlen = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	str = calloc(xlen, sizeof(char));
	if (str) {
		fread(str, 1, xlen, fp);
	} else {
		ERR("script is empty\n");
		fclose(fp);
		x = calloc(1, sizeof(struct stack));
		return x;
	}
	printf(".\n");
	fclose(fp);
	printf(".\n");

	strcat(src, str);
	// run the script
	x = run(len, len + xlen, src, args);

	len += xlen;
	
	return x;
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
#define DO_CONCAT(i, line)		concat(i, src, len, line, locals)
// helper function that concatenates functions together
struct index_stack *concat(u_int *i, char *src, u_int len, u_int *lines, struct LABEL *locals) {
	struct index_stack *ret;
	struct index_stack *x;
	ret = calloc(1, sizeof(struct index_stack));
	x = ret;
	u_int extraLines = 0;
	while (src[*i] != ')') {
		++*i;
		if (*i >= len) {
			u_int line = *lines;
			EXCEPTION("concatenation never closed");
		}
		char ch = src[*i];
		DEBUG("%d concat %c\n", *i, ch);
		if (ch == '/') {
			++*i;
			if (1 + *i >= len) {
				u_int line = *lines;
				EXCEPTION("concatenation never closed");
			}
			ch = src[*i];
			if (ch == '-') {
				DEBUG("%d concat comment\n", *i);
				u_int extraExtraLines = 0;
				while (1) {
					++*i;
					if (1 + *i >= len) {
						u_int line = extraLines + *lines;
						WARN("(in concat) comment never closed");
						exit(0);
					}
					if (ch == '\n')
						extraExtraLines++;
					else if (ch == '-' && src[1 + *i] == '/')
						break;
				}
				extraLines += extraExtraLines;
			}
		}
		else if (ch == '\n')
			extraLines++;
		else if (ch == '(') {			// meta concatenation
			struct index_stack *y = DO_CONCAT(i, lines);
			struct index_stack *z = y;
			x->next = y;
			while (z->next != NULL)
				z = z->next;
			x = z;
		} else if (ch == '\'') {		// label concatenation
			int j = 0;
			int stop = 0;
			int local = 0;
			char label[33];
			++*i;
			if (src[*i] == ':') {
				local = 1;
				++*i;
			} else
				local = 0;
			while (*i < len && stop == 0) {
				ch = src[*i];
				label[j++] = ch;
				++*i;
				if (j > 31) {
					u_int line = *lines + extraLines;
					EXCEPTION("(in concat) label reference too long\n");
				}
				if (ch == '\'') {
					label[j - 1] = 0;
					break;
				}
				else if (!IS_ABCNUM(ch) && ch != '_') {
					u_int line = *lines + extraLines;
					if (ch == '\n') {
						EXCEPTION("(in concat) illegal label character '\\n'\n")
					}
					else if (ch == '\t') {
						EXCEPTION("(in concat) illegal label character '\\t'\n")
					}
					else if (ch == '\b') {
						EXCEPTION("(in concat) illegal label character '\\b'\n")
					}
					else if (ch == '\0') {
						EXCEPTION("(in concat) illegal label character '\\0'\n")
					}
					else {
						EXCEPTION("(in concat) illegal label character '%c'\n", ch)
					}
				}
			}
			// some debug code that prints the label
			DEBUG("%d concat \"%s\"\n", *i, label);
			
			int labelLen = strlen(label);	// label length
			int labelExists = 0;
			struct index_stack *y;		// where we store the goto
			// get the label
			for (struct LABEL *j = local? locals: labels; j != NULL; j = j->next) {
				DEBUG("%d concat \"%s\" == \"%s\"? ", *i, label, j->name);
				if (j->name[0] == label[0])
					if (labelLen == strlen(j->name))
						if (eq(j->name, label, labelLen)) {
							y = j->gto;
							labelExists = 1;
							break;
						}
				DEBUG("no\n");
			}
			if (labelExists) DEBUG("yes\n");
			// if the label wasn't found then yell at the user
			if (!labelExists) {
				u_int line = *lines + extraLines;
				if (local) {
					EXCEPTION("(in concat) reference of undeclared local \"%s\"\n", label);
				} else {
					EXCEPTION("(in concat) reference of undeclared label \"%s\"\n", label);
				}
			}
			
			struct index_stack *z = y;
			x->next = y;
			while (z->next != NULL)
				z = z->next;
			x = z;
		} else if (ch == '{') {		// anonymous function concatenation
			// create the label
			struct index_stack *s = calloc(1, sizeof(struct index_stack));
			s->index = *i;
			x->next = s;	// append the label
			x = s;
			// we don't want to run anything inside the function
					// until it is called
					// so we skip until the function is closed
			int nestCount = 1;				// used to count the number of curly brackets
			int extraExtraLines = 0;		// counts the amount the line breaks are encountered while skipping the function
			// we don't count directly to the line variable because
			// if the function is ever closed
			// then the error will tell the user the line number of where the function
			// was declared rather than just saying what the last line number is
			do {
				++*i;
				if (*i >= len) {
					u_int line = *lines + extraLines;
					EXCEPTION("(in concat) function never closed\n");
				}
				ch = src[*i];
				if (ch == '{')
					nestCount++;
				else if (ch == '}')
					nestCount--;
				else if (ch == '\n')
					extraExtraLines++;
			} while (nestCount);
			// add up the line counts
			extraLines += extraExtraLines;
		}
	}
	*lines += extraLines;
	return ret->next;
}
// the function that interprets the bfn code
struct stack *run(u_int index, u_int len, char *src, struct stack *args) {
	struct stack *arg = new_stack();
	struct stack *ret = new_stack();
	u_int errs = 0;
	char *tape = calloc(tSize, sizeof(char));
	u_int tp = 0;
	u_int line = getLine(index, src);
	struct LABEL *locals = calloc(1, sizeof(struct LABEL));
	// label variable is used to store
	// the name of a label
	// before it is used by the program
	char label[33];
	int local = 0;
	LOG("length of script: %d, indexing from: %d\n", len, index);
	for (u_int i = index; i < len; i++) {
		DEBUG("%d %c\n", i, src[i]);
		
		if (src[i] == '/') {
			++i;
			if (1 + i >= len) {
				if (scope) {
					ERR("function never cloesed\n")
				} else
					exit(0);
			}
			char ch = src[i];
			if (ch == '-') {
				DEBUG("%d comment\n", i);
				u_int extraLines = 0;
				while (1) {
					++i;
					if (1 + i >= len) {
						WARN("comment never closed");
						exit(0);
					}
					if (ch == '\n')
						extraLines++;
					else if (ch == '-' && src[1 + i] == '/')
						break;
				}
				line += extraLines;
			}
		}
		else if (src[i] == '\n')
			line++;
		else if (src[i] == '~') {
			// char *filename;
			// filename = malloc(len * sizeof(char));
			// int j = 0;
			while (src[i] != '\n') {
				i++;
			// 	if (i >= len) {
			// 		break;
			// 	}
			// 	filename[j++] = src[i];
			}
			// filename[--j] = 0;
			// ret = bfn_runFile(filename, args);
			// free(filename);
			WARN("script calls are currently on hold until the bfn validator is finished.\n");
		} else if (src[i] == '=')		// reset pointer value
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
			push(arg, tape[tp]);
		else if (src[i] == '&')		// push to return stack
			push(ret, tape[tp]);
		else if (src[i] == '%')		// pop from input stack
			tape[tp] = pop(args);
		else if (src[i] == ':')		// pop from return stack
			tape[tp] = pop(ret);
		else if (src[i] == '[') {	// while loop
			struct index_stack *x = calloc(1, sizeof(struct stack));
			x->next = loopStack;
			x->index = i;
			loopStack = x;
		} else if (src[i] == ']') {
			if (loopStack->next == NULL) {
				WARN("unexpected ']'\n");
				return 0;
			} else if (tape[tp])
				i = loopStack->index;
			else {
				loopStack->index = loopStack->next->index;
				struct index_stack *y = loopStack->next->next;
				free(loopStack->next);
				loopStack->next = y;
			}
		} else if (src[i] == '\'') {
			int j = 0;
			int stop = 0;
			i++;
			if (src[i] == ':') {
				local = 1;
				i++;
			} else
				local = 0;
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
			DEBUG("%d \"", i);
			if (local)
				DEBUG(":");
			DEBUG("%s\"\n", label);
		} else if (src[i] == '{') {		// function declaration
			// create the label
			struct LABEL *l = calloc(1, sizeof(struct LABEL));
			struct index_stack *s = calloc(1, sizeof(struct index_stack));
			s->index = i; 
			l->gto = s;		// set the position to go to when called
			for (int j = 0; j < 32; ++j) {	// set the name of the label
				l->name[j] = label[j];
				label[j] = 0;
			}
			struct LABEL *x = local? locals: labels;
			DEBUG("%d new func \"%s\"\n", i, l->name);
			
			while (x->next != NULL && strcmp(l->name, x->next->name))	// find where we're going to store our label
				x = x->next;
			if (x->next == NULL)
				l->next = NULL;
			else
				l->next = x->next->next;
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
			int labelExists = 0;
			struct LABEL *obj = NULL;		// where we store the label
			// get the label
			for (struct LABEL *x = local? locals: labels; x != NULL; x = x->next) {
				if (x->name[0] == label[0])
					if (labelLen == strlen(x->name))
						if (eq(x->name, label, labelLen)) {
							obj = x;
							labelExists = 1;
							break;
						}
			}
			
			// if the label wasn't found then yell at the user
			if (!labelExists) {
				if (local) {
					EXCEPTION("reference of undeclared local \"%s\"\n", label);
				} else {
					EXCEPTION("reference of undeclared label \"%s\"\n", label);
				}
			}
			
			// reset the label variable
			for (int j = 32; j; --j)
				label[j] = 0;
			scope++;			// increase the scope counter
			struct index_stack *x = obj->gto;
			do {
				arg = run(x->index + 1, len, src, arg);	// run the function
				x = x->next;
			} while (x != NULL);
			ret = arg;
			scope--;			// decrease the scope counter
		} else if (src[i] == '}') {
			if (scope < 1)
				WARN("unexpected '}'");
			return ret;
		} else if (src[i] == '(') {
			// create the label
			struct LABEL *l = calloc(1, sizeof(struct LABEL));
			l->gto = DO_CONCAT(&i, &line);		// set the position to go to when called
			for (int j = 0; j < 32; ++j) {	// set the name of the label
				l->name[j] = label[j];
				label[j] = 0;
			}
			struct LABEL *x = local? locals: labels;
			while (x->next != NULL && strcmp(x->next->name, l->name))	// find where we're going to store our label
				x = x->next;
			if (x->next == NULL)
				l->next = NULL;
			else
				l->next = x->next->next;
			x->next = l;
			
			// reset the label variable
			for (int j = 32; j; --j)
				label[j] = 0;
		}
		
		tp %= tSize;
		if (tp < 0)
			tp += tSize;
	}
	return ret;
}
