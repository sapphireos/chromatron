
#ifndef IR_H
#define IR_H

#include <stdint.h>
#include <stdbool.h>
#include "hash32.h"
#include "datatypes.h"
#include "bytecode.h"
#include "program.h"

#define IR_MAX_LABELS	65535

#define IR_ENABLE_DEBUG_LISTING

typedef enum{
	IR_NOP,
	IR_DECLARE,
	// IR_ARRAY_SIZE,
	IR_BRACKET,
	IR_DOT,
	// IR_ARRAY_INDEX,
	IR_OBJ_FIELD,
	IR_OBJ_LOAD,
	IR_OBJ_STORE,
	IR_ASSIGN,
	IR_BINOP,
	IR_UNARYOP,
	IR_ASSIGN_OP,
	// IR_LVALUE,
	// IR_LBRACKET,
	// IR_RBRACKET,
	IR_PUSH_CONST,
	// IR_PUSH_IDENT,
	IR_HALT,			// halts VM
	IR_END_EXPR, 		// expr termination marker
	// IR_LOAD,
	// IR_STORE,
	IR_FUNC,
	IR_END_FUNC,
	IR_CALL,
	IR_RETURN,
	IR_YIELD,
	IR_DELAY,
	IR_RATE,
	IR_PUBLISH,
	IR_LABEL,
	IR_JUMP,
	IR_LOOP_INIT,
	IR_LOOP_EXIT,
	IR_IDENT,
	IR_LVALUE,
	IR_ASSERT,
	IR_START,	    	// start of IR stream
	IR_END,				// end of IR stream
} ir_type_t;

typedef enum{
	IR_OP_ADD,
	IR_OP_SUB,
	IR_OP_MUL,
	IR_OP_DIV,
	IR_OP_MOD,
	IR_OP_IN,
	IR_OP_COMPEQ,
	IR_OP_COMPNEQ,
	IR_OP_COMPLT,
	IR_OP_COMPLTE,
	IR_OP_COMPGT,
	IR_OP_COMPGTE,
	IR_OP_LOGICAL_AND,
	IR_OP_LOGICAL_OR,
} ir_operator_t;

// typedef enum{
// 	IR_UNARY_CONVERT_F16,
// } ir_unary_t;

typedef enum{
	IR_JUMP_ALWAYS,
	IR_JUMP_ZERO,
	IR_JUMP_NOT_ZERO,
	IR_JUMP_BREAK,
	IR_JUMP_CONTINUE,
} ir_jump_type_t;


typedef struct{
	uint32_t ident_hash;
	int8_t rank;
	bool is_ref;
} ir_arg_declare_t;

typedef struct{
	uint32_t ident_hash;
	uint8_t rank;
} ir_arg_array_t;

typedef struct{
	uint32_t ident_hash;
	bool is_vector;
	bool assume_resolved;
} ir_arg_ident_t;

typedef struct{
	uint8_t exprs;
} ir_arg_bracket_t;

typedef struct{
	uint32_t value;
} ir_arg_const_t;

typedef struct{
	data_type_t left_type;
	data_type_t right_type;
	ir_operator_t op;
} ir_arg_binop_t;

typedef struct{
	data_type_t left_type;
	data_type_t right_type;
	ir_operator_t op;
	uint32_t obj_hash;
	uint8_t obj_rank;
	uint32_t field_hash;
	uint8_t field_rank;
	bool is_obj;
	bool is_local;
	bool is_vector;
} ir_arg_assign_op_t;

typedef struct{
	uint32_t ident_hash;
	uint8_t arg_count;
	bool indirect;
	bool unused_return;
	bool discard;
} ir_arg_call_t;

typedef struct{
	uint8_t return_expressions;
} ir_arg_return_t;

typedef struct{
	uint32_t ident_hash;
	uint8_t arg_count;
	uint16_t label_index;
	bool is_coroutine;
} ir_arg_func_t;

typedef struct{
	ir_jump_type_t jump;
	int16_t label_index;
} ir_arg_jump_t;

typedef struct{
	uint16_t label_index;
} ir_arg_label_t;

typedef struct{
	uint32_t obj_hash;
	data_type_t data_type;
	uint32_t field_hash;
} ir_arg_obj_t;

typedef struct{
	uint32_t obj_hash;
	uint32_t field_hash;
} ir_arg_obj_access_t;

typedef struct{
	uint32_t obj_hash;
	uint8_t obj_rank;
	uint32_t field_hash;
	uint8_t field_rank;
	bool store;
} ir_arg_dot_t;

typedef struct{
	uint32_t obj_hash;
	uint8_t obj_rank;
	uint32_t field_hash;
	uint8_t field_rank;
	bool is_obj;
	bool is_local;
	bool is_vector;
	bool is_ref;
	uint8_t right_side_values;
	data_type_t source_type;
} ir_arg_assign_t;

typedef struct{
	uint32_t ident_hash;
} ir_arg_loop_init_t;

typedef struct{
	uint32_t ident_hash;
	uint16_t label_index;
} ir_arg_loop_exit_t;

