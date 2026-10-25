
#ifdef FX4_COMPILER

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "program.h"
#include "bytecode.h"
#include "hash32.h"
#include "ir.h"
#include "debug_printf.h"

int handle_if(lexer_t *lexer, ir_state_t *ir_state);
int handle_else(lexer_t *lexer, ir_state_t *ir_state);
int handle_block(lexer_t *lexer, ir_state_t *ir_state);
int handle_return(lexer_t *lexer, ir_state_t *ir_state);
int handle_declare(lexer_t *lexer, ir_state_t *ir_state);
int handle_statement(lexer_t *lexer, ir_state_t *ir_state);

void print_error(char *file, int line, char *format, ...){


}

// lower number is higher precedence
int8_t precedence(token_type_t type){

    switch(type){
        case TOKEN_IDENT:
            return 1;
            break;

        case TOKEN_CALL:
            return 1;
            break;

        case TOKEN_DOT:
            return 1;
            break;

        case TOKEN_MUL:
        case TOKEN_DIV:
        case TOKEN_MOD:
            return 3;
            break;

        case TOKEN_ADD:
        case TOKEN_SUB:
            return 4;
            break;

        case TOKEN_COMP_LT:
        case TOKEN_COMP_LTE:
        case TOKEN_COMP_GT:
        case TOKEN_COMP_GTE:
        case TOKEN_IN:
            return 6;
            break;

        case TOKEN_COMP_EQ:
        case TOKEN_COMP_NEQ:
            return 7;
            break;

        case TOKEN_LOGICAL_AND:
        case TOKEN_LOGICAL_OR:
            return 8;
            break;

        case TOKEN_LVALUE:
            return 14;
            break;

        case TOKEN_ASSIGN:
        case TOKEN_ADD_ASSIGN:
        case TOKEN_SUB_ASSIGN:
        case TOKEN_MUL_ASSIGN:
        case TOKEN_DIV_ASSIGN:
        case TOKEN_MOD_ASSIGN:
            return 14;
            break;

        default:
            printf("PRECEDENCE ERROR: %d\n", type);
            exit(-1);
            return -1;
            break;
    }
}

static bool is_left_associated(token_type_t type){

    if((type == TOKEN_DECLARE) ||
       // (type == TOKEN_DOT) ||
       (type == TOKEN_ASSIGN)){

        return false;
    }

    return true;
}


#define SY_MODE_NORMAL  0
#define SY_MODE_DECLARE 1

