
#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdint.h>
#include "hash32.h"
// #include "lexer.h"
#include "datatypes.h"
// #include "bytecode.h"
#include "array.h"

#ifdef FX4_COMPILER
#include <stdbool.h>
#endif

#define MAX_RANKS			4
#define MAX_OBJECT_FIELDS	16
#define MAX_FUNC_ARGS		8
#define MAX_CALL_DEPTH		8
#define MAX_COROUTINES		16


#ifndef cnt_of_array
	// Count of array macro
	#define cnt_of_array( array ) (int)( sizeof( array ) / sizeof( array[0] ) )
#endif

typedef struct{
	data_type_t type;
	bool is_ref;
	uint32_t name_hash;
	uint8_t array_rank;
	uint8_t obj_index;
	int8_t func_index;
	int16_t addr;
} var_info_t;

typedef struct{
    uint32_t name_hash;
	uint8_t field_count;
	var_info_t fields[MAX_OBJECT_FIELDS];
} obj_def_t;

typedef struct{
	uint32_t name_hash;
	uint8_t arg_count_min;
	uint8_t arg_count_max;
	uint8_t arg_count;
	// var_info_t args[MAX_FUNC_ARGS];
	data_type_t return_type;
	uint16_t label_index;
	uint16_t locals_index;
	uint16_t locals_size;
	uint16_t instruction_offset;
	uint16_t instruction_count;
	bool is_coroutine;
} func_def_t;

#define PARSER_OUTPUT_STACK_SIZE	64
#define PARSER_OPERATOR_STACK_SIZE	32
#define IR_STACK_SIZE				16



#endif
