
#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>
// #include "program.h" // do not include! circular!
#include "datatypes.h"

typedef enum{
	TOKEN_END,

	// operators
	TOKEN_ADD,
	TOKEN_SUB,
	TOKEN_MUL,
	TOKEN_DIV,
	TOKEN_MOD,

	TOKEN_LOGICAL_AND,
	TOKEN_LOGICAL_OR,

	TOKEN_ADD_ASSIGN,
	TOKEN_SUB_ASSIGN,
	TOKEN_MUL_ASSIGN,
	TOKEN_DIV_ASSIGN,
	TOKEN_MOD_ASSIGN,

	TOKEN_COMP_LT,
	TOKEN_COMP_LTE,
	TOKEN_COMP_GT,
	TOKEN_COMP_GTE,
	TOKEN_COMP_EQ,
	TOKEN_COMP_NEQ,
	
	TOKEN_DOT,
	TOKEN_REF,

	TOKEN_LPAREN,
	TOKEN_RPAREN,

	TOKEN_LBRACE,
	TOKEN_RBRACE,

	TOKEN_LBRACKET,
	TOKEN_RBRACKET,

	TOKEN_FOR,
	TOKEN_WHILE,
	TOKEN_IN,
	TOKEN_BREAK,
	TOKEN_CONTINUE,

	TOKEN_IF,
	TOKEN_ELSE,

	TOKEN_ASSIGN,
	TOKEN_FUNC,
	TOKEN_COROUTINE,
	
	TOKEN_DECLARE,
	
	TOKEN_CALL_ARGS,
	TOKEN_CALL, 
	TOKEN_RETURN,
	TOKEN_YIELD,
	TOKEN_DELAY,
	TOKEN_RATE,
	TOKEN_PUBLISH,

	TOKEN_IDENT,
	TOKEN_LVALUE,

	TOKEN_NUMBER,
	TOKEN_BOOL,
	TOKEN_STRING,
	TOKEN_OBJECT,
	
	TOKEN_SEMICOLON,
	TOKEN_COMMA,

	TOKEN_ASSERT,

	TOKEN_HALT,

	TOKEN_NOP,
	TOKEN_ARRAY_LOAD,
	TOKEN_ARRAY_INDEX,
	
	TOKEN_TYPE_COUNT,
} token_type_t;


typedef struct{
	token_type_t type;
	char *str;
	uint16_t str_len;
	uint16_t line_num;
	data_type_t data_type;
	union{
		int32_t i32;
		fixed16_t f16;
	} number;
} token_t;

typedef struct{
	const char* fname;
	const char* input;
	uint32_t input_len;
	uint32_t position;
	uint16_t current_line_number;
} lexer_t;


void print_token(token_t* token);

token_t get_token(lexer_t *lexer);
token_t peek_current_token(lexer_t *lexer);
token_t peek_next_token(lexer_t *lexer);

#endif

