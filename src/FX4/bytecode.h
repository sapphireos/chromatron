
#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include "array.h"
#include "program.h"

#ifndef FX4_COMPILER
#include "sapphire.h"
#endif

// array meta in global
typedef struct __attribute__((packed)){
	uint16_t array_index;
	uint16_t size;
} array_meta_t;

// array meta in array space
typedef struct __attribute__((packed)){
	uint8_t strides[MAX_RANKS];
} array_meta2_t;

typedef struct __attribute__((packed)){
	uint16_t label_index;
	uint16_t locals_index;
} func_meta_t;

typedef struct{
// typedef struct __attribute__((packed)){ // !!! DO NOT PACK! Alignment issues on ESP8266!
	uint16_t current_pc;
	uint8_t locals_count;
	uint64_t tick;
} coroutine_state_t;

typedef struct __attribute__((packed)){ // !!! See above!  Do not access via pointer!  This is for on-disk storage.
	uint16_t current_pc;
	uint8_t index;
	uint8_t locals_count;
	uint64_t tick;
} coroutine_state_serialized_t;


#ifdef FX4_COMPILER
typedef size_t coroutine_handle_t;
#else
typedef mem_handle_t coroutine_handle_t;
#endif

// typedef struct __attribute__((packed)){
// 	uint32_t obj_hash;
// 	uint32_t field_hash;
// 	uint16_t strides[MAX_RANKS];
// } obj_meta_t;


#define OPCODE_TRAP	        		0x00
#define OPCODE_PUSH_CONST			0x01
#define OPCODE_ADD					0x02
#define OPCODE_SUB					0x03
#define OPCODE_MUL					0x04
#define OPCODE_DIV					0x05
#define OPCODE_PRINT				0x06
#define OPCODE_HALT 				0x07
#define OPCODE_STORE_GLOBAL			0x08
#define OPCODE_LOAD_GLOBAL			0x09
#define OPCODE_ARRAY_SIZE			0x0A
// #define OPCODE_ARRAY_INDEX			0x0B // unused
#define OPCODE_STORE_ARRAY			0x0C
#define OPCODE_LOAD_ARRAY			0x0D
#define OPCODE_CALL_NATIVE			0x0E
#define OPCODE_CALL					0x0F
#define OPCODE_RETURN				0x10
#define OPCODE_FUNC					0x11
#define OPCODE_END_FUNC				0x12
#define OPCODE_JUMP					0x13
#define OPCODE_JUMP_Z				0x14
#define OPCODE_DISCARD				0x15
#define OPCODE_LOAD_RETURN			0x16
#define OPCODE_RETURN_VOID			0x17
#define OPCODE_LOAD_OBJ				0x18
#define OPCODE_STORE_OBJ			0x19
#define OPCODE_LOAD_OBJ_N			0x1A
#define OPCODE_STORE_OBJ_N			0x1B
#define OPCODE_COMPLT				0x1C
#define OPCODE_JUMP_NZ				0x1D
#define OPCODE_CONV0_I32_TO_F16		0x1E
#define OPCODE_CONV1_I32_TO_F16		0x1F
#define OPCODE_CONV0_F16_TO_I32		0x20
#define OPCODE_CONV1_F16_TO_I32		0x21
#define OPCODE_COMPLTE 				0x22
#define OPCODE_LOOP 				0x23
// #define OPCODE_ZERO 				0x24 // unused
#define OPCODE_COMPGT 				0x25
#define OPCODE_COMPGTE 				0x26
#define OPCODE_ICALL 				0x27
#define OPCODE_OBJ_SIZE				0x28
#define OPCODE_LOAD_LOCAL			0x29
#define OPCODE_STORE_LOCAL			0x2A
#define OPCODE_CORO_INIT			0x2B
#define OPCODE_CORO_START			0x2C
#define OPCODE_CORO_STOP			0x2D
#define OPCODE_ZERO_LOCAL			0x2E
#define OPCODE_COMPEQ 				0x2F
#define OPCODE_COMPNEQ 				0x30
#define OPCODE_ASSERT 				0x31
// #define OPCODE_RESUME 				0x32 // unused
#define OPCODE_CORO_EXIT			0x33
#define OPCODE_YIELD  				0x34
#define OPCODE_DELAY  				0x35
#define OPCODE_ADD_ASSIGN  			0x36
#define OPCODE_MOD 		 			0x37
#define OPCODE_ADD_ASSIGN_LOCAL		0x38
#define OPCODE_SUB_ASSIGN  			0x39
#define OPCODE_SUB_ASSIGN_LOCAL		0x3A
#define OPCODE_STORE_VECTOR			0x3B
#define OPCODE_ASSIGN_OP_ARRAY		0x3C
#define OPCODE_ASSIGN_OP_VECTOR		0x3D
#define OPCODE_MUL_F16				0x3E
#define OPCODE_VSTORE_HUE			0x3F
#define OPCODE_PSTORE_HUE			0x40
#define OPCODE_ASSIGN_OP_OBJ        0x41
#define OPCODE_ASSIGN_OP_OBJ_N      0x42
#define OPCODE_RATE      			0x43
#define OPCODE_DIV_F16				0x44
#define OPCODE_PSTORE_SAT			0x45
#define OPCODE_PSTORE_VAL			0x46
#define OPCODE_PSTORE_HSFADE		0x47
#define OPCODE_PSTORE_VFADE			0x48
#define OPCODE_VSTORE_SAT			0x49
#define OPCODE_VSTORE_VAL			0x4A
#define OPCODE_VSTORE_HSFADE		0x4B
#define OPCODE_VSTORE_VFADE			0x4C
#define OPCODE_PIXEL_VECTOR_OP_HUE	0x4D
#define OPCODE_PIXEL_OP_HUE			0x4E
#define OPCODE_PUSH_CONST2			0x4F
#define OPCODE_PIXEL_VECTOR_LOAD	0x50
#define OPCODE_PIXEL_LOAD			0x51
// #define OPCODE_LOAD_ACCUM			0x52 // unused
#define OPCODE_SWAP					0x53
#define OPCODE_MOVE_VECTOR			0x54
#define OPCODE_MOVE_OBJ	  			0x55
#define OPCODE_LOAD_REF	  			0x56
#define OPCODE_PSTORE_HUE2			0x57
#define OPCODE_PSTORE_SAT2			0x58
#define OPCODE_PSTORE_VAL2			0x59
#define OPCODE_PSTORE_HSFADE2		0x5A
#define OPCODE_PSTORE_VFADE2		0x5B
#define OPCODE_PIXEL_LOAD2			0x5C
#define OPCODE_PIXEL_VECTOR_OP_SAT	0x5D
#define OPCODE_PIXEL_OP_SAT			0x5E
#define OPCODE_PIXEL_VECTOR_OP_VAL	0x5F
#define OPCODE_PIXEL_OP_VAL			0x60
#define OPCODE_PIXEL_OP_HUE2		0x61
#define OPCODE_PIXEL_OP_SAT2		0x62
#define OPCODE_PIXEL_OP_VAL2		0x63
#define OPCODE_PUBLISH				0x64
#define OPCODE_MUL_ASSIGN  			0x65
#define OPCODE_MUL_ASSIGN_LOCAL		0x66
#define OPCODE_DIV_ASSIGN  			0x67
#define OPCODE_DIV_ASSIGN_LOCAL		0x68
#define OPCODE_MOD_ASSIGN  			0x69
#define OPCODE_MOD_ASSIGN_LOCAL		0x6A
#define OPCODE_LOGICAL_AND			0x6B
#define OPCODE_LOGICAL_OR			0x6C

