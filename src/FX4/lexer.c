
#ifdef FX4_COMPILER

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lexer.h"
#include "program.h"
#include "bytecode.h"
#include "hash32.h"
#include "debug_printf.h"


static bool print_tokens = false;


void print_token(token_t* token){

	if(token == NULL){

		printf("NULL TOKEN\n");
		exit(EXIT_FAILURE);
	}

	debug_printf("Token %2d @ %3d:  ", token->type, token->line_num);

	// NOTE!  We cannnot use the %.*s in printf for non-null terminated strings on the ESP32, it will crash.

	char s[256] = {0};
	if((token->str != NULL) && (token->str_len > 0)){
	
		memcpy(s, token->str, token->str_len);
		s[token->str_len] = 0;
	}

	if(token->type == TOKEN_IDENT){

		debug_printf("Ident: [%s] Len: %d\n", s, token->str_len);
	}
	else if(token->type == TOKEN_LVALUE){

		debug_printf("Lvalue: [%s] Len: %d\n", s, token->str_len);
	}
	else if(token->type == TOKEN_BOOL){

		debug_printf("Bool: [%s] Len: %d\n", s, token->str_len);
	}
	else if(token->type == TOKEN_NUMBER){

		debug_printf("Number: [%s] = %d\n", s, token->number.i32);
	}
	else if(token->type == TOKEN_STRING){

		debug_printf("String: [%s] Len: %d\n", s, token->str_len);
	}
	else if(token->type == TOKEN_OBJECT){

		debug_printf("Object: [%s] Len: %d\n", s, token->str_len);
	}
	else if(token->type == TOKEN_SEMICOLON){

		debug_printf("SEMICOLON\n");
	}
	else if(token->type == TOKEN_LPAREN){

		debug_printf("LPAREN\n");
	}
	else if(token->type == TOKEN_RPAREN){

		debug_printf("RPAREN\n");
	}
	else if(token->type == TOKEN_LBRACE){

		debug_printf("LBRACE\n");
	}
	else if(token->type == TOKEN_RBRACE){

		debug_printf("RBRACE\n");
	}
	else if(token->type == TOKEN_LBRACKET){

		debug_printf("LBRACKET\n");
	}
	else if(token->type == TOKEN_RBRACKET){

		debug_printf("RBRACKET\n");
	}
	else if(token->type == TOKEN_DOT){

		debug_printf("DOT\n");
	}
	else if(token->type == TOKEN_REF){

		debug_printf("REF\n");
	}
	else if(token->type == TOKEN_FOR){

		debug_printf("FOR\n");
	}
	else if(token->type == TOKEN_WHILE){

		debug_printf("WHILE\n");
	}
	else if(token->type == TOKEN_IN){

		debug_printf("IN\n");
	}
	else if(token->type == TOKEN_BREAK){

		debug_printf("BREAK\n");
	}
	else if(token->type == TOKEN_CONTINUE){

		debug_printf("CONTINUE\n");
	}
	else if(token->type == TOKEN_IF){

		debug_printf("IF\n");
	}
	else if(token->type == TOKEN_ELSE){

		debug_printf("ELSE\n");
	}
	else if(token->type == TOKEN_ADD){

		debug_printf("ADD\n");
	}
	else if(token->type == TOKEN_SUB){

		debug_printf("SUB\n");
	}
	else if(token->type == TOKEN_MUL){

		debug_printf("MUL\n");
	}
	else if(token->type == TOKEN_DIV){

		debug_printf("DIV\n");
	}
	else if(token->type == TOKEN_MOD){

		debug_printf("MOD\n");
	}
	else if(token->type == TOKEN_LOGICAL_AND){

		debug_printf("AND\n");
	}
	else if(token->type == TOKEN_LOGICAL_OR){

		debug_printf("OR\n");
	}
	else if(token->type == TOKEN_ADD_ASSIGN){

		debug_printf("ADD_ASSIGN\n");
	}
	else if(token->type == TOKEN_SUB_ASSIGN){

		debug_printf("SUB_ASSIGN\n");
	}
	else if(token->type == TOKEN_MUL_ASSIGN){

		debug_printf("MUL_ASSIGN\n");
	}
	else if(token->type == TOKEN_DIV_ASSIGN){

		debug_printf("DIV_ASSIGN\n");
	}
	else if(token->type == TOKEN_MOD_ASSIGN){

		debug_printf("MOD_ASSIGN\n");
	}
	else if(token->type == TOKEN_COMP_LT){

		debug_printf("COMP_LT\n");
	}
	else if(token->type == TOKEN_COMP_LTE){

		debug_printf("COMP_LTE\n");
	}
	else if(token->type == TOKEN_COMP_GT){

		debug_printf("COMP_GT\n");
	}
	else if(token->type == TOKEN_COMP_GTE){

		debug_printf("COMP_GTE\n");
	}
	else if(token->type == TOKEN_COMP_EQ){

		debug_printf("COMP_EQ\n");
	}
	else if(token->type == TOKEN_COMP_NEQ){

		debug_printf("COMP_NEQ\n");
	}
	else if(token->type == TOKEN_ASSIGN){

		debug_printf("ASSIGN\n");
	}
	else if(token->type == TOKEN_DECLARE){

		debug_printf("DECLARE\n");
	}
	else if(token->type == TOKEN_ASSERT){

		debug_printf("ASSERT\n");
	}
	else if(token->type == TOKEN_CALL){

		debug_printf("CALL\n");
	}
	else if(token->type == TOKEN_CALL_ARGS){

		debug_printf("CALL_ARGS\n");
	}
	else if(token->type == TOKEN_RETURN){

		debug_printf("RETURN\n");
	}
	else if(token->type == TOKEN_YIELD){

		debug_printf("YIELD\n");
	}
	else if(token->type == TOKEN_DELAY){

		debug_printf("DELAY\n");
	}
	else if(token->type == TOKEN_RATE){

		debug_printf("RATE\n");
	}
	else if(token->type == TOKEN_PUBLISH){

		debug_printf("PUBLISH\n");
	}
	else if(token->type == TOKEN_FUNC){

		debug_printf("FUNC\n");
	}
	else if(token->type == TOKEN_COROUTINE){

		debug_printf("COROUTINE\n");
	}
	else if(token->type == TOKEN_END){

		debug_printf("END\n");
	}
	else if(token->type == TOKEN_HALT){

		debug_printf("HALT\n");
	}
	else if(token->type == TOKEN_COMMA){

		debug_printf("COMMA\n");
	}
	else if(token->type == TOKEN_NOP){

		debug_printf("NOP\n");
	}
	else if(token->type == TOKEN_ARRAY_LOAD){

		debug_printf("ARRAY_LOAD\n");
	}
	else if(token->type == TOKEN_ARRAY_INDEX){

		debug_printf("ARRAY_INDEX\n");
	}
	else{

		debug_printf("Unknown: %d\n", token->type);
	}
}

