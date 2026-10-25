
#include <stdio.h>
#include "hash32.h"
#include "array.h"
#include "pixelarray.h"
#include "syslib.h"
#include "debug_printf.h"

#ifndef FX4_COMPILER
#include "gfx_lib.h"
#include "catbus_common.h"
#endif

#if defined(ENABLE_GFX) || defined(FX4_COMPILER)

#ifdef FX4_COMPILER
// main pixels array
#define PIXEL_COUNT 8
static uint16_t pixels_hue[PIXEL_COUNT];
static uint16_t pixels_sat[PIXEL_COUNT];
static uint16_t pixels_val[PIXEL_COUNT];
static uint16_t pixels_vfade[PIXEL_COUNT];
static uint16_t pixels_hsfade[PIXEL_COUNT];

static uint16_t pix_count = PIXEL_COUNT;

void pixelarray_print(void){

	debug_printf("Pixels:");

	debug_printf("\n  hue:   ");
	for(int i = 0; i < pix_count; i++){

		debug_printf("%5d ", pixels_hue[i]);
	}

	debug_printf("\n  sat:   ");
	for(int i = 0; i < pix_count; i++){

		debug_printf("%5d ", pixels_sat[i]);
	}

	debug_printf("\n  val:   ");
	for(int i = 0; i < pix_count; i++){

		debug_printf("%5d ", pixels_val[i]);
	}

	debug_printf("\n  vfade: ");
	for(int i = 0; i < pix_count; i++){

		debug_printf("%5d ", pixels_vfade[i]);
	}

	debug_printf("\n  hsfade:");
	for(int i = 0; i < pix_count; i++){

		debug_printf("%5d ", pixels_hsfade[i]);
	}

	debug_printf("\n");
}

#else

static uint16_t *pixels_hue;
static uint16_t *pixels_sat;
static uint16_t *pixels_val;
static uint16_t *pixels_vfade;
static uint16_t *pixels_hsfade;

static uint16_t *pixels_hue_step;
static uint16_t *pixels_sat_step;
static uint16_t *pixels_val_step;

// #define pixels_hue _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_HUE)
// #define pixels_sat _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_SAT)
// #define pixels_val _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_VAL)
// #define pixels_hsfade _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_HS_FADE)
// #define pixels_vfade _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_V_FADE)

// #define pixels_hue_step _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_HUE_STEP)
// #define pixels_sat_step _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_SAT_STEP)
// #define pixels_val_step _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_VAL_STEP)

#endif

static uint16_t get_pix_count(void);
static uint16_t get_pix_size_x(void);
static uint16_t get_pix_size_y(void);
static int _pixelarray_lookup_ptr(uint32_t name_hash, pixel_array_t **pixarray);
static int _set_field(pixel_array_t *pixarray, uint32_t field_hash, int32_t value);
static int _get_field(const pixel_array_t *pixarray, uint32_t field_hash, int32_t *value);

static array_t pixel_array;
static uint8_t pixarray_count;
static pixel_array_t *pixarray_ptr;


int pixelarray_init(void){

	pixel_array = array_alloc(sizeof(pixel_array_t), 1);	

	#ifdef FX4_COMPILER
	
	// // set up a predefined custom array for testing:
	// pixelarray_create(HASH32(my_pixels), -1);

	// pixel_array_t *pixarray;
	// if(_pixelarray_lookup_ptr(HASH32(my_pixels), &pixarray) == 0){

	// 	pixarray->index = 2;
	// 	pixarray->count = 4;
	// }
	
	// return pixelarray_create(HASH32(pixels), -1);

	return 0;
	
	#else

	// SapphireOS init

	pixels_hue 		= _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_HUE);
	pixels_sat 		= _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_SAT);
	pixels_val 		= _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_VAL);
	pixels_hsfade 	= _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_HS_FADE);
	pixels_vfade 	= _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_V_FADE);

	pixels_hue_step = _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_HUE_STEP);
	pixels_sat_step = _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_SAT_STEP);
	pixels_val_step = _gfx_u16p_get_array_ptr(PIX_ARRAY_ATTR_VAL_STEP);

	#endif

	return 0;
}

void pixelarray_deinit(void){

	array_free(pixel_array);
	pixel_array = 0;
	pixarray_ptr = NULL;
	pixarray_count = 0;
}

void pixelarray_deref(void){

	pixarray_ptr = (pixel_array_t *)array_get_data(pixel_array);
	pixarray_count = array_get_count(pixel_array);
}

