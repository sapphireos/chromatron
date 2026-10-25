
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array.h"


#ifdef FX4_COMPILER
#include <assert.h>

#define ASSERT assert

#else
#include "memory.h"
#include "logging.h"
#endif


/*

Dynamic array:

Allocate storage with malloc.
Stores the allocated size in the first 2 bytes
of the storage space, then returns a pointer
to the location after the size.

Items are added to the array via interface.
Array automatically grows.

Handle, not pointer?



*/

array_t array_alloc(uint16_t item_size, uint32_t capacity){

	// enforce minimum size, cannot be zero:
	if(capacity == 0){

		capacity = 1;
	}

	uint32_t mem_size = item_size * capacity;

	#ifdef FX4_COMPILER
	array_hdr_t *ptr = malloc(sizeof(array_hdr_t) + mem_size);

	if(ptr == NULL){

		printf("******************** ARRAY ALLOC FAIL!\n");

		exit(EXIT_FAILURE);

		return 0;
	}
	#else
	mem_handle_t h = mem2_h_alloc2(sizeof(array_hdr_t) + mem_size, MEM_TYPE_VM_ARRAY);
	if(h <= 0){

		log_v_error_P(PSTR("alloc fail"));

		return 0;
	}

	array_hdr_t *ptr = mem2_vp_get_ptr_fast(h);
	#endif

	ptr->item_size = item_size;
	ptr->capacity = capacity;
	ptr->count = 0;

	#ifdef FX4_COMPILER
	return (array_t)ptr;
	#else
	return h;
	#endif
}

array_hdr_t* array_get_header(array_t array){

	#ifdef FX4_COMPILER
	array_hdr_t *ptr = (array_hdr_t *)array;

	if(ptr == NULL){

		return NULL;
	}
	#else
	array_hdr_t *ptr = (array_hdr_t *)mem2_vp_get_ptr(array);
	#endif

	return ptr;
}

void* array_get_data(array_t array){

	#ifdef FX4_COMPILER
	array_hdr_t *ptr = (array_hdr_t *)array;

	if(ptr == NULL){

		return NULL;
	}
	#else
	array_hdr_t *ptr = array_get_header(array);
	#endif

	return (void *)(ptr + 1);
}

int array_get_count(array_t array){

	#ifdef FX4_COMPILER
	const array_hdr_t *hdr = array_get_header(array);

	if(hdr == NULL){

		return 0;
	}
	#else
	const array_hdr_t *hdr = array_get_header(array);
	#endif

	return hdr->count;
}

array_t array_grow(array_t array, uint32_t new_capacity){

	// printf("ARRAY GROW: %d\n", array);

	const array_hdr_t *hdr = array_get_header(array);

	if(hdr == NULL){

		return 0;
	}

	if(new_capacity < hdr->capacity){

		printf("ERROR cannot decrease array size\n");
		return 0;
	}

	array_t new_array = array_alloc(hdr->item_size, new_capacity);

	#ifdef FX4_COMPILER
	// deals with CWE-401 warning with GCC -fanalzyer
	ASSERT(new_array > 0);
	#endif

	if(new_array == 0){

		#ifdef FX4_COMPILER

		array_free(array);

		exit(EXIT_FAILURE);
		#endif

		return array;
	}

	const void *old_data = array_get_data(array);
	void *new_data = array_get_data(new_array);

	memcpy(new_data, old_data, hdr->count * hdr->item_size);

	array_hdr_t *new_hdr = array_get_header(new_array);
	new_hdr->count = hdr->count;

	array_free(array);

	return new_array;

}

void array_free(array_t array){

	#ifdef FX4_COMPILER
	
	const array_hdr_t *hdr = array_get_header(array);

	if(hdr == NULL){

		return;
	}

	free((void *)hdr);
	
	#else

	mem2_v_free(array);

	#endif
}

void array_add(array_t *array, const void *data){

	array_hdr_t *hdr = array_get_header(*array);

	if(hdr == NULL){

		#ifdef FX4_COMPILER
		printf("Invalid array\n");
		exit(EXIT_FAILURE);
		#endif

		return;
	}
	
	// ASSERT(hdr->count <= hdr->capacity)

	if(hdr->count == hdr->capacity){

		*array = array_grow(*array, hdr->capacity + 8);
		
		if(*array == 0){

			return;
		}

		hdr = array_get_header(*array); // update header pointer!
	}

	uint8_t *array_ptr = array_get_data(*array);
	void *ptr = (void *)(array_ptr + hdr->item_size * hdr->count);
		
	memcpy(ptr, data, hdr->item_size);
	
	hdr->count++;
}

int array_get_used_space(array_t array){

	const array_hdr_t *hdr = array_get_header(array);

	ASSERT(hdr != NULL);

	return sizeof(array_hdr_t) + hdr->capacity * hdr->item_size;
}

int array_get_data_space(array_t array){

	const array_hdr_t *hdr = array_get_header(array);

	ASSERT(hdr != NULL);

	return hdr->count * hdr->item_size;
}