bool is_end(lexer_t *lexer){

	return lexer->position >= lexer->input_len;
}


char peek(lexer_t *lexer){

	if(is_end(lexer)){

		return '\0';
	}

	char c = lexer->input[lexer->position];

	return c;
}


char peek_next(lexer_t *lexer){

	if(is_end(lexer)){

		return '\0';
	}

	char c = lexer->input[lexer->position + 1];

	if((lexer->position + 1) >= lexer->input_len){

		return '\0';
	}

	return c;
}


char advance(lexer_t *lexer){

	char c = lexer->input[lexer->position];

	if(c == '\n'){

		lexer->current_line_number++;
	}

	lexer->position++;

	return c;
}

bool is_digit(char c){

	if((c >= '0') && (c <= '9')){

		return true;
	}

	return false;
}

bool is_name_body(char c){

	if(((c >= 'A') && (c <= 'Z')) ||
	   ((c >= 'a') && (c <= 'z')) ||
	   is_digit(c) ||
	   (c == '_')){

		return true;
	}

	return false;
}

bool is_name(char c){

	return is_name_body(c);

	// if(((c >= 'A') && (c <= 'Z')) ||
	//    ((c >= 'a') && (c <= 'z')) ||
	//    is_digit(c)){

	// 	return true;
	// }

	// return false;
}

const char* current_position(lexer_t *lexer){

	return &lexer->input[lexer->position];
}

bool char_in_str(char c, const char *str, int len){

	while(len > 0){

		if(*str == c){

			return true;
		}

		str++;
		len--;
	}

	return false;
}


int32_t string_to_int32(const char *str, size_t length){
    // Handle null pointer
    if (str == NULL) {
        return 0;
    }
    
    // Handle zero length
    if (length == 0) {
        return 0;
    }
    
    // Use a static buffer of 32 characters
    char temp_str[32];
    
    // Check if the string is too long for our buffer
    if (length >= sizeof(temp_str)) {
        return 0;  // Return 0 on string too long
    }
    
    // Copy the string and add null terminator
    memcpy(temp_str, str, length);
    temp_str[length] = '\0';
    
    // Convert string to long integer
    char *endptr;
    long converted = strtol(temp_str, &endptr, 10);
    
    // Check for conversion errors (invalid characters)
    if (*endptr != '\0') {

    	printf("Integer conversion error\n");

        return 0;  // Return 0 on conversion error
    }
    
    // Check for overflow/underflow
    if (converted > INT32_MAX || converted < INT32_MIN) {

    	printf("Integer over/underflow\n");

        return 0;  // Return 0 on overflow/underflow
    }
    
    // Return result
    return (int32_t)converted;
}