// these map to the pixelarray fields
#define PIXEL_ATTR_COUNT			0
#define PIXEL_ATTR_INDEX			1
#define PIXEL_ATTR_SIZE_X			2
#define PIXEL_ATTR_SIZE_Y			3
#define PIXEL_ATTR_REVERSE			4

#define PIXEL_ATTR_HUE				100
#define PIXEL_ATTR_SAT				101
#define PIXEL_ATTR_VAL				102
#define PIXEL_ATTR_HSFADE			103
#define PIXEL_ATTR_VFADE			104


#define ASSIGN_OP_NONE 				0
#define ASSIGN_OP_ADD 				1
#define ASSIGN_OP_SUB 				2
#define ASSIGN_OP_MUL 				3
#define ASSIGN_OP_DIV 				4
#define ASSIGN_OP_MOD 				5


#define VM4_STATUS_OK 					0
#define VM4_STATUS_HALT                  1

#define VM4_STATUS_ERROR 				-1
#define VM4_STATUS_TRAP  				-2
#define VM4_STATUS_ASSERTION 			-3
#define VM4_STATUS_CYCLE_LIMIT 			-4
#define VM4_STATUS_BAD_STACK 			-5
#define VM4_STATUS_STACK_OVER_UNDER		-6
#define VM4_STATUS_STACK_NOT_EMPTY		-7

#define VM4_STATUS_BAD_CF_META			-10
#define VM4_STATUS_ERR_MOVE_OBJ			-11
#define VM4_STATUS_ERR_OBJ_SIZE			-12
#define VM4_STATUS_ERR_YIELD			-13
#define VM4_STATUS_ERR_DELAY			-14
#define VM4_STATUS_ERR_RATE				-15
#define VM4_STATUS_ERR_LOAD_OBJ			-16
#define VM4_STATUS_ERR_STORE_OBJ		-17
#define VM4_STATUS_ERR_LOAD_OBJ_N		-18
#define VM4_STATUS_ERR_STORE_OBJ_N		-19
#define VM4_STATUS_ERR_ASSIGN_OP_OBJ	-20
#define VM4_STATUS_ERR_ASSIGN_OP_OBJ_N  -21
#define VM4_STATUS_ERR_PUBLISH 		    -22
#define VM4_STATUS_ERR_ASSIGN_OP_VECTOR -23
#define VM4_STATUS_ERR_CALL_DEPTH 		-24
#define VM4_STATUS_ERR_ICALL_DEPTH 		-25
#define VM4_STATUS_ERR_FUNC 			-26
#define VM4_STATUS_ERR_ASSIGN_OP_ARRAY  -27