// Shunting Yard
int shunting_yard(lexer_t *lexer, ir_state_t *ir_state, int mode){

    token_t output[PARSER_OUTPUT_STACK_SIZE];
    int output_idx = 0;

    token_t operator_stack[PARSER_OPERATOR_STACK_SIZE];
    int operator_idx = 0;


    // #define push_output(a) {program->output[program->output_idx++] = *a;}
    #define push_output(a) {output[output_idx++] = *a;}
    // #define push_output(a) {}

    #define print_output    {debug_printf("Output:\n");   for(int i = 0; i < output_idx; i++){debug_printf("\t"); print_token(&output[i]);}}
    #define print_operator  {debug_printf("Operator:\n"); for(int i = 0; i < operator_idx; i++){debug_printf("\t"); print_token(&operator_stack[i]);}}

    // print_token(start_token);

    token_t token = peek_current_token(lexer);
    token_t prev_token = {0};

    token_t operator_tos = {0};

    int open_brackets = 0;
    // int parsed_expressions = 0;

    debug_printf("EXPR SHUNTING YARD:\n");

    while(token.type != TOKEN_END){

        print_token(&token);     

        switch(token.type){
            case TOKEN_SEMICOLON:

                // parsed_expressions++;

                // printf("SEMICOLON\n");
                // push_output(&token);

                // end of expression, flush everything

                while(operator_idx > 0){

                    operator_tos = operator_stack[operator_idx - 1];

                    // pop operator stack to output_q
                    push_output(&operator_tos);
                    operator_idx--;
                }

                push_output(&token);
                get_token(lexer);
                
                // terminator on normal statements
                goto end;

                break;

            case TOKEN_LBRACE:
                
                if(operator_idx == 0){

                    goto end;
                }

                operator_tos = operator_stack[operator_idx - 1];

                if(operator_tos.type != TOKEN_ASSIGN){

                    goto end;
                }

                // // goto end;
                // get_token(lexer);
                    
                // push token onto operator stack
                operator_stack[operator_idx++] = token;

                get_token(lexer);

                // goto end;
                
                break;

            case TOKEN_RBRACE:
                // printf("RBRACE ERROR\n");
                // return -1;

                get_token(lexer);
                
                break;

            case TOKEN_COMMA:
                // end of expression, but we will continue
                // parsing additional expressions after
                // the comma (until semicolon)

                // parsed_expressions++;

                // flush everything to LPAREN or LBRACKET

                // while operator TOS is not LPAREN/LBRACKET:
                if(operator_idx > 0){

                    operator_tos = operator_stack[operator_idx - 1];

                    while((operator_tos.type != TOKEN_LPAREN) &&
                          (operator_tos.type != TOKEN_LBRACE) &&
                          (operator_tos.type != TOKEN_LBRACKET)){

                        // assert stack not empty
                        if(operator_idx == 0){

                            printf("ERROR (comma)!\n");

                            return -1;
                        }

                        // pop operator stack to output_q
                        push_output(&operator_tos);
                        operator_idx--;

                        // update operator TOS
                        operator_tos = operator_stack[operator_idx - 1];
                    }

                    // assert operator tos is LPAREN
                     // this should always be false and thus a warning in cppcheck!
                    // if((operator_tos.type != TOKEN_LPAREN) &&
                    //    (operator_tos.type != TOKEN_LBRACKET)){

                    //     // aren't supposed to get here!
                    //     printf("ERRORRORO (comma!)\n");
                    // }
                }

                push_output(&token);
                get_token(lexer);

                // print_output;
                // print_operator;

                break;

            case TOKEN_BOOL:
            case TOKEN_NUMBER:
            case TOKEN_IDENT:
            case TOKEN_STRING:
                
                // check if prev token is value or ident
                if((prev_token.type == TOKEN_BOOL) ||
                   (prev_token.type == TOKEN_NUMBER) ||
                   (prev_token.type == TOKEN_IDENT) ||
                   (prev_token.type == TOKEN_STRING)){

                    error_printf(token.line_num, "Missing separator between values\n");

                    return -1;
                }

                // check if next token is assign
                token_t next = peek_next_token(lexer);
                if(next.type == TOKEN_ASSIGN){

                    token.type = TOKEN_LVALUE;
                }
                // or assign op
                else if((next.type == TOKEN_ADD_ASSIGN) ||
                        (next.type == TOKEN_SUB_ASSIGN) ||
                        (next.type == TOKEN_MUL_ASSIGN) ||
                        (next.type == TOKEN_DIV_ASSIGN) ||
                        (next.type == TOKEN_MOD_ASSIGN)){


                    token.type = TOKEN_LVALUE;
                }

                push_output(&token);    
                
                get_token(lexer);
                break;

            case TOKEN_CALL:
                get_token(lexer);
                // print_token(&token);
                
                token_t call_args = token;
                call_args.type = TOKEN_CALL_ARGS;
                push_output(&call_args);

                // push token onto operator stack
                operator_stack[operator_idx++] = token;

                break;

            case TOKEN_LPAREN:
                get_token(lexer);
                    
                // push token onto operator stack
                operator_stack[operator_idx++] = token;
                    
                break;

            case TOKEN_RPAREN:
                get_token(lexer);

                // while operator TOS is not LPAREN:
                if(operator_idx > 0){

                    operator_tos = operator_stack[operator_idx - 1];

                    while(operator_tos.type != TOKEN_LPAREN){

                        // assert stack not empty
                        if(operator_idx == 0){

                            printf("ERROR LPAREN\n");

                            return -1;
                        }

                        // pop operator stack to output_q
                        push_output(&operator_tos);
                        operator_idx--;

                        // update operator TOS
                        operator_tos = operator_stack[operator_idx - 1];
                    }

                    // assert operator tos is LPAREN
                    if(operator_tos.type != TOKEN_LPAREN){ // this should always be false and thus a warning in cppcheck!

                        // aren't supposed to get here!
                        printf("ERRORRORO LPAREN\n");
                    }

                    // pop LPAREN and discard
                    operator_idx--;

                    // if any operators are left:
                    if(operator_idx > 0){

                        // update operator TOS
                        operator_tos = operator_stack[operator_idx - 1];

                        // if function:
                        if(operator_tos.type == TOKEN_CALL){

                            // printf("LPAREN FUNC\n");

                            // pop function to output queue:

                            // pop operator stack to output_q
                            push_output(&operator_tos);
                            operator_idx--;
                        }
                    }
                }
                
                break;

            case TOKEN_LBRACKET:
                get_token(lexer); // consume

                if((mode == SY_MODE_DECLARE) && (open_brackets == 0)){

                    // skip pushing first bracket in declare mode
                    // this is because we don't want to process the brackets
                    // as an array index lookup in declare mode.
                }
                else{

                    // only push to output in normal mode
                    
                    // check if previous is ident (should be)
                    if((output_idx > 0) && (output[output_idx - 1].type == TOKEN_IDENT)){

                        // pop the ident from output and push to operator stack.
                        // this way the rbacket can find the ident and pull it forward
                        operator_stack[operator_idx++] = output[output_idx - 1];
                        output_idx--;
                    }
                    else{

                        printf("LBRACKET must follow IDENT\n");

                        return -1;
                    }
                }

                // push token onto operator stack
                operator_stack[operator_idx++] = token;

                open_brackets++;

                push_output(&token);

                break;

            case TOKEN_RBRACKET:

                if(open_brackets <= 0){

                    printf("ERROR: bracket mismatch!\n");
                    return -1;
                }

                open_brackets--;

                get_token(lexer);
                // finish subscription

                // printf("R BRACKET: %d\n", operator_idx);

                // flush operators until LBRACKET
                if(operator_idx > 0){

                    operator_tos = operator_stack[operator_idx - 1];

                    while(operator_tos.type != TOKEN_LBRACKET){

                        // assert stack not empty
                        if(operator_idx == 0){

                            printf("ERROR LBRACKET\n");
                            return -1;
                        }

                        // pop operator stack to output_q
                        push_output(&operator_tos);
                        operator_idx--;

                        // update operator TOS
                        operator_tos = operator_stack[operator_idx - 1];
                    }

                    // assert operator tos is LBRACKET
                    if(operator_tos.type != TOKEN_LBRACKET){ // this should always be false and thus a warning in cppcheck!

                        // aren't supposed to get here!
                        printf("ERRORRORO LBRACKET\n");

                        return -1;
                    }

                    // pop LBRACKET and discard

                    operator_idx--;

                    // push token to output q
                    if((mode == SY_MODE_DECLARE) && (open_brackets == 0)){

                        // skip pushing first bracket in declare mode
                        // this is because we don't want to process the brackets
                        // as an array index lookup in declare mode.
                    }
                    else{

                        if(operator_idx == 0){

                            printf("ERROR OP IDX\n");

                            return -1;
                        }

                        operator_tos = operator_stack[operator_idx - 1];

                        // there should be an IDENT before the LBRACKET,
                        // let's get it so we can move it to the array index
                        if(operator_tos.type != TOKEN_IDENT){

                            printf("LBRACKET missing IDENT!\n");
                            return -1;
                        }

                        // check if next token is assign
                        next = peek_current_token(lexer);
                        
                        if(next.type == TOKEN_ASSIGN){

                            // change to lvalue!
                            operator_tos.type = TOKEN_LVALUE;
                        }

                        // push bracket to output
                        push_output(&token);

                        // push ident to output
                        push_output(&operator_tos);   

                        // pop IDENT from operator stack
                        operator_idx--;
                    
                        // change token type array index
                        // token.type = TOKEN_ARRAY_INDEX;
                        // push_output(&token);    
                    }
                    
                    // RBRACKET acts like an operator:
                    // pop 2 values from stack, the computed index
                    // and the identifier to lookup
                }
                else if(open_brackets > 0){

                    printf("Missing LBRACKET?\n");

                    return -1;
                }

                // push_output(&token);


                break;

            case TOKEN_ADD_ASSIGN:
            case TOKEN_SUB_ASSIGN:
            case TOKEN_MUL_ASSIGN:
            case TOKEN_DIV_ASSIGN:
            case TOKEN_MOD_ASSIGN:            

                // check if previous token is rbracket
                if(prev_token.type == TOKEN_RBRACKET){

                    // check output is ident
                    if(output[output_idx - 1].type == TOKEN_IDENT){

                        // change to LVALUE!
                        output[output_idx - 1].type = TOKEN_LVALUE;
                    }

                }
                __attribute__ ((fallthrough));
            
            case TOKEN_ASSIGN:
            case TOKEN_DOT:
            case TOKEN_ADD:
            case TOKEN_SUB:
            case TOKEN_MUL:
            case TOKEN_DIV:
            case TOKEN_MOD:
            case TOKEN_LOGICAL_AND:
            case TOKEN_LOGICAL_OR:
            case TOKEN_COMP_LT:
            case TOKEN_COMP_LTE:
            case TOKEN_COMP_GT:
            case TOKEN_COMP_GTE:
            case TOKEN_COMP_EQ:
            case TOKEN_COMP_NEQ:
            case TOKEN_IN:
                
                if((token.type == TOKEN_ASSIGN) && (mode == SY_MODE_DECLARE)){

                    token_t semicolon = {
                        .type = TOKEN_SEMICOLON,
                        .line_num = token.line_num,
                    };

                    push_output(&semicolon);

                    goto end;
                }

                get_token(lexer);

                // check precedence:
                // all higher precedence operators cleared from stack
                // first

                // first check if stack has anything on it:
                if(operator_idx > 0){

                    operator_tos = operator_stack[operator_idx - 1];

                    while(
                            ((operator_tos.type != TOKEN_LPAREN) &&
                             (operator_tos.type != TOKEN_LBRACKET) &&
                             (operator_tos.type != TOKEN_LBRACE)) && // operator TOS is NOT left paren AND NOT left bracket
                            (
                                (precedence(operator_tos.type) < precedence(token.type)) || // (operator TOS has greater precedence than token) OR
                                (
                                    (precedence(operator_tos.type) == precedence(token.type)) && // (they are same AND token is left associated)
                                    (is_left_associated(token.type))
                                )
                            )
                        ){


                        // printf("POP TOS: %d op: %d\n", operator_tos.type, token.type);

                        // pop operator TOS into output
                        push_output(&operator_tos);
                        operator_idx--;

                        // check if operator stack is empty:
                        if(operator_idx == 0){

                            break;
                        }

                        // we are looping around, get updated TOS:
                        operator_tos = operator_stack[operator_idx - 1];
                    }
                }

                // push token onto operator stack
                operator_stack[operator_idx++] = token;

                break;

            default:
                error_printf(token.line_num, "Error parsing expression, unknown token");
                print_token(&token);
                return -1;
                break;
        }

        prev_token = token;
        token = peek_current_token(lexer);
    }

end:


    debug_printf("open brackets: %d\n", open_brackets);

    // check for unbalanced brackets
    if(open_brackets > 0){

        printf("ERROR unbalanced brackets\n");

        return -1;
    }

    // flush operator stack onto output queue
    while(operator_idx > 0){

        operator_idx--;

        push_output(&operator_stack[operator_idx]);
    }

    if(token.type == TOKEN_END){

        printf("ERROR Reached end of input before completing expression\n");
        return -1;
    }

    // if(token.type == TOKEN_SEMICOLON){

    //     push_output(&token);
    // }


    // don't consume LBRACE, for loop needs it
    // to open block
    // don't consume COMMA, call args needs it for next arg

    debug_printf("STREAM\n");

    for(int i = 0; i < output_idx; i++){

        print_token(&output[i]);
    }

    // Render shunting yard into IR

    debug_printf("RENDER\n");

    int parsed_expressions = 0;
        
    ir_t ir = {0};

    for(int i = 0; i < output_idx; i++){
        
        // ir_t ir_peeked = {0};

        ir.line_num = output[i].line_num;

        print_token(&output[i]);

        switch(output[i].type){
            case TOKEN_IDENT:
                ir.type = IR_IDENT;
                ir.args.ident.ident_hash = hash_u32_data((uint8_t *)output[i].str, output[i].str_len);

                // check if next token is lbracket
                if(output[i + 1].type == TOKEN_LBRACKET){

                    // warp forward to closing rbracket
                    open_brackets = 0;

                    for(int j = i + 1; j < output_idx - 2; j++){

                        if(output[j].type == TOKEN_LBRACKET){

                            open_brackets++;
                        }
                        else if(output[j].type == TOKEN_RBRACKET){

                            open_brackets--;

                            if(open_brackets == 0){

                                // check if next + 1 token is assign:
                                // IE, is this array lookup an LVALUE
                                if(output[j + 2].type == TOKEN_ASSIGN){

                                    debug_printf("------------ bracket assign\n");
                                    ir.type = IR_LVALUE;
                                }
                            }
                        }
                    }
                }


                ir_add(ir_state, &ir);

                break;

            case TOKEN_LVALUE:
                ir.type = IR_LVALUE;
                ir.args.ident.ident_hash = hash_u32_data((uint8_t *)output[i].str, output[i].str_len);
                ir_add(ir_state, &ir);

                break;

            case TOKEN_LBRACKET:
                break;

            case TOKEN_RBRACKET:
                int exprs = 1;

                // scroll back to LBRACKETs and
                // count number of args via COMMA
                for(int j = i - 1; j >= 0; j--){

                    if(output[j].type == TOKEN_LBRACKET){

                        output[j].type = TOKEN_NOP;

                        break;
                    }            
                    else if(output[j].type == TOKEN_COMMA){

                        output[j].type = TOKEN_NOP;

                        // remove an arg from parsed expressions as there
                        // will only be a single output expression from the bracket.
                        // this is similar to call.
                        parsed_expressions--; 

                        exprs++;
                    }
                }

                debug_printf("EXPRS %d\n", exprs);

                // ir.type = IR_END_EXPR;
                // ir_add(ir_state, &ir);

                ir.type = IR_BRACKET;
                ir.args.bracket.exprs = exprs;
                ir_add(ir_state, &ir);

                break;

            case TOKEN_LBRACE:
                break;

            case TOKEN_NUMBER:
                ir.type = IR_PUSH_CONST;
                    
                if(output[i].data_type == TYPE_NUMBER){

                    ir.args.constant.value = output[i].number.i32;
                }
                else if(output[i].data_type == TYPE_FIXED16){

                    ir.args.constant.value = output[i].number.f16;
                }

                ir.data_type = output[i].data_type;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_BOOL:
                ir.type = IR_PUSH_CONST;
                    
                ir.args.constant.value = output[i].number.i32;
                
                ir.data_type = output[i].data_type;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_STRING:
                ir.type = IR_PUSH_CONST;
            
                ir.args.constant.value = hash_u32_data((uint8_t *)output[i].str, output[i].str_len);

                ir.data_type = output[i].data_type;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_ASSIGN:
                ir.type = IR_ASSIGN;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_ADD:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_ADD;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_ADD_ASSIGN:
                ir.type = IR_ASSIGN_OP;
                
                ir.args.assign_op.op = IR_OP_ADD;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_SUB_ASSIGN:
                ir.type = IR_ASSIGN_OP;
                
                ir.args.assign_op.op = IR_OP_SUB;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_MUL_ASSIGN:
                ir.type = IR_ASSIGN_OP;
                
                ir.args.assign_op.op = IR_OP_MUL;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_DIV_ASSIGN:
                ir.type = IR_ASSIGN_OP;
                
                ir.args.assign_op.op = IR_OP_DIV;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_MOD_ASSIGN:
                ir.type = IR_ASSIGN_OP;
                
                ir.args.assign_op.op = IR_OP_MOD;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_SUB:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_SUB;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_MUL:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_MUL;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_DIV:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_DIV;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_MOD:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_MOD;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMP_EQ:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_COMPEQ;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMP_NEQ:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_COMPNEQ;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMP_LT:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_COMPLT;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMP_LTE:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_COMPLTE;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMP_GT:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_COMPGT;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMP_GTE:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_COMPGTE;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_IN:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_IN;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_LOGICAL_AND:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_LOGICAL_AND;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_LOGICAL_OR:
                ir.type = IR_BINOP;
                
                ir.args.binop.op = IR_OP_LOGICAL_OR;

                ir_add(ir_state, &ir);

                break;

            // case TOKEN_SEMICOLON:
            //     // ir.type = IR_END_EXPR;

            //     // ir_add(ir_state, &ir);
            //     printf("Stray semicolon\n");
            //     return -1;

            //     break;

            case TOKEN_CALL_ARGS:
                debug_printf("CALL ARGS: %.*s\n", output[i].str_len, output[i].str);
                break;

            case TOKEN_CALL:
                ir.type = IR_CALL;
                ir.args.call.ident_hash = hash_u32_data((uint8_t *)output[i].str, output[i].str_len);

                int arg_count = 0;

                if(output[i - 1].type != TOKEN_CALL_ARGS){

                    arg_count++;

                    // scroll back to call args and
                    // count number of args
                    for(int j = i - 1; j >= 0; j--){

                        if(output[j].type == TOKEN_CALL_ARGS){

                            output[j].type = TOKEN_NOP;

                            break;
                        }
                        else if(output[j].type == TOKEN_COMMA){

                            output[j].type = TOKEN_NOP;

                            // remove an arg from parsed expressions as there
                            // will only be a single output expression from the call.
                            // this is similar to bracket.
                            parsed_expressions--; 

                            arg_count++;
                        }            
                    }
                }

                debug_printf("CALL: %s args: %d\n", hash_s_get_str(ir.args.call.ident_hash), arg_count);
                
                ir.args.call.arg_count = arg_count;

                ir_add(ir_state, &ir);

                break;

            case TOKEN_DOT:
                ir.type = IR_DOT;
                ir_add(ir_state, &ir);

                break;

            case TOKEN_COMMA:
                parsed_expressions++;

                // ir.type = IR_END_EXPR;
                // ir_add(ir_state, &ir);

                break;

            case TOKEN_SEMICOLON:
                parsed_expressions++;

                // ir.type = IR_END_EXPR;
                // ir_add(ir_state, &ir);

                break;

            case TOKEN_NOP:
                break;

            default:
                debug_printf(">>>>>>>>>>>>>>>>>>\n");
                debug_printf("unknown expr type: %d\n", output[i].type);
                print_token(&output[i]);
                debug_printf("<<<<<<<<<<<<<<<<<<\n");
                return -1;
                break;
        }
    }

    // ir.type = IR_END_EXPR;
    // ir_add(ir_state, &ir);


    // reset output
    // output_idx = 0;

    debug_printf("EXPR_END: %d exprs\n", parsed_expressions);

    return parsed_expressions;
}