fixed16_t string_to_fixed16(const char *str, size_t length){

	int16_t integer = 0;
	uint16_t fraction = 0;

	const char *decimal_ptr = NULL;
	int int_len = 0;
	int frac_len = 0;

	// scan to decimal
	for(size_t i = 0; i < length; i++){

		if(str[i] == '.'){

			decimal_ptr = &str[i];

			break;
		}

		int_len++;
	}

	frac_len = (length - int_len) - 1;


	// i64 for extra conversion range
	int64_t i32 = string_to_int32(str, int_len);

	// TODO need to do an error return here - later
	if(i32 > F16_MAX){

		printf("Fixed16 value out of range\n");

		return 0;
	}
	else if(i32 < F16_MIN){

		printf("Fixed16 value out of range\n");

		return 0;
	}

	integer = i32;

	if(decimal_ptr != NULL){

		i32 = string_to_int32(decimal_ptr + 1, frac_len);

		uint64_t base = 1;
		for(int i = 0; i < frac_len; i++){

			base *= 10;
		}

		fraction = (i32 * 65536) / base;

		// adjust fraction down for negative numbers
		if(integer < 0){

			fraction = 65536 - fraction;
			integer--;
		}

		// printf("F16: %.*s %ld i %d f %d %d.%d base: %d\n", length, str, i32, int_len, frac_len, integer, fraction, base);
	}

	fixed16_t final = ((uint32_t)integer << 16) | fraction;

	// printf("F16: %ld\n", final);

	return final;
	// return ((uint32_t)integer * 65535) + fraction;
}


