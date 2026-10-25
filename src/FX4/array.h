#ifndef ARRAY_H
#define ARRAY_H

#include <string.h>
#include <stdint.h>

#ifndef FX4_COMPILER
#include "memory.h"
#endif

typedef struct{
	uint16_t item_size;
	uint16_t capacity;
	uint16_t count;
	uint16_t padding;
} array_hdr_t;

#ifdef FX4_COMPILER
typedef size_t array_t;
#else
typedef mem_handle_t array_t;
#endif

array_t array_alloc(uint16_t item_size, uint32_t capacity);
array_hdr_t* array_get_header(array_t array);
void* array_get_data(array_t array);
int array_get_count(array_t array);
array_t array_grow(array_t array, uint32_t new_capacity);
void array_free(array_t array);
void array_add(array_t *array, const void *data);
int array_get_used_space(array_t array);
int array_get_data_space(array_t array);

#endif