int handle_ident(lexer_t *lexer, ir_state_t *ir_state){

    // check if object instantiation (declare):
    token_t token = peek_current_token(lexer);

    if(token.type != TOKEN_IDENT){

        printf("ERROR (ident)\n");
        return -1;
    }

    uint32_t name_hash = hash_u32_data((uint8_t *)token.str, token.str_len);

    // lookup obj def
    if(ir_lookup_obj_def(ir_state, name_hash, NULL) >= 0){

        // this token is actually a declare for an object
        return handle_declare(lexer, ir_state);

    }
    
    return shunting_yard(lexer, ir_state, SY_MODE_NORMAL);
}

int handle_while(lexer_t *lexer, ir_state_t *ir_state){

    token_t token = get_token(lexer);
    // print_token(token);

    if(token.type != TOKEN_WHILE){

        printf("ERROR (while)\n");
        return -1;
    }

    debug_printf("WHILE:\n");

    // create labels
    int label_loop_top = ir_create_label(ir_state);

    if(label_loop_top < 0){

        return -1;
    }

    if(ir_push_entry_label(ir_state, label_loop_top) < 0){

        return -1;
    }

    int label_loop_exit = ir_create_label(ir_state);

    if(label_loop_exit < 0){

        return -1;
    }

    if(ir_push_exit_label(ir_state, label_loop_exit) < 0){

        return -1;
    }

    // insert label for jump target to top
    ir_t ir_label_loop_top = {
        .type = IR_LABEL,
        .line_num = token.line_num,
        .args.label = {
            .label_index = label_loop_top,
        },
    };

    ir_add(ir_state, &ir_label_loop_top);

    if(shunting_yard(lexer, ir_state, SY_MODE_NORMAL) < 0){

        printf("Invalid while loop expression\n");

        return -1;
    }

    // jump if false (zero) to exit
    ir_t ir_jump_cond = {
        .type = IR_JUMP,
        .line_num = token.line_num,
        .args.jump = {
            .jump = IR_JUMP_ZERO,
            .label_index = label_loop_exit,
        },
    };

    ir_add(ir_state, &ir_jump_cond);
    
    if(handle_block(lexer, ir_state) < 0){

        return -1;
    }

    // jump to top
    ir_t ir_jump_top = {
        .type = IR_JUMP,
        .line_num = token.line_num,
        .args.jump = {
            .jump = IR_JUMP_ALWAYS,
            .label_index = label_loop_top,
        },
    };

    ir_add(ir_state, &ir_jump_top);
    

        // insert label for jump target to exit
    ir_t ir_label_loop_exit = {
        .type = IR_LABEL,
        .line_num = token.line_num,
        .args.label = {
            .label_index = label_loop_exit,
        },
    };

    ir_add(ir_state, &ir_label_loop_exit);


    if(ir_pop_entry_label(ir_state) < 0){

        return -1;
    }

    if(ir_pop_exit_label(ir_state) < 0){

        return -1;
    }

    return 0;
}