token_t get_token(lexer_t *lexer){

	// get first character
	// char c = peek();

	token_t token = {0};
	token.type = TOKEN_END;
	token.str = (char *)&lexer->input[lexer->position];
	token.line_num = lexer->current_line_number;

	token_t prev_token = {0};

	while(peek(lexer) != '\0'){

		// printf("%d\n", (int)peek());

		token.str = (char *)&lexer->input[lexer->position];
		token.line_num = lexer->current_line_number;
		token.data_type = 0;
		token.number.i32 = 0;

		// comment
		if(peek(lexer) == '#'){

			// advance to EOF or end of line
			while((peek(lexer) != '\n') && (peek(lexer) != '\0')){

				advance(lexer);
			}
		}
		else if((peek(lexer) == '/') && (peek_next(lexer) == '/')){

			// advance to EOF or end of line
			while((peek(lexer) != '\n') && (peek(lexer) != '\0')){

				advance(lexer);
			}

		}
		// negative number
		// if prev token was number or ident, this would be a sub operator.
		// if not, then it is a negative constant
		else if((peek(lexer) == '-') && 
				(is_digit(peek_next(lexer))) &&
				(prev_token.type != TOKEN_NUMBER) &&
				(prev_token.type != TOKEN_IDENT)){

			advance(lexer);

			token.type = TOKEN_NUMBER;
			token.str_len = 1;

			while(is_digit(peek(lexer))){

				token.str_len++;
				advance(lexer);
			}

			bool is_decimal = false;

			// check for decimal
			if((peek(lexer) == '.') && (is_digit(peek_next(lexer)))){

				token.str_len++;
				advance(lexer);

				while(is_digit(peek(lexer))){

					token.str_len++;
					advance(lexer);
				}

				is_decimal = true;
			}
			
			if(is_decimal){

				token.data_type = TYPE_FIXED16;
				token.number.f16 = string_to_fixed16(token.str, token.str_len);
			}
			else{

				token.data_type = TYPE_NUMBER;
				token.number.i32 = string_to_int32(token.str, token.str_len);
			}

			break;
		}
		// number
		else if(is_digit(peek(lexer))){

			token.type = TOKEN_NUMBER;
			token.str_len = 0;

			while(is_digit(peek(lexer))){

				token.str_len++;
				advance(lexer);
			}

			bool is_decimal = false;

			// check for decimal
			if((peek(lexer) == '.') && (is_digit(peek_next(lexer)))){

				token.str_len++;
				advance(lexer);

				while(is_digit(peek(lexer))){

					token.str_len++;
					advance(lexer);
				}

				is_decimal = true;
			}

			if(is_decimal){

				token.data_type = TYPE_FIXED16;
				token.number.f16 = string_to_fixed16(token.str, token.str_len);
			}
			else{

				token.data_type = TYPE_NUMBER;
				token.number.i32 = string_to_int32(token.str, token.str_len);
			}

			break;
		}
		// string
		else if(peek(lexer) == '"'){

			token.type = TOKEN_STRING;
			token.data_type = TYPE_STRING;
			token.str_len = 0;
			advance(lexer);
			token.str++;

			// scan through string
			while(peek(lexer) != '"'){

				// add to string
				token.str_len++;
				advance(lexer);
			}

			advance(lexer); // strip end quote

			break;
		}
		// name
		else if(is_name(peek(lexer))){

			while(is_name_body(peek(lexer))){

				token.str_len++;
				advance(lexer);
			}


			/*

			These need to check input length (to whitespace)
			against keyword length.

			Currently they will greedily accept a partial match.

			IE i will match "in"

			*/
			if((token.str_len == 4) && (strncmp(token.str, "true", token.str_len) == 0)){

				token.type = TOKEN_BOOL;
				token.data_type = TYPE_NUMBER;
				token.number.i32 = 1;
			}
			else if((token.str_len == 5) && (strncmp(token.str, "false", token.str_len) == 0)){

				token.type = TOKEN_BOOL;
				token.data_type = TYPE_NUMBER;
				token.number.i32 = 0;
			}
			else if((token.str_len == 3) && (strncmp(token.str, "and", token.str_len) == 0)){

				token.type = TOKEN_LOGICAL_AND;
				token.data_type = TYPE_NUMBER;
			}
			else if((token.str_len == 2) && (strncmp(token.str, "or", token.str_len) == 0)){

				token.type = TOKEN_LOGICAL_OR;
				token.data_type = TYPE_NUMBER;
			}
			else if((token.str_len == 6) && (strncmp(token.str, "number", token.str_len) == 0)){

				// declare number
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_NUMBER;
			}
			else if((token.str_len == 7) && (strncmp(token.str, "fixed16", token.str_len) == 0)){

				// declare number
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_FIXED16;
			}
			else if((token.str_len == 4) && (strncmp(token.str, "bool", token.str_len) == 0)){

				// declare number
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_NUMBER;
			}
			else if((token.str_len == 6) && (strncmp(token.str, "string", token.str_len) == 0)){

				// declare string
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_STRING;
			}
			else if((token.str_len == 6) && (strncmp(token.str, "object", token.str_len) == 0)){

				// declare string
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_OBJECT_DEF;
			}
			else if((token.str_len == 8) && (strncmp(token.str, "function", token.str_len) == 0)){

				// declare func ref
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_REF_FUNCTION;
			}
			else if((token.str_len == 9) && (strncmp(token.str, "coroutine", token.str_len) == 0)){

				// declare func ref
				token.type = TOKEN_DECLARE;
				token.data_type = TYPE_REF_COROUTINE;
			}

			// else if((token.str_len == 10) && (strncmp(token.str, "pixelarray", token.str_len) == 0)){

			// 	// declare pixelarray ref
			// 	token.type = DECLARE;
			// 	token.data_type = TYPE_REF_PIXELARRAY;
			// }
			else if((token.str_len == 2) && (strncmp(token.str, "fn", token.str_len) == 0)){

				// declare func
				token.type = TOKEN_FUNC;
			}
			else if((token.str_len == 2) && (strncmp(token.str, "co", token.str_len) == 0)){

				// declare coroutine
				token.type = TOKEN_COROUTINE;
			}
			else if((token.str_len == 3) && (strncmp(token.str, "for", token.str_len) == 0)){

				token.type = TOKEN_FOR;
			}
			else if((token.str_len == 5) && (strncmp(token.str, "while", token.str_len) == 0)){

				token.type = TOKEN_WHILE;
			}
			else if((token.str_len == 2) && (strncmp(token.str, "in", token.str_len) == 0)){

				token.type = TOKEN_IN;
			}
			else if((token.str_len == 5) && (strncmp(token.str, "break", token.str_len) == 0)){

				token.type = TOKEN_BREAK;
			}
			else if((token.str_len == 8) && (strncmp(token.str, "continue", token.str_len) == 0)){

				token.type = TOKEN_CONTINUE;
			}
			else if((token.str_len == 2) && (strncmp(token.str, "if", token.str_len) == 0)){

				token.type = TOKEN_IF;
			}
			else if((token.str_len == 4) && (strncmp(token.str, "else", token.str_len) == 0)){

				token.type = TOKEN_ELSE;
			}
			else if((token.str_len == 6) && (strncmp(token.str, "return", token.str_len) == 0)){

				token.type = TOKEN_RETURN;
			}
			else if((token.str_len == 5) && (strncmp(token.str, "yield", token.str_len) == 0)){

				token.type = TOKEN_YIELD;
			}
			else if((token.str_len == 5) && (strncmp(token.str, "delay", token.str_len) == 0)){

				token.type = TOKEN_DELAY;
			}
			else if((token.str_len == 4) && (strncmp(token.str, "rate", token.str_len) == 0)){

				token.type = TOKEN_RATE;
			}
			else if((token.str_len == 7) && (strncmp(token.str, "publish", token.str_len) == 0)){

				token.type = TOKEN_PUBLISH;
			}
			else if((token.str_len == 6) && (strncmp(token.str, "assert", token.str_len) == 0)){

				token.type = TOKEN_ASSERT;
			}
			else if(peek(lexer) == '('){

				token.type = TOKEN_CALL;
			}
			else{

				token.type = TOKEN_IDENT;
			}

			break;
		}
		else if(peek(lexer) == ';'){

			token.type = TOKEN_SEMICOLON;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		// operators
		else if(peek(lexer) == '('){

			token.type = TOKEN_LPAREN;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == ')'){

			token.type = TOKEN_RPAREN;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '{'){

			token.type = TOKEN_LBRACE;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '}'){

			token.type = TOKEN_RBRACE;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '['){

			token.type = TOKEN_LBRACKET;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == ']'){

			token.type = TOKEN_RBRACKET;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '.'){

			token.type = TOKEN_DOT;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '&'){

			token.type = TOKEN_REF;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '+') && (peek_next(lexer) == '=')){

			token.type = TOKEN_ADD_ASSIGN;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '-') && (peek_next(lexer) == '=')){

			token.type = TOKEN_SUB_ASSIGN;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '*') && (peek_next(lexer) == '=')){

			token.type = TOKEN_MUL_ASSIGN;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '/') && (peek_next(lexer) == '=')){

			token.type = TOKEN_DIV_ASSIGN;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '%') && (peek_next(lexer) == '=')){

			token.type = TOKEN_MOD_ASSIGN;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '+'){

			token.type = TOKEN_ADD;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '-'){

			token.type = TOKEN_SUB;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '*'){

			token.type = TOKEN_MUL;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '/'){

			token.type = TOKEN_DIV;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '%'){

			token.type = TOKEN_MOD;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '=') && (peek_next(lexer) == '=')){

			token.type = TOKEN_COMP_EQ;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '!') && (peek_next(lexer) == '=')){

			token.type = TOKEN_COMP_NEQ;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}

		else if((peek(lexer) == '<') && (peek_next(lexer) == '=')){

			token.type = TOKEN_COMP_LTE;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '<'){

			token.type = TOKEN_COMP_LT;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if((peek(lexer) == '>') && (peek_next(lexer) == '=')){

			token.type = TOKEN_COMP_GTE;
			token.str_len = 2;
			advance(lexer);
			advance(lexer);

			break;
		}
		else if(peek(lexer) == '>'){

			token.type = TOKEN_COMP_GT;
			token.str_len = 1;
			advance(lexer);

			break;
		}

		else if(peek(lexer) == '='){

			token.type = TOKEN_ASSIGN;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else if(peek(lexer) == ','){

			token.type = TOKEN_COMMA;
			token.str_len = 1;
			advance(lexer);

			break;
		}
		else{

			advance(lexer);
		}
	}

	// char s[256];
	// memcpy(s, token.str, token.str_len);
	// s[token.str_len] = 0;
	// printf("   TOK %d <%.*s> -> %d\n", token.type, token.str_len, s, token.str_len);

// done:

	if(print_tokens){

		print_token(&token);
	}

	return token;
}

token_t peek_current_token(lexer_t *lexer){

	// remember current position

	bool should_print = print_tokens;
	print_tokens = false;

	int temp_line = lexer->current_line_number;
	int temp_position = lexer->position;

	// get next token
	token_t token = get_token(lexer);

	// rewind input stream
	lexer->current_line_number = temp_line;
	lexer->position = temp_position;

	print_tokens = should_print;

	// done
	return token;
}

token_t peek_next_token(lexer_t *lexer){

	// remember current position

	bool should_print = print_tokens;
	print_tokens = false;

	int temp_line = lexer->current_line_number;
	int temp_position = lexer->position;

	// get next token, twice
	get_token(lexer);
	token_t next = get_token(lexer);

	// rewind input stream
	lexer->current_line_number = temp_line;
	lexer->position = temp_position;

	print_tokens = should_print;

	// done
	return next;
}

#endif
