
#ifndef PIXELARRAY_H
#define PIXELARRAY_H

#include <stdint.h>

#ifdef FX4_COMPILER
#include <stdbool.h>
#endif

typedef struct __attribute__((packed)){
	uint32_t name_hash;

	int16_t count;
	int16_t index;
    int16_t size_x;
    int16_t size_y;

    bool reverse;
    uint8_t vm_id;
} pixel_array_t;

void pixelarray_print(void);

int pixelarray_init(void);
void pixelarray_deinit(void);

void pixelarray_deref(void);

int pixelarray_create(uint32_t name_hash, int32_t vm_id);
void pixelarray_add(pixel_array_t *pixarray, int32_t vm_id);
void pixelarray_destroy(int32_t vm_id);
int pixelarray_count(void);
pixel_array_t* pixelarray_get_ptr(void);

int pixelarray_lookup(uint32_t name_hash, pixel_array_t *pixarray);
int pixelarray_lookup_ptr(uint32_t name_hash, pixel_array_t **pixarray);

int pixelarray_pstore_hue(uint32_t name_hash, uint16_t index, int32_t val);
int pixelarray_pstore_sat(uint32_t name_hash, uint16_t index, int32_t val);
int pixelarray_pstore_val(uint32_t name_hash, uint16_t index, int32_t val);
int pixelarray_pstore_hsfade(uint32_t name_hash, uint16_t index, int32_t val);
int pixelarray_pstore_vfade(uint32_t name_hash, uint16_t index, int32_t val);

int pixelarray_pstore_hue2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val);
int pixelarray_pstore_sat2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val);
int pixelarray_pstore_val2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val);
int pixelarray_pstore_hsfade2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val);
int pixelarray_pstore_vfade2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val);

int pixelarray_vstore_hue(uint32_t name_hash, int32_t val);
int pixelarray_vstore_sat(uint32_t name_hash, int32_t val);
int pixelarray_vstore_val(uint32_t name_hash, int32_t val);
int pixelarray_vstore_hsfade(uint32_t name_hash, int32_t val);
int pixelarray_vstore_vfade(uint32_t name_hash, int32_t val);

int pixelarray_vector_op_hue(uint32_t name_hash, uint8_t op, int32_t val);
int pixelarray_vector_op_sat(uint32_t name_hash, uint8_t op, int32_t val);
int pixelarray_vector_op_val(uint32_t name_hash, uint8_t op, int32_t val);

int pixelarray_pixel_op_hue(uint32_t name_hash, uint8_t op, int32_t index, int32_t val);
int pixelarray_pixel_op_sat(uint32_t name_hash, uint8_t op, int32_t index, int32_t val);
int pixelarray_pixel_op_val(uint32_t name_hash, uint8_t op, int32_t index, int32_t val);

int pixelarray_pixel_op_hue2(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t val);
int pixelarray_pixel_op_sat2(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t val);
int pixelarray_pixel_op_val2(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t val);


int pixelarray_vector_load(uint32_t name_hash, uint8_t op, int32_t *val);
int pixelarray_load(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t *val);

int pixelarray_load_obj(uint32_t obj_hash, uint32_t field_hash, int32_t *val);

int pixelarray_load_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	int32_t *val);

int pixelarray_store_obj(uint32_t obj_hash, uint32_t field_hash, int32_t val);

int pixelarray_store_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	int32_t val);

int pixelarray_op_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t op, 
	int32_t val);

int pixelarray_op_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes,
	uint8_t op, 
	int32_t val);

#endif