int handle_for(lexer_t *lexer, ir_state_t *ir_state){

    token_t token = get_token(lexer);
    // print_token(token);

    if(token.type != TOKEN_FOR){

        printf("ERROR (for)\n");
        return -1;
    }

    debug_printf("FOR:\n");

    token_t next = peek_current_token(lexer);

    if(next.type != TOKEN_IDENT){

        printf("Missing ident for loop\n");
        return -1;
    }

    // create labels
    int label_loop_top = ir_create_label(ir_state);

    if(label_loop_top < 0){

        return -1;
    }

    int label_loop_cont = ir_create_label(ir_state);

    if(label_loop_cont < 0){

        return -1;
    }

    if(ir_push_entry_label(ir_state, label_loop_cont) < 0){

        return -1;
    }


    int label_loop_exit = ir_create_label(ir_state);

    if(label_loop_exit < 0){

        return -1;
    }

    if(ir_push_exit_label(ir_state, label_loop_exit) < 0){

        return -1;
    }

    ir_t ir_loop_init = {
        .type = IR_LOOP_INIT,
        .line_num = token.line_num,
        .args.loop_init = {
            .ident_hash = hash_u32_data((uint8_t *)next.str, next.str_len),
        },
        .data_type = TYPE_NONE,
    };
    ir_add(ir_state, &ir_loop_init);

    // insert label for jump target to top
    ir_t ir_label_loop_top = {
        .type = IR_LABEL,
        .line_num = token.line_num,
        .args.label = {
            .label_index = label_loop_top,
        },
    };
    ir_add(ir_state, &ir_label_loop_top);

    if(shunting_yard(lexer, ir_state, SY_MODE_NORMAL) < 0){

        printf("Invalid for loop expression\n");

        return -1;
    }

    // jump if zero to end
    ir_t ir_jump_cond = {
        .type = IR_JUMP,
        .line_num = token.line_num,
        .args.jump = {
            .jump = IR_JUMP_ZERO,
            .label_index = label_loop_exit,
        },
    };
    ir_add(ir_state, &ir_jump_cond);


    if(handle_block(lexer, ir_state) < 0){

        return -1;
    }

    // insert label for jump target to continue
    ir_t ir_label_loop_cont = {
        .type = IR_LABEL,
        .line_num = token.line_num,
        .args.label = {
            .label_index = label_loop_cont,
        },
    };
    ir_add(ir_state, &ir_label_loop_cont);

    ir_t ir_loop_exit = {
        .type = IR_LOOP_EXIT,
        .line_num = token.line_num,
        .args.loop_exit = {
            .ident_hash = hash_u32_data((uint8_t *)next.str, next.str_len),
            .label_index = label_loop_top,
        },
        .data_type = TYPE_NONE,
    };
    ir_add(ir_state, &ir_loop_exit);

    // insert label for loop exit
    ir_t ir_label_loop_exit = {
        .type = IR_LABEL,
        .line_num = token.line_num,
        .args.label = {
            .label_index = label_loop_exit,
        },
    };
    ir_add(ir_state, &ir_label_loop_exit);

    if(ir_pop_entry_label(ir_state) < 0){

        return -1;
    }

    if(ir_pop_exit_label(ir_state) < 0){

        return -1;
    }

    debug_printf("END_FOR\n");

    return 0;
}

int handle_if(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("IF:\n");

    token_t token = get_token(lexer);
    // print_token(token);

    if(token.type != TOKEN_IF){

        printf("ERROR (if)\n");
        return -1;
    }

    if(shunting_yard(lexer, ir_state, SY_MODE_NORMAL) < 0){

        error_printf(token.line_num, "Invalid if expression");

        return -1;
    }

    // create label
    int label = ir_create_label(ir_state);

    if(label < 0){

        return -1;
    }

    // insert jump to label
    ir_t ir_jump = {
        .type = IR_JUMP,
        .line_num = token.line_num,
        .args.jump = {
            .jump = IR_JUMP_ZERO,
            .label_index = label,
        },
    };

    ir_add(ir_state, &ir_jump);
    
    if(handle_block(lexer, ir_state) < 0){

        return -1;
    }

    debug_printf("END_IF\n");

    // peek for ELSE
    token_t next = peek_current_token(lexer);
    int else_label = -1;

    if(next.type == TOKEN_ELSE){

        // create label
        else_label = ir_create_label(ir_state);

        if(else_label < 0){

            return -1;
        }

        // insert jump to label
        ir_t ir_jump_else = {
            .type = IR_JUMP,
            .line_num = next.line_num,
            .args.jump = {
                .jump = IR_JUMP_ALWAYS,
                .label_index = else_label,
            },
        };

        ir_add(ir_state, &ir_jump_else);
    }

    // insert label for jump target to else
    ir_t ir_label = {
        .type = IR_LABEL,
        .line_num = token.line_num,
        .args.label = {
            .label_index = label,
        },
    };

    ir_add(ir_state, &ir_label);
    
    
    if(next.type == TOKEN_ELSE){

        if(handle_else(lexer, ir_state) < 0){

            return -1;
        }

        // insert label for jump target to else
        ir_t ir_else_label = {
            .type = IR_LABEL,
            .line_num = next.line_num,
            .args.label = {
                .label_index = else_label,
            },
        };

        ir_add(ir_state, &ir_else_label);
    }

    return 0;
}