typedef struct{
	ir_type_t type;
	uint16_t line_num; // useful for error reporting
	data_type_t data_type;
	union{
		ir_arg_declare_t declare;
		ir_arg_array_t array;
		ir_arg_ident_t ident;
		ir_arg_const_t constant;
		ir_arg_binop_t binop;
		ir_arg_assign_op_t assign_op;
		ir_arg_func_t func;
		ir_arg_call_t call;
		ir_arg_return_t ret;
		ir_arg_jump_t jump;
		ir_arg_label_t label;
		ir_arg_obj_t obj;
		ir_arg_obj_access_t obj_access;
		ir_arg_bracket_t bracket;
		ir_arg_dot_t dot;
		ir_arg_assign_t assign;
		ir_arg_loop_init_t loop_init;
		ir_arg_loop_exit_t loop_exit;
	} args;
	bool stack_empty;
} ir_t;

typedef struct{
	uint8_t opcode;
	int line_num;
} ir_debug_listing_t;

typedef struct{
	array_t ir_list;
	array_t opcode_list;
	array_t const_list;
	array_t jump_targets;
	array_t global_symbols;
	array_t functions;
	array_t objects;
	#ifdef IR_ENABLE_DEBUG_LISTING
	array_t debug_listing;
	const char *source_fname;
	#endif
	uint16_t global_count;
	uint16_t locals_count;
	int8_t func_index;
	uint16_t next_label;

	uint16_t label_entry_stack[32];
	uint8_t label_entry_index;

	uint16_t label_exit_stack[32];
	uint8_t label_exit_index;
	
	uint16_t ir_stack[64];
	uint8_t stack_index;
	
	uint8_t last_opcode;
	// bool accum_loaded;
	int current_line;
} ir_state_t;

void ir_init(ir_state_t *ir_state);
void ir_deinit(ir_state_t *ir_state);
int ir_get_state_size(ir_state_t *ir_state);
void ir_add(ir_state_t *ir_state, const ir_t *ir);
int ir_count(ir_state_t *ir_state);
ir_t* ir_get_index(ir_state_t *ir_state, int index);
void ir_peek(ir_state_t *ir_state, ir_t *ir);
// void ir_peek_prev1(ir_state_t *ir_state, ir_t *ir);
// void ir_peek_prev2(ir_state_t *ir_state, ir_t *ir);
void ir_print(const ir_t *ir);
void ir_print_list(array_t ir_list);
void ir_print_symbols(ir_state_t *ir_state);
void ir_print_objects(ir_state_t *ir_state);
void ir_print_functions(ir_state_t *ir_state);

int ir_type_size(data_type_t data_type);
int ir_obj_size(const obj_def_t *obj_def);

int ir_lookup_obj_def(ir_state_t *ir_state, uint32_t obj_hash, obj_def_t *obj_def);
int ir_lookup_obj_field(ir_state_t *ir_state, uint32_t obj_hash, uint32_t field_hash, obj_def_t *obj_def);

int ir_lookup_symbol(ir_state_t *ir_state, uint32_t hash, var_info_t *var_info);
int ir_lookup_symbol_ptr(ir_state_t *ir_state, uint32_t hash, var_info_t **var_info);
int ir_lookup_type(ir_state_t *ir_state, uint32_t hash, data_type_t *data_type);

int ir_add_global_var(ir_state_t *ir_state, uint32_t name_hash, data_type_t type);
int ir_add_local_var(ir_state_t *ir_state, uint32_t name_hash, data_type_t type, int8_t func_index);

int ir_add_object(ir_state_t *ir_state, uint32_t obj_hash);
int ir_add_object_field(ir_state_t *ir_state, uint32_t obj_hash, uint32_t field_hash, data_type_t data_type);
int ir_declare_object(ir_state_t *ir_state, uint32_t type_hash, uint32_t name_hash);

int ir_lookup_libfunc(ir_state_t *ir_state, uint32_t func_hash, func_def_t *func_def);
int ir_lookup_func(ir_state_t *ir_state, uint32_t func_hash, func_def_t *func_def);
int ir_lookup_func_by_index(ir_state_t *ir_state, int index, func_def_t *func_def);

int ir_add_func_local(ir_state_t *ir_state, int index);

int ir_create_label(ir_state_t *ir_state);
int ir_push_entry_label(ir_state_t *ir_state, uint16_t label);
int ir_peek_entry_label(ir_state_t *ir_state);
int ir_pop_entry_label(ir_state_t *ir_state);
int ir_push_exit_label(ir_state_t *ir_state, uint16_t label);
int ir_peek_exit_label(ir_state_t *ir_state);
int ir_pop_exit_label(ir_state_t *ir_state);

int ir_push_stack(ir_state_t *ir_state, const ir_t *ir);
int ir_pop_stack(ir_state_t *ir_state, ir_t *ir);
int ir_peek_stack(ir_state_t *ir_state, ir_t *ir);
int ir_stack_depth(ir_state_t *ir_state);

int ir_push_opcode(ir_state_t *ir_state, uint8_t opcode);
int ir_push_param(ir_state_t *ir_state, uint8_t param);
int ir_push_const(ir_state_t *ir_state, int32_t value);
int ir_create_jump_target(ir_state_t *ir_state, int16_t label_index);

void ir_print_listing(ir_state_t *ir_state);
void ir_print_debug_listing(ir_state_t *ir_state);

int ir_render_vm(ir_state_t *ir_state, vm_t *vm);

int ir_pass_0(ir_state_t *ir_state);
int ir_pass_1(ir_state_t *ir_state);
// int ir_pass_2(ir_state_t *ir_state);

#endif