// brainFn-interpreter/stacks.h
//   custom library that implements char stacks in C
// author    Dante Davis
// from      2023 April 20th
// to        2023 April 22nd
// GNU GPL 3.0 COPYLEFT LICENSE
// i apologize for the rather disgusting mess that is this code.
#include <stdlib.h>

struct stack {
	char val;
	struct stack *next;
};

struct stack *new_stack();
char pop(struct stack *s);
void push(struct stack *s, char v);
unsigned long long stack_len(struct stack *s);

struct stack *new_stack() {
	struct stack *x = calloc (1, sizeof(struct stack));
	return x;
}
char pop(struct stack *s) {
	if (s->next == NULL)
		return 0;
	char x = s->val;
	s->val = s->next->val;
	struct stack *y = s->next->next;
	free(s->next);
	s->next = y;
	return x;
}
void push(struct stack *s, char v) {
	struct stack *x = calloc(1, sizeof(struct stack));
	x->val = s->val;
	x->next = s->next;
	s->next = x;
	s->val = v;
}
unsigned long long stack_len(struct stack *s) {
	struct stack x = *s;
	unsigned long long len = 0;
	while (x.next != NULL) {
		x = *x.next;
		len++;
	}
	return len;
}