int handle_else(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("ELSE:\n");

    token_t token = get_token(lexer);
    // print_token(token);

    if(token.type != TOKEN_ELSE){

        printf("ERROR (else)\n");
        return -1;
    }

    token_t next = peek_current_token(lexer);
    // print_token(&next);

    // check if this is actually an ELSE_IF!
    if(next.type == TOKEN_IF){

        if(handle_if(lexer, ir_state) < 0){

            return -1;
        }
    }
    else if(next.type == TOKEN_LBRACE){


        if(handle_block(lexer, ir_state) < 0){

            return -1;
        }
    }
    else{

        printf("Invalid token after ELSE\n");

        return -1;
    }

    debug_printf("END_ELSE\n");

    return 0;
}

int handle_break(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("BREAK\n");

    token_t token = get_token(lexer);
    // print_token(token);

    if(token.type != TOKEN_BREAK){

        printf("ERROR (break)\n");
        return -1;
    }

    int label = ir_peek_exit_label(ir_state);

    if(label < 0){

        return -1;
    }

    ir_t ir_break = {
        .type = IR_JUMP,
        .line_num = token.line_num,
        .args.jump = {
            .jump = IR_JUMP_BREAK,
            .label_index = label,
        },
    };

    ir_add(ir_state, &ir_break);

    return 0;
}


int handle_continue(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("CONTINUE\n");

    token_t token = get_token(lexer);
    // print_token(token);

    if(token.type != TOKEN_CONTINUE){

        printf("ERROR (continue)\n");
        return -1;
    }

    int label = ir_peek_entry_label(ir_state);

    if(label < 0){

        return -1;
    }

    ir_t ir_continue = {
        .type = IR_JUMP,
        .line_num = token.line_num,
        .args.jump = {
            .jump = IR_JUMP_CONTINUE,
            .label_index = label,
        },
    };

    ir_add(ir_state, &ir_continue);

    return 0;
}

int handle_assert(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("Handle ASSERT\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_ASSERT){

        printf("ERROR (assert)\n");
        return -1;
    }

    int parsed_expressions = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);

    if(parsed_expressions < 0){

        printf("Invalid expression\n");

        return -1;
    }

    debug_printf("ASSERT: %d exprs\n", parsed_expressions);

    if(parsed_expressions > 1){

        printf("Multiple assertion values not allowed\n");

        return -1;
    }

    ir_t ir = {
        .type = IR_ASSERT,
        .line_num = token.line_num,
    };

    ir_add(ir_state, &ir);

    return 0;
}

int handle_declare_object(lexer_t *lexer, ir_state_t *ir_state){
        
    token_t token = get_token(lexer);
    
    debug_printf("DECLARE OBJECT: %.*s\n", token.str_len, token.str);
    
    if(token.type != TOKEN_IDENT){
   
        printf("ERROR (declare object)\n");
        return -1;
    }

    uint32_t obj_hash = hash_u32_data((uint8_t *)token.str, token.str_len);
    
    if(ir_add_object(ir_state, obj_hash) < 0){

        printf("Failed to declare object\n");

        return -1;
    }

    // expecting LBRACE to open object definition:
    token = get_token(lexer);

    if(token.type != TOKEN_LBRACE){

        printf("Missing LBRACE on object definition\n");
        return -1;
    }

    token = get_token(lexer);
    int field_index = 0;

    while(token.type != TOKEN_RBRACE){

        // expecting declare:
        // print_token(&token);

        if(token.type != TOKEN_DECLARE){

            printf("Expected DECLARE in object definition\n");
            return -1;
        }

        // get ident
        token_t ident = get_token(lexer);

        if(ident.type != TOKEN_IDENT){

            printf("Expected IDENT in object definition\n");
            return -1;
        }

        data_type_t data_type = token.data_type;

        // check for array
        token_t next = peek_current_token(lexer);
        // print_token(&next);

        // check if array (starts with open bracket for size):
        if(next.type == TOKEN_LBRACKET){

            data_type |= TYPE_ARRAY;   

            // get_token(lexer);

            next = peek_next_token(lexer);

            // check if missing expressions
            if(next.type == TOKEN_RBRACKET){

                printf("ERROR cannot declare array with no rank\n");
                return -1;
            }

            int rank = shunting_yard(lexer, ir_state, SY_MODE_DECLARE);

            if(rank < 0){

                printf("Invalid expression (declare)\n");

                return -1;
            }
            else if(rank > 0){

                printf("Arrays not supported within objects!\n");

                return -1;
            }
            
            printf("\tarray rank: %d\n", rank);   
        }


        // get semicolon
        token_t semicolon = get_token(lexer);

        if(semicolon.type != TOKEN_SEMICOLON){

            printf("Missing semicolon in object definition\n");
            return -1;
        }

        debug_printf("\tFIELD: %.*s: %.*s\n", token.str_len, token.str, ident.str_len, ident.str);


        ir_t ir_obj = {
            .type = IR_OBJ_FIELD,
            .line_num = token.line_num,
            .args.obj = {
                .obj_hash = obj_hash,
                .data_type = data_type,
                .field_hash = hash_u32_data((uint8_t *)ident.str, ident.str_len),
            },
            .data_type = TYPE_OBJECT,
        };

        ir_add(ir_state, &ir_obj);
        
        field_index++;

        if(field_index >= MAX_OBJECT_FIELDS){

            printf("Exceeded maximum number of object fields.\n");
            return -1;
        }

        token = get_token(lexer);
    }

    if(token.type != TOKEN_RBRACE){

        printf("Missing RBRACE on object definition\n");
        return -1;
    }
    
    return 0;
}

int handle_declare(lexer_t *lexer, ir_state_t *ir_state){

    token_t token = get_token(lexer);
    token_t ident = peek_current_token(lexer);

    bool is_ref = false;
    if(ident.type == TOKEN_REF){

        is_ref = true;
        get_token(lexer);
        ident = peek_current_token(lexer);
    }

    if(is_ref){

        debug_printf("DECLARE REF: %.*s&: %.*s\n", token.str_len, token.str, ident.str_len, ident.str);
    }
    else{

        debug_printf("DECLARE: %.*s: %.*s\n", token.str_len, token.str, ident.str_len, ident.str);    
    }
    

    uint32_t declare_hash = hash_u32_data((uint8_t *)token.str, token.str_len);

    // check if object creation:
    // lookup obj def
    int obj_idx = ir_lookup_obj_def(ir_state, declare_hash, NULL);

    if(obj_idx >= 0){

        debug_printf("Object creation\n");
    }
    else if(token.type != TOKEN_DECLARE){

        printf("ERROR (declare)\n");
        return -1;
    }

    data_type_t data_type = token.data_type;

    if(obj_idx >= 0){

        data_type = TYPE_OBJECT | obj_idx;
    }
    else if(data_type == TYPE_OBJECT_DEF){
    
        return handle_declare_object(lexer, ir_state);
    }

    token_t next = peek_next_token(lexer);
    // print_token(&next);

    // check if array (starts with open bracket for size):
    if(next.type == TOKEN_LBRACKET){

        data_type |= TYPE_ARRAY;   
    }


    ir_t ir = {
        .type = IR_DECLARE,
        .line_num = token.line_num,
        .args.declare = {
            .ident_hash = hash_u32_data((uint8_t *)ident.str, ident.str_len),
            .is_ref = is_ref,
        },
        .data_type = data_type,
    };

    bool is_array = false;

    // check if array:
    if(next.type == TOKEN_LBRACKET){

        is_array = true;

        get_token(lexer);

        next = peek_next_token(lexer);

        int rank = -1;

        // check if missing expressions
        if(next.type == TOKEN_RBRACKET){

            // printf("ERROR cannot declare array with no rank\n");
            // return -1;

            get_token(lexer);
            get_token(lexer);
        }
        else{
        
            rank = shunting_yard(lexer, ir_state, SY_MODE_DECLARE);

            if(rank <= 0){

                printf("Invalid expression (declare)\n");

                return -1;
            }
        }
        
        ir.args.declare.rank = rank;

        next = peek_current_token(lexer);
    }
    
    // push IR nop placeholder for array size const, if needed
    ir_t ir_nop = {
        .type = IR_NOP,
        .line_num = next.line_num,
    };

    ir_add(ir_state, &ir_nop);
    int ir_nop_index = array_get_count(ir_state->ir_list) - 1;

    // add declare IR
    ir_add(ir_state, &ir);    
    int ir_index = array_get_count(ir_state->ir_list) - 1;

    if(next.type == TOKEN_ASSIGN){

        debug_printf("DECLARE ASSIGN\n");

        if(!is_array){

            get_token(lexer);    
        }
        
        // push IR lvalue
        ir_t ir_ident = {
            .type = IR_LVALUE,
            .line_num = next.line_num,
            .args.ident = {
                .ident_hash = hash_u32_data((uint8_t *)ident.str, ident.str_len),
            },
            .data_type = data_type,
        };

        ir_add(ir_state, &ir_ident);

        int status = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);

        if(status < 0){

            return status;
        }

        if(ir.args.declare.rank < 0){

            ir_t *ir_ptr = ir_get_index(ir_state, ir_index);
            ir_ptr->args.declare.rank = 1;

            // insert IR const for array size
            ir_t ir_ident2 = {
                .type = IR_PUSH_CONST,
                .line_num = ir_nop.line_num,
                .args.constant.value = status,
                .data_type = TYPE_NUMBER,
            };

            ir_t *ir_nop_ptr = ir_get_index(ir_state, ir_nop_index);
            *ir_nop_ptr = ir_ident2;
        }
    }
    else if(next.type == TOKEN_SEMICOLON){

        // end of expression
        // this declaration has no initializer and is not an array
        // we can pop the ident token since we will not be assigning to it.
        get_token(lexer);
    }

    return 0;
}


