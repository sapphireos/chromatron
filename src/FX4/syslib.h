
#ifndef SYSLIB_H
#define SYSLIB_H

#include "bytecode.h"
#include "program.h"

// typedef struct{
// 	char *object_name;
// 	char *field_name;
// 	data_type_t data_type;
// 	int (*handler)(
// 		uint32_t obj_hash,
// 		uint8_t obj_rank_count,
// 		const uint16_t *obj_ranks,
// 		uint32_t field_hash,
// 		uint8_t field_rank_count,
// 		const uint16_t *field_ranks,
// 		int32_t *value,
// 		bool store);
// } sys_obj_t;

typedef struct{
	char *func_name;
	int32_t (*func)(vm_t *vm, uint8_t arg_count, int32_t *args);
	data_type_t return_type;
	uint8_t arg_count_min;
	uint8_t arg_count_max;
	data_type_t args[MAX_FUNC_ARGS];
} sys_func_t;


int syslib_lookup_func(uint32_t func_hash);
int syslib_lookup_func_def(uint32_t func_hash, func_def_t *func_def);
int syslib_call_func(vm_t *vm, uint32_t func_hash, uint8_t arg_count, int32_t *args, int32_t *ret_val);
// int syslib_lookup_obj(uint32_t obj_hash, uint32_t field_hash);

// int syslib_handle_obj(
// 		uint32_t obj_hash,
// 		uint8_t obj_rank_count,
// 		uint16_t *obj_ranks,
// 		uint32_t field_hash,
// 		uint8_t field_rank_count,
// 		uint16_t *field_ranks,
// 		int32_t *value,
// 		bool store);

// int syslib_obj_size(uint32_t obj_hash);

int syslib_create_obj(uint32_t obj_hash, uint32_t name_hash, int32_t vm_id);

int syslib_load_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t datatype,
	int32_t *val);

int syslib_load_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	int32_t *val);

int syslib_store_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t datatype,
	int32_t val);

int syslib_store_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	uint8_t datatype,
	int32_t val);

int syslib_move_obj(
	uint32_t target, 
	uint32_t source);

int syslib_op_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t op, 
	int32_t val);

int syslib_op_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes,
	uint8_t op, 
	int32_t val);

#endif


