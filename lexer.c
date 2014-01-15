#include "lexer.h"

#include <stdio.h>
#include <ctype.h>

#define KEYWORD_COUNT    9

typedef struct Keyword {
	int      size;
	char*    id;
	Token    token;
} Keyword; 

static const Keyword keywords[KEYWORD_COUNT] = {
	{ 3, "end", TK_END },
	{ 3, "var", TK_VAR },
	{ 2, "do", TK_DO },
	{ 4, "done", TK_DONE },
	{ 2, "to", TK_TO },
	{ 3, "for", TK_FOR },
	{ 6, "return", TK_RETURN },
	{ 5, "begin", TK_BEGIN },
	{ 9, "procedure", TK_PROCEDURE }
};

static inline Token lookup_keyword(char *id, int size) {
	int i;
	for (i=0; i<KEYWORD_COUNT; i++)
		if  ((keywords[i].size == size) && (strncmp(id, keywords[i].id, size) == 0))
			return keywords[i].token;
	return TK_UNKNOWN;
}

static inline void append_char(Lexer *l, int c) {
	if (c == '\n') {
		l->location.until.line++;
		l->location.until.column = 1;
	} else
		l->location.until.column++;

	if (l->lexeme_size < MAX_LEXEME_SIZE)
		l->lexeme[l->lexeme_size] = c;
	l->lexeme_size++;
}

static inline int peek_char(Lexer *l) {
	if (l->last_char == EOF)
		l->last_char = l->input_fun(l->user_data);
	return l->last_char;
}

static inline void next_char(Lexer *l) {
	if (l->last_char != EOF)
		append_char(l, l->last_char);
	l->last_char = EOF;
}

static inline void do_lex(Lexer *l) {
	int c;
start:
	l->lexeme_size = 0;
	l->location.from = l->location.until;
	c = peek_char(l);
	next_char(l);
	if (c == EOF) {
		l->token = TK_EOF;
	} else if (isalpha(c) || (c == '_')) {
		c = peek_char(l);
		while (isalpha(c) || (c == '_')) {
			next_char(l);
			c = peek_char(l);
		}
		l->token = lookup_keyword(l->lexeme, l->lexeme_size);
		if (l->token == TK_UNKNOWN)
			l->token = TK_ID;
	} else if (isdigit(c)) {
		c = peek_char(l);
		while (isdigit(c)) {
			next_char(l);
			c = peek_char(l);
		}
		l->token = TK_NUM;
	} else if (isspace(c)) {
		c = peek_char(l);
		while (isspace(c)) {
			next_char(l);
			c = peek_char(l);
		}
		goto start;
	} else if (c == ';') {
		l->token = TK_SEMI;
	} else if (c == '(') {
		l->token = TK_LPAREN;
	} else if (c == ')') {
		l->token = TK_RPAREN;
	} else if (c == ':') {
		c = peek_char(l);
		if (c == '=') {
			next_char(l);
			l->token = TK_ASSIGN;
		} else
			l->token = TK_COLON;
	} else if (c == ',') {
		l->token = TK_COMMA;
	} else if (c == '*') {
		l->token = TK_MULT;
	} else if (c == '+') {
		l->token = TK_ADD;
	} else {
		l->token = TK_UNKNOWN;
	}
}

void lex(Lexer *l) {
	do_lex(l);
	l->lexeme[ (l->lexeme_size <= MAX_LEXEME_SIZE) ? l->lexeme_size : MAX_LEXEME_SIZE ] = '\0';
}

void init_lexer(Lexer *lexer, GetChar input_fun, void *user_data) {
	lexer->user_data = user_data;
	lexer->input_fun = input_fun;
	lexer->lexeme_size = 0;
	lexer->last_char = EOF;
	lexer->location.until.line = 1;
	lexer->location.until.column = 1;
	lexer->token = TK_UNKNOWN;
}