int handle_return(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("Handle RETURN\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_RETURN){

        printf("ERROR (return)\n");
        return -1;
    }

    // token_t next = peek_current_token(lexer);

    int parsed_expressions = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);

    if(parsed_expressions < 0){

        printf("Invalid expression\n");

        return -1;
    }

    debug_printf("RETURN: %d exprs\n", parsed_expressions);

    if(parsed_expressions > 1){

        printf("Multiple return values not allowed\n");

        return -1;
    }

    // consume semicolon    
    // token = get_token(lexer);

    ir_t ir = {
        .type = IR_RETURN,
        .line_num = token.line_num,
        .args.ret.return_expressions = parsed_expressions,
    };

    ir_add(ir_state, &ir);

    debug_printf("RETURN:\n");

    return 0;
}

int handle_yield(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("Handle YIELD\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_YIELD){

        printf("ERROR (yield)\n");
        return -1;
    }

    ir_t ir = {
        .type = IR_YIELD,
        .line_num = token.line_num,
    };

    ir_add(ir_state, &ir);

    return 0;
}

int handle_delay(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("Handle DELAY\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_DELAY){

        printf("ERROR (delay)\n");
        return -1;
    }

    int parsed_expressions = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);

    if(parsed_expressions < 0){

        printf("Invalid expression\n");

        return -1;
    }

    debug_printf("DELAY: %d exprs\n", parsed_expressions);


    ir_t ir = {
        .type = IR_DELAY,
        .line_num = token.line_num,
    };

    ir_add(ir_state, &ir);

    return 0;
}

int handle_rate(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("Handle RATE\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_RATE){

        printf("ERROR (rate)\n");
        return -1;
    }

    int parsed_expressions = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);

    if(parsed_expressions < 0){

        printf("Invalid expression\n");

        return -1;
    }

    debug_printf("RATE: %d exprs\n", parsed_expressions);


    ir_t ir = {
        .type = IR_RATE,
        .line_num = token.line_num,
    };

    ir_add(ir_state, &ir);

    return 0;
}

int handle_publish(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("Handle PUBLISH\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_PUBLISH){

        printf("ERROR (publish)\n");
        return -1;
    }

    token_t ident = get_token(lexer);

    if(ident.type != TOKEN_IDENT){

        printf("ident must follow publish\n");
        return -1;
    }

    debug_printf("PUBLISH: %s \n", ident.str);

    uint32_t ident_hash = hash_u32_data((uint8_t *)ident.str, ident.str_len);

    ir_t ir = {
        .type = IR_PUBLISH,
        .args.ident.ident_hash = ident_hash,
        .line_num = token.line_num,
    };

    ir_add(ir_state, &ir);

    return 0;
}

int handle_func_args(lexer_t *lexer, ir_state_t *ir_state, char *func_name, int name_len){

    token_t token = get_token(lexer);

    if(token.type != TOKEN_LPAREN){

        printf("Missing LPAREN\n");
        return -1;
    }

    debug_printf("\tARGS:\n");

    token = peek_current_token(lexer);

    int arg_count = 0;

    while(token.type != TOKEN_RPAREN){
        // print_token(&token);

        switch(token.type){
            case TOKEN_DECLARE:
                if(handle_declare(lexer, ir_state) < 0){

                    return -1;
                }
                arg_count++;
                break;

            case TOKEN_COMMA:
                get_token(lexer);
                break;

            case TOKEN_IDENT:
                get_token(lexer);
                break;

            default:
                printf("Invalid token (args)\n");
                print_token(&token);
                return -1;
                break;
        }

        token = peek_current_token(lexer);
    }

    token_t rparen = get_token(lexer); // consume RPAREN
    if(rparen.type != TOKEN_RPAREN){

        printf("ERROR missing RPAREN\n");

        return -1;
    }

    return arg_count;
}

int _build_func_or_coro(lexer_t *lexer, ir_state_t *ir_state, token_t *token, token_t *ident, bool coro){

    // create label for end of function
    int end_label = ir_create_label(ir_state);

    if(end_label < 0){

        return -1;
    }

    // insert jump to end_label
    ir_t ir_jump = {
        .type = IR_JUMP,
        .line_num = token->line_num,
        .args.jump = {
            .jump = IR_JUMP_ALWAYS,
            .label_index = end_label,
        },
    };

    ir_add(ir_state, &ir_jump);
    // this jumps over the function, allowing
    // straightline global code to execute
    // without falling into a function block.
    // this is a bit simpler than
    // collecting all of the global init code
    // together (which would require sorting
    // the IR code).
    // We just run straight through, and can
    // call already defined functions as needed.


    ir_t ir_func = {
        .type = IR_FUNC,
        .line_num = token->line_num,
        .args.func.ident_hash = hash_u32_data((uint8_t *)ident->str, ident->str_len),
        .data_type = TYPE_NONE,
    };

    // create label for start of function
    int start_label = ir_create_label(ir_state);

    if(start_label < 0){

        return -1;
    }

    // insert label for jump target to start of function
    ir_t ir_start_label = {
        .type = IR_LABEL,
        .line_num = token->line_num,
        .args.label = {
            .label_index = start_label,
        },
    };

    ir_func.args.func.label_index = start_label;
    ir_add(ir_state, &ir_func);

    int ir_index = array_get_count(ir_state->ir_list) - 1;
    
    ir_t *ir_func_ptr = ir_get_index(ir_state, ir_index);

    if(ir_func_ptr == NULL){

        return -1;
    }

    int arg_count = handle_func_args(lexer, ir_state, ident->str, ident->str_len);
    if(arg_count < 0){

        return -1;
    }

    // must "rehydrate" function pointer in IR list at any time array size may have changed!
    ir_func_ptr = ir_get_index(ir_state, ir_index);

    if(ir_func_ptr == NULL){

        return -1;
    }

    ir_func_ptr->args.func.arg_count = arg_count;

    if(coro){

        ir_func_ptr->args.func.is_coroutine = true;

        debug_printf("CORO: %.*s args: %d:\n", ident->str_len, ident->str, arg_count);
    }
    else{

        ir_func_ptr->args.func.is_coroutine = false;
        
        debug_printf("FUNC: %.*s args: %d:\n", ident->str_len, ident->str, arg_count);
    }

    // handle return, if present
    token_t next = peek_current_token(lexer);
    // print_token(&next);

    if(next.type == TOKEN_DECLARE){

        debug_printf("\tRETURNS: %.*s type %d\n", next.str_len, next.str, next.data_type);

        // return_type = next.info.data_type;
        ir_func_ptr->data_type = next.data_type;

        get_token(lexer); // consume token
    }

    ir_add(ir_state, &ir_start_label);

    // handle body
    if(handle_block(lexer, ir_state) < 0){

        return -1;
    }

    // must "rehydrate" function pointer in IR list at any time array size may have changed!
    ir_func_ptr = ir_get_index(ir_state, ir_index);

    if(ir_func_ptr == NULL){

        return -1;
    }

    // get last IR:
    const ir_t *ir_ptr = array_get_data(ir_state->ir_list);
    const ir_t *last_ir = &ir_ptr[array_get_count(ir_state->ir_list) - 1];

    // check if function requires a return:
    if(ir_func_ptr->data_type != TYPE_NONE){
        
        if(last_ir->type != IR_RETURN){

            printf("No return at end of function that returns a value\n");

            return -1;
        }
    }
    else{

        // function does not require a return value
        // we need to add one!
        if(last_ir->type != IR_RETURN){

            ir_t ir_return = {
                .type = IR_RETURN,
                .line_num = token->line_num,
                .args.ret.return_expressions = 0,
            };

            ir_add(ir_state, &ir_return);
        }
    }


    ir_t ir_end = {
        .type = IR_END_FUNC,
        .line_num = token->line_num,
    };

    ir_add(ir_state, &ir_end);

    debug_printf("END_FUNC\n");

    // insert label for jump target to end of function
    ir_t ir_end_label = {
        .type = IR_LABEL,
        .line_num = token->line_num,
        .args.label = {
            .label_index = end_label,
        },
    };

    ir_add(ir_state, &ir_end_label);

    return 0;
}