#define VM4_STATUS_NOT_INITIALIZED		-100
#define VM4_STATUS_FILE_NOT_FOUND		-101
#define VM4_STATUS_BAD_MAGIC			-102
#define VM4_STATUS_WRONG_VERSION		-103
#define VM4_STATUS_ARRAY_ALLOC_FAIL		-104
#define VM4_STATUS_BAD_INS_LEN			-105
#define VM4_STATUS_MAX_COROUTINES		-106
#define VM4_STATUS_BAD_COROUTINE_INDEX	-107
#define VM4_STATUS_BAD_COROUTINE_PTR	-108
#define VM4_STATUS_COROUTINE_FAIL		-109
#define VM4_STATUS_STRING_NOT_FOUND		-110

#define VM4_STATUS_NOT_RUNNING          -127

#define VM4_STATUS_COROUTINE_EXITED		100
#define VM4_STATUS_COROUTINE_YIELDED	101
#define VM4_STATUS_COROUTINE_DELAYED	102
#define VM4_STATUS_NO_COROUTINE			103
#define VM4_STATUS_COROUTINE_NOT_READY 	104
#define VM4_STATUS_NO_READY_COROUTINE 	105


#define VM4_STACK_SIZE		32

typedef struct __attribute__((packed)){
	uint16_t return_addr;
	int32_t *locals;
} call_frame_t;

typedef struct __attribute__((packed)){
	uint16_t instruction_offset;
} jump_target_t;

#ifdef FX4_COMPILER
typedef struct{
#else
// typedef struct  __attribute__((packed)){
typedef struct{ // DO NOT PACK! there will be alignment issues with array_t!!!
#endif
	// MUST BE 32 bit aligned on ESP8266!
    uint64_t rng_seed;

    uint32_t program_name_hash;
    uint32_t file_hash;

	int32_t stack[VM4_STACK_SIZE];
	uint32_t sp; // stack pointer

	array_t globals_list;
	array_t locals_list;
	array_t array_list;
	array_t object_list;
	array_t constants_list;
	array_t jump_targets;

	call_frame_t call_stack[MAX_CALL_DEPTH];
	uint32_t call_depth;

	uint64_t current_tick;
	uint64_t frame_number;

	uint32_t strings_offset;

	array_t instruction_list;
	uint32_t pc; // program counter
	uint32_t cycle_count;
	
	coroutine_handle_t coroutines[MAX_COROUTINES];

	int32_t return_value;
	uint32_t vm_id;
} vm_t;



#define BYTECODE_MAGIC				0x42345846 // FX4B in hex
#define BYTECODE_VERSION			3

typedef struct __attribute__((packed)){
	uint32_t magic;
	uint8_t version;
	uint8_t padding;
	uint16_t instruction_len;
	uint16_t globals_len;
	uint16_t locals_len;
	uint16_t array_len;
	uint16_t object_len;
	uint16_t constants_len;
	uint16_t jump_targets_len;
	uint16_t strings_len;
	uint16_t coroutines_len;
	uint16_t pixelarray_len;
	// sync/runtime data
	uint64_t rng_seed;
	uint64_t frame_number;
	uint64_t current_tick;

	// data follows
} vm_stream_header_t;



void vm_init(vm_t *vm);
void vm_deinit(vm_t *vm);

int vm_get_state_size(vm_t *vm);

int vm_get_instruction_size(uint8_t opcode);

void print_vm_state(vm_t *vm);
void print_vm_listing(vm_t *vm);
char* vm_get_opcode_str(uint8_t opcode);

void vm_print_listing(
    uint8_t *instructions, 
    uint16_t count, 
    int32_t *constants, 
    uint16_t const_count,
    jump_target_t *jump_targets,
    uint16_t jump_count);

int vm_run_instructions(vm_t *vm, int coroutine_index, uint32_t call_func_meta);

int vm_run_for_time(vm_t *vm, uint32_t increment, uint64_t stop_tick);
int vm_run_tick(vm_t *vm, uint64_t current_tick);
int vm_run_coroutine_tick(vm_t *vm, uint8_t index);

int vm_get_coroutine_count(vm_t *vm);
int vm_run_coroutines(vm_t *vm);
int vm_run_coroutine(vm_t *vm, uint8_t index);

int vm_get_serialized_size(vm_t *vm);
int vm_serialize(vm_t *vm, char *fname);
int vm_deserialize(vm_t *vm, char *fname);

int32_t* vm_get_global(vm_t *vm, uint16_t index);
int32_t* vm_get_array(vm_t *vm, uint16_t index);

int vm_lookup_hash(uint32_t hash, char *s, int max_len);

#endif