int pixelarray_create(uint32_t name_hash, int32_t vm_id){

	// check if this array already exists
	if(pixelarray_lookup(name_hash, NULL) == 0){

		return 0;
	}

	pixel_array_t pixels = {
		.name_hash=name_hash,
		.count=get_pix_count(),
		.index=0,
		.reverse=false,
		.size_x=get_pix_size_x(),
		.size_y=get_pix_size_y(),
		.vm_id=vm_id,
	};

	// look for a free slot
	for(int i = 0; i < pixarray_count; i++){

		if(pixarray_ptr[i].name_hash == 0){

			pixarray_ptr[i] = pixels;

			goto done;
		}
	}

	// no slots, add one
	array_add(&pixel_array, &pixels);
	
	#ifndef FX4_COMPILER
	log_v_debug_P(PSTR("Create pixelarray: 0x%08x"), name_hash);
	#endif

done:
	pixelarray_deref();

	return 0;
}

void pixelarray_add(pixel_array_t *pixarray, int32_t vm_id){

	pixarray->vm_id = vm_id;

	pixel_array_t *existing_pixarray = NULL;

	// check if this array already exists
	if(_pixelarray_lookup_ptr(pixarray->name_hash, &existing_pixarray) == 0){

		// this pixelarray is already loaded

		// overwrite state
		*existing_pixarray = *pixarray;

		// and return we are done!
		return;
	}

	#ifndef FX4_COMPILER
	log_v_debug_P(PSTR("Add pixelarray: 0x%08x"), pixarray->name_hash);
	#endif

	// look for a free slot
	for(int i = 0; i < pixarray_count; i++){

		if(pixarray_ptr[i].name_hash == 0){

			pixarray_ptr[i] = *pixarray;

			return;
		}
	}

	// no slots, add one
	array_add(&pixel_array, pixarray);
}

void pixelarray_destroy(int32_t vm_id){

	for(int i = 0; i < pixarray_count; i++){

		if(pixarray_ptr[i].vm_id == vm_id){

			pixarray_ptr[i].name_hash = 0;	
		}
	}
}

int pixelarray_count(void){

	return array_get_count(pixel_array);
}

pixel_array_t* pixelarray_get_ptr(void){

	return (pixel_array_t *)array_get_data(pixel_array);
}

static inline __attribute__((always_inline)) uint16_t get_pix_count(void){

	#ifdef FX4_COMPILER
	return pix_count;
	#else
	return gfx_u16_get_pix_count();
	#endif
}

static inline __attribute__((always_inline)) uint16_t get_pix_size_x(void){

	#ifdef FX4_COMPILER
	return pix_count;
	#else
	return gfx_u16_get_pix_size_x();
	#endif
}

static inline __attribute__((always_inline)) uint16_t get_pix_size_y(void){

	#ifdef FX4_COMPILER
	return pix_count;
	#else
	return gfx_u16_get_pix_size_y();
	#endif
}

static inline __attribute__((always_inline)) bool get_transpose(void){

	#ifdef FX4_COMPILER
	return false;
	#else
	return gfx_b_get_transpose();
	#endif
}

static inline __attribute__((always_inline)) bool get_interleave_x(void){

	#ifdef FX4_COMPILER
	return false;
	#else
	return gfx_b_get_interleave_x();
	#endif
}

static inline __attribute__((always_inline)) bool get_invert_x(void){

	#ifdef FX4_COMPILER
	return false;
	#else
	return gfx_b_get_invert_x();
	#endif
}

static inline __attribute__((always_inline)) int _calc_index(const pixel_array_t *pixarray, uint16_t x, uint16_t y){

	if( get_transpose() ){

        uint16_t temp = y;
        y = x;
        x = temp;
    }

    // x interleave
    // note pix_size_x and pix_size_y must be set correctly for this to work properly
    if( get_interleave_x() ){

        // if y is not given, we need to figure out which row we're in
        if( y == 65535 ){

            uint16_t row = ( x % pixarray->count ) / pixarray->size_x;
            
            if( row & 1 ){

                uint16_t temp_x = x % pixarray->size_x;

                // flip x around
                temp_x = ( pixarray->size_x - 1 ) - temp_x;
                x = ( row * pixarray->size_x ) + temp_x;
            }
        }   
        // 2D access, this is easier because we already know y
        else{

            if( ( y % pixarray->size_y ) & 1 ){

                x = ( pixarray->size_x - 1 ) - ( x % pixarray->size_x );
            }
        }        
    }

    // invert x axis
    if( get_invert_x() ){

        // if y is not given, we need to figure out which row we're in
        if( y == 65535 ){

            uint16_t row = ( x % pixarray->count ) / pixarray->size_x;
            uint16_t temp_x = x % pixarray->size_x;

            // flip x around
            temp_x = ( pixarray->size_x - 1 ) - temp_x;

            x = ( row * pixarray->size_x ) + temp_x;
        }   
        else{

            x = ( pixarray->size_x - 1 ) - ( x % pixarray->size_x );
        }        
    }

    int16_t i = 0;

    if(y == 65535){ // 1D

    	i = x % pixarray->count;
    }
    else{

        x %= pixarray->size_x;
        y %= pixarray->size_y;

        i = x + (y * pixarray->size_x);
    }

    if( pixarray->reverse ){

        i = ( pixarray->count - 1 ) - i;

        if( i < 0 ){

            return 0xffff;
        }
    }

    uint16_t index = ( i + pixarray->index ) % get_pix_count();

	return index;
}