int handle_func(lexer_t *lexer, ir_state_t *ir_state){

    token_t token = get_token(lexer);
    // print_token(&token);
    
    if(token.type != TOKEN_FUNC){

        printf("ERROR (func)\n");
        return -1;
    }   

    token_t ident = get_token(lexer);
    // print_token(&ident);

    if(ident.type != TOKEN_CALL){ // CALL comes in with the name, but we don't actually call this time

        printf("ERROR (func)\n");
        return -1;
    }

    return _build_func_or_coro(lexer, ir_state, &token,  &ident, false);
}

int handle_coroutine(lexer_t *lexer, ir_state_t *ir_state){

    token_t token = get_token(lexer);
    // print_token(&token);
    
    if(token.type != TOKEN_COROUTINE){

        printf("ERROR (coroutine)\n");
        return -1;
    }   

    token_t ident = get_token(lexer);
    // print_token(&ident);

    if(ident.type != TOKEN_CALL){ // CALL comes in with the name, but we don't actually call this time

        printf("ERROR (coroutine)\n");
        return -1;
    }

    return _build_func_or_coro(lexer, ir_state, &token,  &ident, true);
}


int handle_block(lexer_t *lexer, ir_state_t *ir_state){

    debug_printf("OPEN_BLOCK\n");

    token_t token = get_token(lexer);

    if(token.type != TOKEN_LBRACE){

        printf("Missing LBRACE\n");
        return -1;
    }

    token_t next = peek_current_token(lexer);

    int status = 0;

    while((next.type != TOKEN_RBRACE) && (next.type != TOKEN_END)){

        // printf("BLOCK:\n");
        // print_token(&next);

        status = handle_statement(lexer, ir_state);

        if(status < 0){

            printf("ERROR (block)\n");
            break;
        }

        next = peek_current_token(lexer);
    }

    if(status < 0){
        
        return -1;
    }

    token_t rbrace = get_token(lexer); // consume RBRACE
    if(rbrace.type != TOKEN_RBRACE){

        printf("ERROR missing RBRACE\n");

        return -1;
    }

    debug_printf("CLOSE_BLOCK\n");

    return 0;
}


int handle_statement(lexer_t *lexer, ir_state_t *ir_state){
    
    debug_printf("STATEMENT\n");

    token_t token = peek_current_token(lexer);

    int status = 0;

    while((token.type != TOKEN_SEMICOLON) && 
          (token.type != TOKEN_RBRACE) &&
          (token.type != TOKEN_END)){

        // printf("BLOCK:\n");
        // print_token(&token);

        switch(token.type){
            case TOKEN_DECLARE:
                status = handle_declare(lexer, ir_state);
                break;

            case TOKEN_IDENT:
                status = handle_ident(lexer, ir_state);
                break;

            case TOKEN_ASSERT:
                status = handle_assert(lexer, ir_state);
                break;

            case TOKEN_RETURN:
                status = handle_return(lexer, ir_state);
                break;

            case TOKEN_YIELD:
                status = handle_yield(lexer, ir_state);
                break;

            case TOKEN_DELAY:
                status = handle_delay(lexer, ir_state);
                break;

            case TOKEN_RATE:
                status = handle_rate(lexer, ir_state);
                break;

            case TOKEN_PUBLISH:
                status = handle_publish(lexer, ir_state);
                break;

            case TOKEN_CALL:
                status = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);
                break;

            case TOKEN_FOR:
                status = handle_for(lexer, ir_state);
                break;

            case TOKEN_WHILE:
                status = handle_while(lexer, ir_state);
                break;

            case TOKEN_IF:
                status = handle_if(lexer, ir_state);
                break;

            case TOKEN_BREAK:
                status = handle_break(lexer, ir_state);
                break;

            case TOKEN_CONTINUE:
                status = handle_continue(lexer, ir_state);
                break;

            case TOKEN_FUNC:
                status = handle_func(lexer, ir_state);
                break;

            case TOKEN_COROUTINE:
                status = handle_coroutine(lexer, ir_state);
                break;

            case TOKEN_LBRACE:
            //     status = shunting_yard(lexer, ir_state, SY_MODE_NORMAL);
                debug_printf("LBRACE STATEMENT\n");
                return -1;
                break;

            default:
                printf("Invalid token (statement)\n");
                print_token(&token);
                return -1;
                break;
        }

        if(status < 0){

            printf("ERROR (statement)\n");
            break;
        }

        token = peek_current_token(lexer);
    }

    if(status < 0){
        
        return -1;
    }


    // check end marker
    if(token.type == TOKEN_END){
        
    }
    else if(token.type == TOKEN_RBRACE){

    }
    else if(token.type == TOKEN_SEMICOLON){

        // consume semicolon
        token = get_token(lexer);
    }
    else{

        printf("Missing semicolon at end of statement\n");
        return -1;
    }

    debug_printf("END_STATEMENT\n");

    return 0;
}

#include "unity.h"

void setUp(void) {

}

void tearDown(void) {

}


