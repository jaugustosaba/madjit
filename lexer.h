#ifndef LEXER_H
#define LEXER_H

#define MAX_LEXEME_SIZE  20

typedef enum Token {
	TK_UNKNOWN,
	TK_SEMI,
	TK_LPAREN,
	TK_RPAREN,
	TK_COLON,
	TK_COMMA,
	TK_ASSIGN,
	TK_MULT,
	TK_ADD,
	TK_PROCEDURE,
	TK_VAR,
	TK_BEGIN,
	TK_END,
	TK_FOR,
	TK_DO,
	TK_DONE,
	TK_TO,
	TK_RETURN,
	TK_ID,
	TK_NUM,
	TK_EOF
} Token;

typedef struct Position {
	int line;
	int column;
} Position;

typedef struct Location {
	Position  from;
	Position  until;
} Location;

typedef int (*GetChar)(void *user_data);

typedef struct Lexer {
	void*      user_data;
	GetChar    input_fun;
	char       lexeme[MAX_LEXEME_SIZE+1];
	int        lexeme_size;
	int        last_char;
	Location   location;
	Token      token;
} Lexer;

void lex(Lexer *l);

void init_lexer(Lexer *lexer, GetChar input_fun, void *user_data);

#endif