static inline __attribute__((always_inline)) int _calc_index_1d(const pixel_array_t *pixarray, uint16_t x){

	int index = _calc_index(pixarray, x, 65535);

	// // wrap index around target array
	// x %= pixarray->count;

	// uint32_t index = x;

	// // offset index by pixel array offset
	// index += pixarray->index;

	// // wrap index around master array
	// index %= get_pix_count();

	return index;
}

static inline __attribute__((always_inline)) int _calc_index_2d(const pixel_array_t *pixarray, uint16_t x, uint16_t y){

	int index = _calc_index(pixarray, x, y);

	// wrap index around target array
	// x %= pixarray->size_x;
	// y %= pixarray->size_y;

	// uint32_t index = x + (y * pixarray->size_x);

	// return _calc_index_1d(pixarray, index);

	// // offset index by pixel array offset
	// index += pixarray->index;

	// // wrap index around master array
	// index %= get_pix_count();

	return index;
}

static int _set_field(pixel_array_t *pixarray, uint32_t field_hash, int32_t value){

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(index)){
	#else
	if(field_hash == __KV__index){
	#endif

		pixarray->index = value;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(count)){
	#else
	else if(field_hash == __KV__count){
	#endif

		pixarray->count = value;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(reverse)){
	#else
	else if(field_hash == __KV__reverse){
	#endif

		pixarray->reverse = value;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(size_x)){
	#else
	else if(field_hash == __KV__size_x){
	#endif

		pixarray->size_x = value;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(size_y)){
	#else
	else if(field_hash == __KV__size_y){
	#endif

		pixarray->size_y = value;
	}
	else{

		return -1;
	}

	return 0;
}

int _get_field(const pixel_array_t *pixarray, uint32_t field_hash, int32_t *value){

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(index)){
	#else
	if(field_hash == __KV__index){
	#endif

		*value = pixarray->index;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(count)){
	#else
	else if(field_hash == __KV__count){
	#endif

		*value = pixarray->count;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(reverse)){
	#else
	else if(field_hash == __KV__reverse){
	#endif

		*value = pixarray->reverse;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(size_x)){
	#else
	else if(field_hash == __KV__size_x){
	#endif

		*value = pixarray->size_x;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(size_y)){
	#else
	else if(field_hash == __KV__size_y){
	#endif

		*value = pixarray->size_y;
	}
	else{

		return -1;
	}

	return 0;
}

int pixelarray_lookup(uint32_t name_hash, pixel_array_t *pixarray){

	const pixel_array_t *pixptr = (pixel_array_t *)array_get_data(pixel_array);

	for(int i = 0; i < array_get_count(pixel_array); i++){

		if(pixptr[i].name_hash == name_hash){

			if(pixarray != NULL){

				*pixarray = pixptr[i];	
			}
			
			return 0;
		}
	}

	return -1;
}

static inline __attribute__((always_inline)) int _pixelarray_lookup_ptr(uint32_t name_hash, pixel_array_t **pixarray){

	// pixel_array_t *pixptr = (pixel_array_t *)array_get_data(pixel_array);

	for(int i = 0; i < pixarray_count; i++){
	// for(int i = 0; i < array_get_count(pixel_array); i++){

		// if(pixptr[i].name_hash == name_hash){
		if(pixarray_ptr[i].name_hash == name_hash){

			if(pixarray != NULL){

				*pixarray = &pixarray_ptr[i];	
				// *pixarray = &pixptr[i];	
			}
			
			return 0;
		}
	}

	return -1;
}

int pixelarray_lookup_ptr(uint32_t name_hash, pixel_array_t **pixarray){

	return _pixelarray_lookup_ptr(name_hash, pixarray);
}


int pixelarray_pstore_hue(uint32_t name_hash, uint16_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	if(val == 65536){

		// this is a shortcut to allow assignment 1.0 to be maximum, instead
    	// of rolling over to 0.0.
		val = 65535;
	}

	// wrap hue
	val %= 65536;

	index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_hue;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_sat(uint32_t name_hash, uint16_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_sat;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_sat_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_val(uint32_t name_hash, uint16_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_val;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_hsfade(uint32_t name_hash, uint16_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_hsfade;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	step_ptr[index] = 0; // reset fader
	step_ptr = pixels_sat_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_vfade(uint32_t name_hash, uint16_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_vfade;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_hue2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	if(val == 65536){

		// this is a shortcut to allow assignment 1.0 to be maximum, instead
    	// of rolling over to 0.0.
		val = 65535;
	}

	// wrap hue
	val %= 65536;

	index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_hue;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_sat2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_sat;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_sat_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_val2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_val;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_hsfade2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_hsfade;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	step_ptr[index] = 0; // reset fader
	step_ptr = pixels_sat_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}

int pixelarray_pstore_vfade2(uint32_t name_hash, uint16_t index, uint16_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_vfade;
	ptr[index] = val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	step_ptr[index] = 0; // reset fader
	#endif

	return 0;
}


int pixelarray_vstore_hue(uint32_t name_hash, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	if(val == 65536){

		// this is a shortcut to allow assignment 1.0 to be maximum, instead
    	// of rolling over to 0.0.
		val = 65535;
	}

	// wrap hue
	val %= 65536;

	uint16_t *ptr = pixels_hue;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	#endif

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	for(int i = 0; i < pixarray->count; i++){

		ptr[i + index] = val;

		#ifndef FX4_COMPILER
		step_ptr[i + index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_vstore_sat(uint32_t name_hash, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	uint16_t *ptr = pixels_sat;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_sat_step;
	#endif

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	for(int i = 0; i < pixarray->count; i++){

		ptr[i + index] = val;

		#ifndef FX4_COMPILER
		step_ptr[i + index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_vstore_val(uint32_t name_hash, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	uint16_t *ptr = pixels_val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	#endif

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	for(int i = 0; i < pixarray->count; i++){

		ptr[i + index] = val;

		#ifndef FX4_COMPILER
		step_ptr[i + index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_vstore_hsfade(uint32_t name_hash, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	uint16_t *ptr = pixels_hsfade;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	uint16_t *step_ptr2 = pixels_sat_step;
	#endif

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	for(int i = 0; i < pixarray->count; i++){

		ptr[i + index] = val;

		#ifndef FX4_COMPILER
		step_ptr[i + index] = 0; // reset fader
		step_ptr2[i + index] = 0; // reset fader
		#endif
	}


	return 0;
}

int pixelarray_vstore_vfade(uint32_t name_hash, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// clamp value
	if(val < 0){

		val = 0;
	}
	else if(val > 65535){

		val = 65535;
	}

	uint16_t *ptr = pixels_vfade;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	#endif

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	for(int i = 0; i < pixarray->count; i++){

		ptr[i + index] = val;

		#ifndef FX4_COMPILER
		step_ptr[i + index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_vector_op_hue(uint32_t name_hash, uint8_t op, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	uint16_t *ptr = pixels_hue;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		for(int i = 0; i < pixarray->count; i++){

			int32_t temp = ptr[i + index] + val;

			// wrap hue
			temp %= 65536;

			ptr[i + index] = temp;

			#ifndef FX4_COMPILER
			step_ptr[i + index] = 0; // reset fader
			#endif
		}
	}
	else if(op == ASSIGN_OP_SUB){

		for(int i = 0; i < pixarray->count; i++){

			int32_t temp = ptr[i + index] - val;

			// wrap hue
			temp %= 65536;

			ptr[i + index] = temp;

			#ifndef FX4_COMPILER
			step_ptr[i + index] = 0; // reset fader
			#endif
		}
	}

	return 0;
}

int pixelarray_vector_op_sat(uint32_t name_hash, uint8_t op, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	uint16_t *ptr = pixels_sat;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_sat_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		for(int i = 0; i < pixarray->count; i++){

			int32_t temp = ptr[i + index] + val;

			// clamp value
			if(temp < 0){

				temp = 0;
			}
			else if(temp > 65535){

				temp = 65535;
			}

			ptr[i + index] = temp;

			#ifndef FX4_COMPILER
			step_ptr[i + index] = 0; // reset fader
			#endif
		}
	}
	else if(op == ASSIGN_OP_SUB){

		for(int i = 0; i < pixarray->count; i++){

			int32_t temp = ptr[i + index] - val;

			// clamp value
			if(temp < 0){

				temp = 0;
			}
			else if(temp > 65535){

				temp = 65535;
			}

			ptr[i + index] = temp;

			#ifndef FX4_COMPILER
			step_ptr[i + index] = 0; // reset fader
			#endif
		}
	}

	return 0;
}

int pixelarray_vector_op_val(uint32_t name_hash, uint8_t op, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	// int index = _calc_index_1d(pixarray, 0);
	int index = pixarray->index;

	uint16_t *ptr = pixels_val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		for(int i = 0; i < pixarray->count; i++){
			
			int32_t temp = ptr[i + index] + val;

			// clamp value
			if(temp < 0){

				temp = 0;
			}
			else if(temp > 65535){

				temp = 65535;
			}

			ptr[i + index] = temp;

			#ifndef FX4_COMPILER
			step_ptr[i + index] = 0; // reset fader
			#endif
		}
	}
	else if(op == ASSIGN_OP_SUB){

		for(int i = 0; i < pixarray->count; i++){

			int32_t temp = ptr[i + index] - val;

			// clamp value
			if(temp < 0){

				temp = 0;
			}
			else if(temp > 65535){

				temp = 65535;
			}

			ptr[i + index] = temp;

			#ifndef FX4_COMPILER
			step_ptr[i + index] = 0; // reset fader
			#endif
		}
	}

	return 0;
}

int pixelarray_pixel_op_hue(uint32_t name_hash, uint8_t op, int32_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	int array_index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_hue;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		int32_t temp = ptr[array_index] + val;

		// wrap hue
		temp %= 65536;

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		int32_t temp = ptr[array_index] - val;

		// wrap hue
		temp %= 65536;

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_pixel_op_sat(uint32_t name_hash, uint8_t op, int32_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	int array_index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_sat;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_sat_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		int32_t temp = ptr[array_index] + val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		int32_t temp = ptr[array_index] - val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_pixel_op_val(uint32_t name_hash, uint8_t op, int32_t index, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	int array_index = _calc_index_1d(pixarray, index);

	uint16_t *ptr = pixels_val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		int32_t temp = ptr[array_index] + val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		int32_t temp = ptr[array_index] - val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_pixel_op_hue2(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	int array_index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_hue;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_hue_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		int32_t temp = ptr[array_index] + val;

		// wrap hue
		temp %= 65536;

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		int32_t temp = ptr[array_index] - val;

		// wrap hue
		temp %= 65536;

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_pixel_op_sat2(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	int array_index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_sat;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_sat_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		int32_t temp = ptr[array_index] + val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		int32_t temp = ptr[array_index] - val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_pixel_op_val2(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t val){

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	int array_index = _calc_index_2d(pixarray, index, index2);

	uint16_t *ptr = pixels_val;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = pixels_val_step;
	#endif

	if(op == ASSIGN_OP_ADD){

		int32_t temp = ptr[array_index] + val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		int32_t temp = ptr[array_index] - val;

		// clamp value
		if(temp < 0){

			temp = 0;
		}
		else if(temp > 65535){

			temp = 65535;
		}

		ptr[array_index] = temp;

		#ifndef FX4_COMPILER
		step_ptr[array_index] = 0; // reset fader
		#endif
	}

	return 0;
}

int pixelarray_vector_load(uint32_t name_hash, uint8_t op, int32_t *val){

	debug_printf("pixelarray_vector_load %s op: %d\n", hash_s_get_str(name_hash), op);

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	if(op == PIXEL_ATTR_COUNT){

		*val = pixarray->count;
	}
	else if(op == PIXEL_ATTR_INDEX){

		*val = pixarray->index;
	}
	else if(op == PIXEL_ATTR_SIZE_X){

		*val = pixarray->size_x;
	}
	else if(op == PIXEL_ATTR_SIZE_Y){

		*val = pixarray->size_y;
	}
	else if(op == PIXEL_ATTR_REVERSE){

		*val = pixarray->reverse;
	}
	
	return 0;	
}

int pixelarray_load(uint32_t name_hash, uint8_t op, int32_t index, int32_t index2, int32_t *val){

	if(index2 < 0){

		debug_printf("pixelarray_load %s[%d]\n", hash_s_get_str(name_hash), index);	
	}
	else{

		debug_printf("pixelarray_load %s[%d, %d]\n", hash_s_get_str(name_hash), index, index2);
	}
	

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(name_hash, &pixarray) < 0){

		return -1;
	}

	const uint16_t *ptr;

	int array_index = 0;

	if(index2 < 0){
	
		array_index = _calc_index_1d(pixarray, index);
	}
	else{

		array_index = _calc_index_2d(pixarray, index, index2);
	}

	if(op == PIXEL_ATTR_HUE){

		ptr = pixels_hue;
		*val = ptr[array_index];

		// check for max value and coerce to 1.0 in fixed16
		// this performs the reverse operation as the store
		// so that a store then load will return
		// the same value.
		if(*val == 65535){

			*val = 65536;
		}

		return 0;
	}
	else if(op == PIXEL_ATTR_SAT){

		 ptr = pixels_sat;
		 *val = ptr[array_index];

		// check for max value and coerce to 1.0 in fixed16
		// this performs the reverse operation as the store
		// so that a store then load will return
		// the same value.
		if(*val == 65535){

			*val = 65536;
		}

		return 0;
	}
	else if(op == PIXEL_ATTR_VAL){

		 ptr = pixels_val;
		 *val = ptr[array_index];

		// check for max value and coerce to 1.0 in fixed16
		// this performs the reverse operation as the store
		// so that a store then load will return
		// the same value.
		if(*val == 65535){

			*val = 65536;
		}

		return 0;
	}
	else if(op == PIXEL_ATTR_HSFADE){

		 ptr = pixels_hsfade;
		 *val = ptr[array_index];

		return 0;
	}
	else if(op == PIXEL_ATTR_VFADE){

		 ptr = pixels_vfade;
		 *val = ptr[array_index];

		return 0;
	}
	else{

		return -1;
	}

	*val = ptr[array_index];
	
	return 0;	
}

int pixelarray_load_obj(uint32_t obj_hash, uint32_t field_hash, int32_t *val){

	debug_printf("pixelarray_load_obj %s -> %s\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash));

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(obj_hash, &pixarray) < 0){

		return -1;
	}

	uint16_t *ptr;

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(hue)){
	#else
	if(field_hash == __KV__hue){
	#endif

		ptr = pixels_hue;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(sat)){
	#else
	else if(field_hash == __KV__sat){
	#endif

		ptr = pixels_sat;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(val)){
	#else
	else if(field_hash == __KV__val){
	#endif

		ptr = pixels_val;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(hsfade)){
	#else
	else if(field_hash == __KV__hsfade){
	#endif

		ptr = pixels_hsfade;
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(vfade)){
	#else
	else if(field_hash == __KV__vfade){
	#endif

		ptr = pixels_vfade;
	}	
	else{

		return _get_field(pixarray, field_hash, val);
	}

	int index = _calc_index_1d(pixarray, 0); 

	uint16_t max = 0;
	for(int i = 0; i < pixarray->count; i++){

		uint16_t pixval = ptr[i + index];

		if(pixval > max){

			max = pixval;
		}
	}

	*val = max;

	return 0;
}

int pixelarray_load_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	int32_t *val){

	debug_printf("pixelarray_load_obj_n %s -> %s\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash));

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(obj_hash, &pixarray) < 0){

		return -1;
	}

	int index = 0;
	
	if(obj_ranks == 1){

		index = _calc_index_1d(pixarray, obj_indexes[0]);
	}
	else if(obj_ranks == 2){

		index = _calc_index_2d(pixarray, obj_indexes[0], obj_indexes[1]);
	}
	else{

		return -1;
	}

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(hue)){
	#else
	if(field_hash == __KV__hue){
	#endif

		const uint16_t *ptr = pixels_hue;
		*val = ptr[index];

		// check for max value and coerce to 1.0 in fixed16
		// this performs the reverse operation as the store
		// so that a store then load will return
		// the same value.
		if(*val == 65535){

			*val = 65536;
		}
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(sat)){
	#else
	else if(field_hash == __KV__sat){
	#endif

		const uint16_t *ptr = pixels_sat;
		*val = ptr[index];

		// check for max value and coerce to 1.0 in fixed16
		// this performs the reverse operation as the store
		// so that a store then load will return
		// the same value.
		if(*val == 65535){

			*val = 65536;
		}
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(val)){
	#else
	else if(field_hash == __KV__val){
	#endif

		const uint16_t *ptr = pixels_val;
		*val = ptr[index];

		// check for max value and coerce to 1.0 in fixed16
		// this performs the reverse operation as the store
		// so that a store then load will return
		// the same value.
		if(*val == 65535){

			*val = 65536;
		}
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(hsfade)){
	#else
	else if(field_hash == __KV__hsfade){
	#endif	

		const uint16_t *ptr = pixels_hsfade;
		*val = ptr[index];
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(vfade)){
	#else
	else if(field_hash == __KV__vfade){
	#endif

		const uint16_t *ptr = pixels_vfade;
		*val = ptr[index];
	}

	return 0;
}

int pixelarray_store_obj(uint32_t obj_hash, uint32_t field_hash, int32_t val){

	debug_printf("pixelarray_store_obj %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(obj_hash, &pixarray) < 0){

		return -1;
	}

	uint16_t *ptr;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = 0;
	uint16_t *step_ptr2 = 0;
	#endif

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(hue)){
	#else
	if(field_hash == __KV__hue){
	#endif

		if(val == 65536){

			// this is a shortcut to allow assignment 1.0 to be maximum, instead
        	// of rolling over to 0.0.
			val = 65535;
		}

		// wrap hue
    	val %= 65536;

		ptr = pixels_hue;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(sat)){
	#else
	else if(field_hash == __KV__sat){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		ptr = pixels_sat;

		#ifndef FX4_COMPILER
		step_ptr = pixels_sat_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(val)){
	#else
	else if(field_hash == __KV__val){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		ptr = pixels_val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(hsfade)){
	#else
	else if(field_hash == __KV__hsfade){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		ptr = pixels_hsfade;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		step_ptr2 = pixels_sat_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(vfade)){
	#else
	else if(field_hash == __KV__vfade){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		ptr = pixels_vfade;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		#endif
	}	
	else{

		return _set_field(pixarray, field_hash, val);
	}

	int index = _calc_index_1d(pixarray, 0); 

	#ifdef FX4_COMPILER

	for(int i = 0; i < pixarray->count; i++){

		ptr[i + index] = val;
	}

	#else

	if(step_ptr2 == 0){

		for(int i = 0; i < pixarray->count; i++){

			ptr[i + index] = val;
			step_ptr[i + index] = 0; // reset fader
		}
	}
	else{

		// special case for hsfade which resets 2 faders
		for(int i = 0; i < pixarray->count; i++){

			ptr[i + index] = val;
			step_ptr[i + index] = 0; // reset fader
			step_ptr2[i + index] = 0; // reset fader
		}
	}
	#endif

	return 0;
}

int pixelarray_store_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	int32_t val){

	debug_printf("pixelarray_store_obj_n %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(obj_hash, &pixarray) < 0){

		return -1;
	}

	int index = 0;

	if(obj_ranks == 1){

		index = _calc_index_1d(pixarray, obj_indexes[0]);
	}
	else if(obj_ranks == 2){

		index = _calc_index_2d(pixarray, obj_indexes[0], obj_indexes[1]);
	}
	else{

		return -1;
	}

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = 0;
	#endif

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(hue)){
	#else
	if(field_hash == __KV__hue){
	#endif

		if(val == 65536){

			// this is a shortcut to allow assignment 1.0 to be maximum, instead
        	// of rolling over to 0.0.
			val = 65535;
		}

		// wrap hue
    	val %= 65536;

    	uint16_t *ptr = pixels_hue;
		ptr[index] = val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		step_ptr[index] = 0; // reset fader
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(sat)){
	#else
	else if(field_hash == __KV__sat){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		uint16_t *ptr = pixels_sat;
		ptr[index] = val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_sat_step;
		step_ptr[index] = 0; // reset fader
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(val)){
	#else
	else if(field_hash == __KV__val){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		uint16_t *ptr = pixels_val;
		ptr[index] = val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		step_ptr[index] = 0; // reset fader
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(hsfade)){
	#else
	else if(field_hash == __KV__hsfade){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		uint16_t *ptr = pixels_hsfade;
		ptr[index] = val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		step_ptr[index] = 0; // reset fader
		step_ptr = pixels_sat_step;
		step_ptr[index] = 0; // reset fader
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(vfade)){
	#else
	else if(field_hash == __KV__vfade){
	#endif

		// clamp value
		if(val < 0){

			val = 0;
		}
		else if(val > 65535){

			val = 65535;
		}

		uint16_t *ptr = pixels_vfade;
		ptr[index] = val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		step_ptr[index] = 0; // reset fader
		#endif
	}

	return 0;
}


int pixelarray_op_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t op, 
	int32_t val){

	debug_printf("pixelarray_op_obj %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(obj_hash, &pixarray) < 0){

		return -1;
	}

	uint16_t *ptr;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = 0;
	uint16_t *step_ptr2 = 0;
	#endif

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(hue)){
	#else
	if(field_hash == __KV__hue){
	#endif

		ptr = pixels_hue;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(sat)){
	#else
	else if(field_hash == __KV__sat){
	#endif

		ptr = pixels_sat;

		#ifndef FX4_COMPILER
		step_ptr = pixels_sat_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(val)){
	#else
	else if(field_hash == __KV__val){
	#endif

		ptr = pixels_val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(hsfade)){
	#else
	else if(field_hash == __KV__hsfade){
	#endif

		ptr = pixels_hsfade;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		step_ptr2 = pixels_sat_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(vfade)){
	#else
	else if(field_hash == __KV__vfade){
	#endif

		ptr = pixels_vfade;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		#endif
	}	
	else{

		return -1;
	}

	int index = _calc_index_1d(pixarray, 0); 

	if(op == ASSIGN_OP_ADD){

		for(int i = 0; i < pixarray->count; i++){

			ptr[i + index] += val;
			#ifndef FX4_COMPILER
			step_ptr[i + index]	= 0; // reset fader
			#endif
		}
	}
	else if(op == ASSIGN_OP_SUB){

		for(int i = 0; i < pixarray->count; i++){

			ptr[i + index] -= val;
			#ifndef FX4_COMPILER
			step_ptr[i + index]	= 0; // reset fader
			#endif
		}
	}

	// special handling for hsfade, this resets hue and sat faders
	#ifndef FX4_COMPILER
	if(step_ptr2 != 0){

		for(int i = 0; i < pixarray->count; i++){

			step_ptr2[i + index]	= 0; // reset fader
		}
	}
	#endif

	return 0;
}

int pixelarray_op_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes,
	uint8_t op, 
	int32_t val){

	debug_printf("pixelarray_op_obj_n %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);

	pixel_array_t *pixarray;
	if(_pixelarray_lookup_ptr(obj_hash, &pixarray) < 0){

		return -1;
	}

	int index = 0;

	if(obj_ranks == 1){

		index = _calc_index_1d(pixarray, obj_indexes[0]);
	}
	else if(obj_ranks == 2){

		index = _calc_index_2d(pixarray, obj_indexes[0], obj_indexes[1]);
	}
	else{

		return -1;
	}

	uint16_t *ptr;

	#ifndef FX4_COMPILER
	uint16_t *step_ptr = 0;
	uint16_t *step_ptr2 = 0;
	#endif

	#ifdef FX4_COMPILER
	if(field_hash == HASH32(hue)){
	#else
	if(field_hash == __KV__hue){
	#endif

		ptr = pixels_hue;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(sat)){
	#else
	else if(field_hash == __KV__sat){
	#endif

		ptr = pixels_sat;

		#ifndef FX4_COMPILER
		step_ptr = pixels_sat_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(val)){
	#else
	else if(field_hash == __KV__val){
	#endif

		ptr = pixels_val;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(hsfade)){
	#else
	else if(field_hash == __KV__hsfade){
	#endif

		ptr = pixels_hsfade;

		#ifndef FX4_COMPILER
		step_ptr = pixels_hue_step;
		step_ptr2 = pixels_sat_step;
		#endif
	}
	#ifdef FX4_COMPILER
	else if(field_hash == HASH32(vfade)){
	#else
	else if(field_hash == __KV__vfade){
	#endif

		ptr = pixels_vfade;

		#ifndef FX4_COMPILER
		step_ptr = pixels_val_step;
		#endif
	}	
	else{

		return -1;
	}

	if(op == ASSIGN_OP_ADD){

		ptr[index] += val;
		#ifndef FX4_COMPILER
		step_ptr[index] = 0; // reset fader
		#endif
	}
	else if(op == ASSIGN_OP_SUB){

		ptr[index] -= val;
		#ifndef FX4_COMPILER
		step_ptr[index] = 0; // reset fader
		#endif
	}

	#ifndef FX4_COMPILER
	// special handling for hsfade, this resets hue and sat faders
	if(step_ptr2 != 0){

		step_ptr2[index] = 0; // reset fader
	}
	#endif

	return 0;
}

#endif