#include <string.h>

#define u_int				unsigned int
#define RED					"\033[31m"
#define IS_ABCNUM(c)		((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))

#define INVALID(Code)		{e.invalid = 1; e.code = Code; return e;}
#define OK()				{e.invalid = 0; e.code = 0; return e;}

#define SCRIPT				1
#define EMPTY_SCRIPT		2
#define RECURSIVE_CALL		3
#define SYNTAX				4


struct str {
	char *val;
	struct str *next;
};
struct debug_spec {
	int code;
	u_int line;
	u_int sqrBrackets;
	u_int sqrLine;
	u_int crlBrackets;
	u_int crlLine;
	u_int crvBrackets;
	u_int crvLine;
	u_int invalid: 2;
	u_int quote: 2;
	u_int invalid_char: 2;
	u_int too_long: 2;
};

struct str *filescope;
struct debug_spec err;

int append(char *s);
int scope_pop();
int inScope(char *s);

struct debug_spec fileValidate(char *filename);
struct debug_spec validate(char *src, u_int len);

int append(char *s) {
	int len = strlen(s);
	char *x = malloc(len * sizeof(char));
	for (int i = 0; i < len; ++i) {
		x[i] = s[i];
	}
	x[len] = 0;
	struct str *b = malloc(sizeof(struct str));
	b->next = filescope;
	b->val = x;
	filescope = b;
	return 0;
}
int scope_pop() {
	struct str *a = filescope;
	filescope = filescope->next;
	free(a);
	return 0;
}
int inScope(char *s) {
	int len = strlen(s);
	for (struct str *x = filescope; x->next != NULL; x = x->next)
		if (x->val[0] == s[0]			// check the first character
			&& strlen(x->val) == len 	// check the lengths
			&& !strcmp(x->val, s))		// compare them
					return 1;
	return 0;
}

struct debug_spec fileValidate(char *filename) {
	struct debug_spec e;
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
		INVALID(SCRIPT);
	if (inScope(filename))
		INVALID(RECURSIVE_CALL);
	u_int len;
	char *str;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	str = calloc(len, sizeof(char));
	if (str) {
		fread(str, 1, len, fp);
	} else {
		fclose(fp);
		INVALID(EMPTY_SCRIPT);
	}
	fclose(fp);
	append(filename);
	e = validate(str, len);
	if (!e.invalid)
		scope_pop();
	return e;
}

struct debug_spec validate(char *src, u_int len) {
	struct debug_spec e = {0};
	e.line = 1;
	for (u_int i = 0; i < len; i++) {
		if (src[i] == '\n')
			e.line++;
		else if (src[i] == '~') {
			char *filename;
			filename = malloc(len * sizeof(char));
			int j = 0;
			while (src[i] != '\n') {
				i++;
				if (i >= len) {
					break;
				}
				filename[j++] = src[i];
			}
			filename[--j] = 0;
			struct debug_spec invalid = fileValidate(filename);
			if (invalid.code)
				return invalid;
			free(filename);
			e.line++;
		}
		else if (src[i] == '[') {
			e.sqrBrackets++;
			e.sqrLine = e.line;
		}
		else if (src[i] == ']')
			e.sqrBrackets--;
		else if (src[i] == '(') {
			e.crvBrackets++;
			e.crvLine = e.line;
		}
		else if (src[i] == ')')
			e.crvBrackets--;
		else if (src[i] == '{') {
			e.crlBrackets++;
			e.crlLine = e.line;
		}
		else if (src[i] == '}')
			e.crlBrackets--;
		else if (src[i] == '\'') {
			e.quote = 1;
			i++;
			u_int j = 0;
			if (src[i] == ':')
				i++;
			while (1) {
				i++;
				j++;
				if (j > 32) {
					e.too_long = 1;
					INVALID(SYNTAX);
				}
				if (i > len)
					INVALID(SYNTAX);
				if (src[i] == '\'')
					break;
				if (!IS_ABCNUM(src[i]) && src[i] == '_') {
					e.invalid_char = 1;
					INVALID(SYNTAX);
				}
			}
			e.quote = 0;
		}
		else if (src[i] == '/') {
			++i;
			if (1 + i >= len) {
				break;
			}
			char ch = src[i];
			if (ch == '-') {
				u_int extraLines = 0;
				while (1) {
					++i;
					if (1 + i >= len)
						INVALID(SYNTAX);
					if (ch == '\n')
						extraLines++;
					else if (ch == '-' && src[1 + i] == '/')
						break;
				}
				e.line += extraLines;
			}
		}
	}
	if (e.quote || e.sqrBrackets || e.crvBrackets || e.crlBrackets)
		INVALID(SYNTAX);
	OK();
}

extern int isInvalid(char *filename) {
	filescope = malloc(sizeof(struct str));
	filescope->next = NULL;
	filescope->val = NULL;
	err = fileValidate(filename);
	return err.code;
}
extern int genErrMsg() {
	printf(RED "     in ");
	struct str *x = filescope;
	printf(" %s", x->val);
	if (x->next != NULL)
		x = x->next;
	for (; x->next != NULL; x = x->next)
		printf("\n   (via  %s)", x->val);
	if (err.code == 3) {
		printf("\nat line %u: ", err.line);
		printf("recursive script call\n");
	}
	if (err.code == 4) {
		if (err.sqrBrackets)
			printf("\nat line %u: while loop never closed", err.sqrLine);
		if (err.crvBrackets)
			printf("\nat line %u: concatenation never closed", err.crvLine);
		if (err.crlBrackets)
			printf("\nat line %u: function never closed", err.crlLine);
		if (err.invalid_char)
			printf("\nat line %u: invalid label character", err.line);
		else if (err.too_long)
			printf("\nat line %u: label too long", err.line);
		else if (err.quote)
			printf("\n\tquotes never closed");
		printf("\n");
	}
	while (filescope->next != NULL)
		scope_pop();
	return 0;
}