void test_declare_number(void){

    char* s = "number my_num;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}


void test_declare_number_assign(void){

    char* s = "number my_num = 1;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_declare_number_assign2(void){

    char* s = "number my_num = 1 + 2;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_expr_basic_add(void){

    char* s = "number a; a = 1 + 2;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_call_0(void){

    char* s = "number a; a = test_func();";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_call_1(void){

    char* s = "number a; a = test_func(1);";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_call_2(void){

    char* s = "number a; a = test_func(1, 2);";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_call_3(void){

    char* s = "number a; a = test_func(1 + 2, 3 + 4);";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_call_4(void){

    char* s = "number a; a = test_func(1, test_func2(1 + 2));";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_if(void){

    char* s = "if (1) { }";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_if_else(void){

    char* s = "if (1) { } else { }";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_if_else_if(void){

    char* s = "if (1) { } else if (0) { } else { }";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_for_loop(void){

    char* s = "fn loop(){ for a in 10 { } }";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_for_loop_with_expr(void){

    char* s = "fn loop(){ for a in 1 + 2 { } }";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_declare_array(void){

    char* s = "number my_array[4];";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_declare_array_expr(void){

    char* s = "number my_array[4] = 1;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_declare_array_expr2(void){

    char* s = "number my_array[4] = 1 + 2;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_declare_array_init(void){

    char* s = "number my_array[4] = [1, 2, 3, 4];";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_declare_array_init2(void){

    char* s = "number my_array[4] = (1, 2, 3, 4);";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_array_subscript(void){

    char* s = "number a; number my_array[4]; a = my_array[1];";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_array_subscript2(void){

    char* s = "number a; number my_array[4]; a = my_array[1 + 2];";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_declare_1(void){

    char* s = "fn func(){};";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_declare_2(void){

    char* s = "fn func() number{return 0;};";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_declare_3(void){

    char* s = "fn func(number a){};";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_declare_4(void){

    char* s = "fn func(number a, number b){};";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_declare_5(void){

    char* s = "fn func(number a) number{return 0;};";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_declare_6(void){

    char* s = "fn func(number a, number b) number{return 0;};";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_return_1(void){

    char* s = "fn func() number{return 1;}";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_return_2(void){

    char* s = "fn func2(number a, number b) number {return 0;} fn func() number{return func2(1, 2);}";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_return_3(void){

    char* s = "fn func2(number a, number b) number {return 0;} func2(3, 4); fn func() number{return func2(1, 2);}";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_declare(void){

    char* s = "object pixelarray{ number index; number count; bool reverse; };";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_declare_defaults(void){

    char* s = "object pixelarray{ number index = 1; number count = 2 + 3; bool reverse = false; };";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_load(void){

    char* s = "number a; a = pixels.hue;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_load2(void){

    char* s = "number a; a = pixels.hue[1];";

    TEST_ASSERT_EQUAL_INT(-1, parse_str(s, NULL, NULL));
}

void test_object_load3(void){

    char* s = "number a; a = pixels[2].hue;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_load4(void){

    char* s = "number a; a = pixels[2].hue[1];";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_store(void){

    char* s = "number a; pixels.hue = a;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_store2(void){

    char* s = "number a; pixels.hue[2] = a;";

    TEST_ASSERT_EQUAL_INT(-1, parse_str(s, NULL, NULL));
}

void test_object_store3(void){

    char* s = "number a; pixels[1].hue = a;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_object_store4(void){

    char* s = "number a; pixels[1].hue[2] = a;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_ref(void){

    char* s = "fn func(){} function my_func = func;";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_ref_call(void){

    char* s = "fn func(){} function my_func = func; my_func();";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void test_func_locals(void){

    char* s = "fn func(number a, number b) number{number c = 3; return a + b + c;} func(1, 2);";

    TEST_ASSERT_EQUAL_INT(0, parse_str(s, NULL, NULL));
}

void run_tests(void){

    printf("STARTING TESTS\n");    

    UNITY_BEGIN();

    printf("-------------\n");

    RUN_TEST(test_declare_number);
    RUN_TEST(test_declare_number_assign);
    RUN_TEST(test_declare_number_assign2);
    RUN_TEST(test_expr_basic_add);
    RUN_TEST(test_func_call_0);
    RUN_TEST(test_func_call_1);
    RUN_TEST(test_func_call_2);
    RUN_TEST(test_func_call_3);
    RUN_TEST(test_func_call_4);

    RUN_TEST(test_if);
    RUN_TEST(test_if_else);
    RUN_TEST(test_if_else_if);
    RUN_TEST(test_for_loop);
    RUN_TEST(test_for_loop_with_expr);

    RUN_TEST(test_declare_array);
    RUN_TEST(test_array_subscript);
    RUN_TEST(test_array_subscript);
    RUN_TEST(test_array_subscript2);

    RUN_TEST(test_func_declare_1);
    RUN_TEST(test_func_declare_2);
    RUN_TEST(test_func_declare_3);
    RUN_TEST(test_func_declare_4);
    RUN_TEST(test_func_declare_5);
    RUN_TEST(test_func_declare_6);

    RUN_TEST(test_func_return_1);
    RUN_TEST(test_func_return_2);
    RUN_TEST(test_func_return_3);

    RUN_TEST(test_object_load);
    RUN_TEST(test_object_load2);
    RUN_TEST(test_object_load3);
    RUN_TEST(test_object_load4);
    RUN_TEST(test_object_store);
    RUN_TEST(test_object_store2);
    RUN_TEST(test_object_store3);
    RUN_TEST(test_object_store4);

    RUN_TEST(test_func_ref);
    RUN_TEST(test_func_ref_call);
    RUN_TEST(test_func_locals);



    // these don't work yet:


    // RUN_TEST(test_declare_array_expr);
    // RUN_TEST(test_declare_array_expr2);
    // RUN_TEST(test_declare_array_init);
    // RUN_TEST(test_declare_array_init2);

    // RUN_TEST(test_object_declare);
    // RUN_TEST(test_object_declare_defaults);


   
    int status = UNITY_END();

    printf("STATUS: %d\n", status);
    printf("TESTS DONE\n");
}

void init_lexer(lexer_t *lexer, const char *fname, char *s){

    lexer->fname = fname;
    lexer->input = s;
    lexer->input_len = strlen(s);
    lexer->position = 0;
    lexer->current_line_number = 1;
}

void reset_lexer(lexer_t *lexer){

    lexer->position = 0;
    lexer->current_line_number = 1;
}

void list_tokens(lexer_t *lexer){

    reset_lexer(lexer);

    token_t token = get_token(lexer);

    do{

        print_token(&token);

        token = get_token(lexer);

    } while(token.type != TOKEN_END);

    print_token(&token);
}

int parse_str(char *s, const char *fname, vm_t *vm){ 

    debug_printf("Input len: %d\n", strlen(s));
        
    lexer_t lexer = {0};
    init_lexer(&lexer, fname, s);

    // list_tokens(&lexer);

    debug_printf("Parsing:\n");
        
    int status = 0;

    ir_state_t ir_state = {0};

    ir_init(&ir_state);

    #ifdef IR_ENABLE_DEBUG_LISTING
    ir_state.source_fname = fname;
    #endif

    ir_t ir_start = {
        .type = IR_START,
        .line_num = 0,
    };

    ir_add(&ir_state, &ir_start);


    ir_add_object(&ir_state, HASH32(pixelarray));
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(index), TYPE_NUMBER);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(count), TYPE_NUMBER);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(hue), TYPE_GFX16);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(sat), TYPE_GFX16);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(val), TYPE_GFX16);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(hsfade), TYPE_NUMBER);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(vfade), TYPE_NUMBER);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(size_x), TYPE_NUMBER);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(size_y), TYPE_NUMBER);
    ir_add_object_field(&ir_state, HASH32(pixelarray), HASH32(reverse), TYPE_NUMBER);
    ir_declare_object(&ir_state, HASH32(pixelarray), HASH32(pixels));

    ir_add_object(&ir_state, HASH32(keyvalue));
    ir_add_object_field(&ir_state, HASH32(keyvalue), HASH32(_any), TYPE_ANY);
    ir_declare_object(&ir_state, HASH32(keyvalue), HASH32(kv));
        
    token_t token = peek_current_token(&lexer);
    
    while(token.type != TOKEN_END){

        // status = handle_outer(&lexer, &ir_state);
        status = handle_statement(&lexer, &ir_state);

        if(status < 0){

            printf("ERROR (main)\n");
            break;
        }
        
        token = peek_current_token(&lexer);
    }
    
    debug_printf("DONE with parsing\n");

    if(status < 0){
        
        goto end;
    }


    ir_t ir_end = {
        .type = IR_END,
        .line_num = 0,
    };

    ir_add(&ir_state, &ir_end);

    ir_print_list(ir_state.ir_list);

    status = ir_pass_0(&ir_state);

    if(status < 0){
        
        goto end;
    }

    status = ir_pass_1(&ir_state);
    
    if(status < 0){
        
        goto end;
    }

    if(vm != NULL){

        vm_init(vm);

        if(ir_render_vm(&ir_state, vm) < 0){

            return -1;
        }
    }


end:
    ir_deinit(&ir_state);
   
    return status;
}

#endif
