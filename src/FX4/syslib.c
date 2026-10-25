
#include <stdio.h>

#include "hash32.h"
#include "syslib.h"
#include "bytecode.h"
#include "array.h"
#include "pixelarray.h"
#include "debug_printf.h"

#ifndef FX4_COMPILER
#include "target.h"
#include "gfx_lib.h"
#include "random.h"
#include "catbus_common.h"
#include "logging.h"
#include "catbus.h"
#include "link4.h"
#include "cron.h"
#include "device_db.h"
#include "superconductor.h"

#ifdef ENABLE_BATTERY
#include "buttons.h"
#include "fuel_gauge.h"
#endif

#endif

/*

Library metadata:

Input to compiler listing available library functions and objects
with type information.
This might be retrieved from a remote target as a binary data
file and loaded into the compiler dynamically.


Library linkage:
Input to bytecode VM to actually access objects/functions.


*/

#ifdef FX4_COMPILER

static uint32_t kv_test_key;
static uint32_t kv_test_array[4];

#endif

int32_t lib_print(vm_t *vm, uint8_t arg_count, int32_t *args){

	#ifdef FX4_COMPILER
	debug_printf("PRINT: %d\n", args[0]);
	printf("PRINT: %d\n", args[0]);
	#else
	log_v_debug_P( PSTR("VM4: %d"), args[0] );
	#endif

	return 0;
}

int32_t lib_prints(vm_t *vm, uint8_t arg_count, int32_t *args){

	#ifdef FX4_COMPILER
	debug_printf("PRINTS: %s\n", hash_s_get_str(args[0]));
	#else
	char s[CATBUS_STRING_LEN] = {0};
	kvdb_i8_lookup_name(args[0], s);
	log_v_debug_P( PSTR("VM4: %s"), s );
	#endif

	return 0;
}

int32_t lib_len(vm_t *vm, uint8_t arg_count, int32_t *args){

	const array_meta_t *meta = (array_meta_t *)&args[0];

	debug_printf("LEN: %d %d %d %d\n", arg_count, args[0], meta->array_index, meta->size);

	return meta->size;
}

int32_t lib_kvlen(vm_t *vm, uint8_t arg_count, int32_t *args){

	debug_printf("KVLEN: %d %d\n", arg_count, args[0]);

	#ifdef FX4_COMPILER
	if((uint32_t)args[0] == HASH32(kv_test_array)){

		return cnt_of_array(kv_test_array);
	}	

	#else


	#endif

	return 1;
}

#ifdef FX4_COMPILER

static uint32_t lfsr32 = 0x45145894;
static uint32_t lfsr31 = 0xAA98915B;

// 31 and 32 bit XOR's LFSR from https://www.maximintegrated.com/en/app-notes/index.mvp/id/4400
#define POLY32 0xB4BCD35C
#define POLY31 0x7A5BC2E3

#define LFSR31_SEED 0x10856523


uint32_t shift_lfsr32( uint32_t _lfsr, uint32_t mask ){

    uint8_t fb = _lfsr & 1;
    _lfsr >>= 1;
    if( fb == 1 ){

        _lfsr ^= mask;
    }

    return _lfsr;
}

uint16_t rand31_32_u16( uint32_t *_lfsr31, uint32_t *_lfsr32 ){

    *_lfsr32 = shift_lfsr32( *_lfsr32, POLY32 );
    *_lfsr32 = shift_lfsr32( *_lfsr32, POLY32 );
    *_lfsr31 = shift_lfsr32( *_lfsr31, POLY31 );

    return ( *_lfsr32 ^ *_lfsr31 ) & 0xffff;
}

uint32_t rand31_32_u32( uint32_t *_lfsr31, uint32_t *_lfsr32 ){

    *_lfsr32 = shift_lfsr32( *_lfsr32, POLY32 );
    *_lfsr32 = shift_lfsr32( *_lfsr32, POLY32 );
    *_lfsr31 = shift_lfsr32( *_lfsr31, POLY31 );

    return ( *_lfsr32 ^ *_lfsr31 );
}

uint8_t rnd_u8_get_int( void ){

    uint16_t val = rand31_32_u16( &lfsr31, &lfsr32 );

    return val & 0xff;
}

uint16_t rnd_u16_get_int( void ){

    uint16_t val = rand31_32_u16( &lfsr31, &lfsr32 );

    return val;
}

int32_t lib_rand(vm_t *vm, uint8_t arg_count, int32_t *args){

	return rnd_u16_get_int();
}



#define DIMMER_LOOKUP_SIZE 256
// smootherstep is an 8 bit lookup table for the function:
// 6 * pow(x, 5) - 15 * pow(x, 4) + 10 * pow(x, 3)
static uint8_t smootherstep_lookup[DIMMER_LOOKUP_SIZE] = {
    #include "smootherstep.csv"
};

#define NOISE_TABLE_SIZE 256
static uint8_t noise_table[NOISE_TABLE_SIZE];

void gfx_v_init_noise( void ){

    for( uint32_t i = 0; i < NOISE_TABLE_SIZE; i++ ){

        noise_table[i] = rnd_u8_get_int();      
    }
}

static uint16_t lerp( uint8_t low, uint8_t high, uint8_t t ){

    return ( (uint16_t)low * ( 256 - t ) + (uint16_t)high * t );
}

uint16_t gfx_u16_noise( uint16_t x ){

    // get floor
    uint8_t x_min = x >> 8;
        
    // get fractional part
    uint8_t t = x & 0xff;

    t = smootherstep_lookup[t];

    return lerp( noise_table[x_min], noise_table[x_min + 1], t );
}


int32_t lib_noise(vm_t *vm, uint8_t arg_count, int32_t *args){

	if(noise_table[0] == 0){

		gfx_v_init_noise();
	}

	printf("noise: %5d -> %5d\n", args[0], gfx_u16_noise(args[0]));

	return gfx_u16_noise(args[0]);
}


int32_t lib_query(vm_t *vm, uint8_t arg_count, int32_t *args){

	printf("query: args: %d | ", arg_count);

	// for(int i = 0; i < arg_count; i++){

	// 	printf("%d ", args[i]);
	// }

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	array_meta_t *array_meta = (array_meta_t *)&args[0];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	for(int i = 0; i < array_meta->size; i++){

		printf("%s ", hash_s_get_str(array_ptr[i]));
	}

	printf("\n");

	return 7;
}

int32_t lib_send(vm_t *vm, uint8_t arg_count, int32_t *args){

	printf("send: args: %d | ", arg_count);
	
	printf("%s -> %s | query: ", hash_s_get_str(args[0]), hash_s_get_str(args[1]));

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	array_meta_t *array_meta = (array_meta_t *)&args[2];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	for(int i = 0; i < array_meta->size; i++){

		printf("%s ", hash_s_get_str(array_ptr[i]));
	}

	printf("\n");


	return 0;
}

int32_t lib_recv(vm_t *vm, uint8_t arg_count, int32_t *args){

	printf("recv: args: %d | ", arg_count);
	
	printf("%s -> %s | query: ", hash_s_get_str(args[0]), hash_s_get_str(args[1]));

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	array_meta_t *array_meta = (array_meta_t *)&args[2];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	for(int i = 0; i < array_meta->size; i++){

		printf("%s ", hash_s_get_str(array_ptr[i]));
	}

	printf("\n");


	return 0;
}

int32_t lib_sync(vm_t *vm, uint8_t arg_count, int32_t *args){

	printf("sync: args: %d | ", arg_count);
	
	printf("%s -> %s | query: ", hash_s_get_str(args[0]), hash_s_get_str(args[1]));

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	array_meta_t *array_meta = (array_meta_t *)&args[2];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	for(int i = 0; i < array_meta->size; i++){

		printf("%s ", hash_s_get_str(array_ptr[i]));
	}

	printf("\n");


	return 0;
}


int32_t lib_pwm_init(vm_t *vm, uint8_t arg_count, int32_t *args){

	return 0;
}

int32_t lib_pwm_write(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	return 0;
}

int32_t lib_button_pressed(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	return 0;
}

int32_t lib_button_hold(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	return 0;
}

int32_t lib_button_released(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	return 0;
}

int32_t lib_button_hold_released(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	return 0;
}

int32_t lib_batt_charge_full(vm_t *vm, uint8_t arg_count, int32_t *args){

	return 0;
}

int32_t lib_batt_charge_top(vm_t *vm, uint8_t arg_count, int32_t *args){

	return 0;
}

int32_t lib_batt_charge_mid(vm_t *vm, uint8_t arg_count, int32_t *args){

	return 0;
}

int32_t lib_batt_charge_low(vm_t *vm, uint8_t arg_count, int32_t *args){

	return 0;
}

int32_t lib_batt_charge_critical(vm_t *vm, uint8_t arg_count, int32_t *args){

	return 0;
}

int32_t lib_superconductor_enable(vm_t *vm, uint8_t arg_count, int32_t *args){

	const array_meta_t *array_meta  = (array_meta_t *)&args[1];

	printf("superconductor_enable: %s array_index: %d size: %d\n", hash_s_get_str(args[0]), array_meta->array_index, array_meta->size);

	return 0;
}

int32_t lib_cron(vm_t *vm, uint8_t arg_count, int32_t *args){

	printf("cron: 0x%0x %s\n", args[0], hash_s_get_str(args[1]));

	return 0;
}

int32_t lib_test(vm_t *vm, uint8_t arg_count, int32_t *args){

	int32_t accum = 0;

	for(int i = 0; i < arg_count; i++){

		accum += args[i];
	}
	
	return accum;
}


// this is the compiler lookup table
static const sys_func_t sys_funcs[] = {
	{"test_lib_call", lib_test, TYPE_NUMBER, 0, 4, {TYPE_NUMBER, TYPE_NUMBER, TYPE_NUMBER, TYPE_NUMBER}},

	{"print", 		lib_print, 	TYPE_NONE,   1, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"prints", 		lib_prints,	TYPE_NONE,   1, 1, {TYPE_STRING, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"len", 		lib_len, 	TYPE_NUMBER, 1, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"_kvlen", 		lib_kvlen, 	TYPE_NUMBER, 1, 1, {TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE, TYPE_NONE}},

	{"rand", 		lib_rand,	TYPE_GFX16, 0, 2, {TYPE_GFX16, TYPE_GFX16, TYPE_NONE, TYPE_NONE}},
	{"noise", 		lib_noise,	TYPE_GFX16, 1, 1, {TYPE_GFX16, TYPE_GFX16, TYPE_NONE, TYPE_NONE}},

	{"query", 		lib_query,	TYPE_NUMBER,  1, 1, {TYPE_STRING, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"send", 		lib_send,	TYPE_NUMBER,  3, 5, {TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_STRING}},
	{"recv", 		lib_recv,	TYPE_NUMBER,  3, 5, {TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_STRING}},
	{"sync", 		lib_sync,	TYPE_NUMBER,  3, 5, {TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_STRING}},

	{"pwm_init", 	lib_pwm_init,	TYPE_NONE,    1, 2, {TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE, TYPE_NONE}},
	{"pwm_write", 	lib_pwm_write,	TYPE_NONE,    2, 2, {TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE, TYPE_NONE}},

	{"cron", 		lib_cron,		TYPE_NONE,    2, 2, {TYPE_NUMBER, TYPE_STRING, TYPE_NONE, TYPE_NONE}},

	{"button_pressed", 			lib_button_pressed,			TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"button_hold", 			lib_button_hold,			TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"button_released", 		lib_button_released,		TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"button_hold_released",	lib_button_hold_released,	TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},

	{"batt_charge_full",		lib_batt_charge_full,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_top",			lib_batt_charge_top,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_mid",			lib_batt_charge_mid,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_low",			lib_batt_charge_low,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_critical",	lib_batt_charge_critical,	TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},

	{"superconductor_enable",	lib_superconductor_enable,	TYPE_NONE,    0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
};

#else
/*

SapphireOS Syslib

*/


int32_t lib_rand(vm_t *vm, uint8_t arg_count, int32_t *args){

	if(arg_count == 0){

		return rnd_u16_get_int_with_seed(&vm->rng_seed);
	}
	else if(arg_count == 1){

		return rnd_u16_range_with_seed(&vm->rng_seed, args[0] % 65536);
	}
	else if(arg_count == 2){

		return args[0] + rnd_u16_range_with_seed(&vm->rng_seed, args[1] - args[0]);
	}

	return 0;
}

int32_t lib_noise(vm_t *vm, uint8_t arg_count, int32_t *args){

	// log_v_debug_P( PSTR("%d -> %d"), args[0], gfx_u16_noise(args[0] % 65536) );

	#ifdef ENABLE_GFX
	return gfx_u16_noise(args[0] % 65536);
	#else
	return 0;
	#endif
}


int32_t lib_query(vm_t *vm, uint8_t arg_count, int32_t *args){

	// log_v_debug_P(PSTR("query: args: %d | "), arg_count);

	// for(int i = 0; i < arg_count; i++){

	// 	log_v_debug_P(PSTR("%08x "), args[i]);
	// }

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	array_meta_t *array_meta = (array_meta_t *)&args[0];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	uint32_t *array_ptr = (uint32_t *)(array_meta2 + 1);

	// for(int i = 0; i < array_meta->size; i++){

	// 	debug_printf("%s ", hash_s_get_str(array_ptr[i]));
	// }

	// debug_printf("\n");

	catbus_query_t query = {0};

	if(array_meta->size > cnt_of_array(query.tags)){
		
		log_v_warn_P(PSTR("bad array size"));
	}

	for(int i = 0; i < array_meta->size; i++){

		query.tags[i] = array_ptr[i];

		// char s[CATBUS_STRING_LEN] = {0};
		// kvdb_i8_lookup_name(array_ptr[i], s);
		// log_v_debug_P(PSTR("%08x %s"), array_ptr[i], s);
	}

	return device_db_u8_query_count(&query);
}

int32_t lib_send(vm_t *vm, uint8_t arg_count, int32_t *args){

	#ifdef ENABLE_LINK4

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	const array_meta_t *array_meta = (array_meta_t *)&args[2];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	const int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	catbus_query_t query = {0};

	for(int i = 0; i < array_meta->size; i++){

		query.tags[i] = array_ptr[i];
	}

	link4_aggregation_t8 agg = LINK4_AGG_LAST;

	if(args[4] == __KV__last){

		agg = LINK4_AGG_LAST;
	}
	else if(args[4] == __KV__min){

		agg = LINK4_AGG_MIN;
	}
	else if(args[4] == __KV__max){

		agg = LINK4_AGG_MAX;
	}
	else if(args[4] == __KV__sum){

		agg = LINK4_AGG_SUM;
	}
	else if(args[4] == __KV__avg){

		agg = LINK4_AGG_AVG;
	}
	else{

		// invalid aggregation
		return -1;
	}

	link4_l_create(LINK4_MODE_SEND, args[0], args[1], &query, vm->vm_id, args[3], agg);

	#endif

	return 0;
}

int32_t lib_recv(vm_t *vm, uint8_t arg_count, int32_t *args){

	#ifdef ENABLE_LINK4

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	const array_meta_t *array_meta = (array_meta_t *)&args[2];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	const int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	catbus_query_t query = {0};

	for(int i = 0; i < array_meta->size; i++){

		query.tags[i] = array_ptr[i];
	}

	link4_aggregation_t8 agg = LINK4_AGG_LAST;

	if(args[4] == __KV__last){

		agg = LINK4_AGG_LAST;
	}
	else if(args[4] == __KV__min){

		agg = LINK4_AGG_MIN;
	}
	else if(args[4] == __KV__max){

		agg = LINK4_AGG_MAX;
	}
	else if(args[4] == __KV__sum){

		agg = LINK4_AGG_SUM;
	}
	else if(args[4] == __KV__avg){

		agg = LINK4_AGG_AVG;
	}
	else{

		// invalid aggregation
		return -1;
	}

	link4_l_create(LINK4_MODE_RECV, args[0], args[1], &query, vm->vm_id, args[3], agg);

	#endif

	return 0;
}

int32_t lib_sync(vm_t *vm, uint8_t arg_count, int32_t *args){

	#ifdef ENABLE_LINK4

	int32_t *array        = (int32_t *)array_get_data(vm->array_list);
	const array_meta_t *array_meta = (array_meta_t *)&args[2];
	array_meta2_t *array_meta2 = (array_meta2_t *)&array[array_meta->array_index];
	const int32_t *array_ptr = (int32_t *)(array_meta2 + 1);

	catbus_query_t query = {0};

	for(int i = 0; i < array_meta->size; i++){

		query.tags[i] = array_ptr[i];
	}

	link4_aggregation_t8 agg = LINK4_AGG_LAST;

	if(args[4] == __KV__last){

		agg = LINK4_AGG_LAST;
	}
	else if(args[4] == __KV__min){

		agg = LINK4_AGG_MIN;
	}
	else if(args[4] == __KV__max){

		agg = LINK4_AGG_MAX;
	}
	else if(args[4] == __KV__sum){

		agg = LINK4_AGG_SUM;
	}
	else if(args[4] == __KV__avg){

		agg = LINK4_AGG_AVG;
	}
	else{

		// invalid aggregation
		return -1;
	}

	link4_l_create(LINK4_MODE_SYNC, args[0], args[1], &query, vm->vm_id, args[3], agg);

	#endif

	return 0;
}
int32_t lib_pwm_init(vm_t *vm, uint8_t arg_count, int32_t *args){

	if(arg_count == 1){

		pwm_v_init_channel(args[0], 0);
	}
	else if(arg_count == 2){

		pwm_v_init_channel(args[0], args[1]);
	}

	return 0;
}

int32_t lib_pwm_write(vm_t *vm, uint8_t arg_count, int32_t *args){

	pwm_v_write(args[0], args[1]);
	
	return 0;
}

#ifdef ENABLE_BATTERY

int32_t lib_button_pressed(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	int button = 0;

	if(arg_count == 1){

		button = args[0];
	}

	return button_b_is_button_pressed(button);
}


int32_t lib_button_hold(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	int button = 0;

	if(arg_count == 1){

		button = args[0];
	}

	return button_b_is_button_hold(button);
}

int32_t lib_button_released(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	int button = 0;

	if(arg_count == 1){

		button = args[0];
	}

	return button_b_is_button_released(button);
}

int32_t lib_button_hold_released(vm_t *vm, uint8_t arg_count, int32_t *args){
	
	int button = 0;

	if(arg_count == 1){

		button = args[0];
	}

	return button_b_is_button_hold_released(button);
}

int32_t lib_batt_charge_full(vm_t *vm, uint8_t arg_count, int32_t *args){

	return fuel_b_threshold_full_charge();	
}

int32_t lib_batt_charge_top(vm_t *vm, uint8_t arg_count, int32_t *args){

	return fuel_b_threshold_top_charge();	
}

int32_t lib_batt_charge_mid(vm_t *vm, uint8_t arg_count, int32_t *args){

	return fuel_b_threshold_mid_charge();	
}

int32_t lib_batt_charge_low(vm_t *vm, uint8_t arg_count, int32_t *args){

	return fuel_b_threshold_low_charge();	
}

int32_t lib_batt_charge_critical(vm_t *vm, uint8_t arg_count, int32_t *args){

	return fuel_b_threshold_critical_charge();	
}

#endif


int32_t lib_superconductor_enable(vm_t *vm, uint8_t arg_count, int32_t *args){

	sc_v_start();

	return 0;
}


int32_t lib_cron(vm_t *vm, uint8_t arg_count, int32_t *args){

	#ifdef ENABLE_TIME_NTP

	char cron_str[CATBUS_STRING_LEN] = {0};
	if(kv_i8_get_name( args[1], cron_str ) < 0){

		return -1;
	}

	// log_v_debug_P(PSTR("cron: 0x%0x %s\n"), args[0], cron_str);

	cron_v_add_job(cron_str, (uint32_t)args[0], vm->vm_id);

	#endif

	return 0;
}

// this is the compiler lookup table
static const sys_func_t sys_funcs[] = {
	{"print", 					lib_print, 		TYPE_NONE,    1, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"prints", 					lib_prints,		TYPE_NONE,    1, 1, {TYPE_STRING, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"len", 					lib_len, 		TYPE_NUMBER,  1, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},

	{"rand", 					lib_rand,		TYPE_GFX16, 0, 2, {TYPE_GFX16, TYPE_GFX16, TYPE_NONE, TYPE_NONE}},
	{"noise", 					lib_noise,		TYPE_GFX16, 1, 1, {TYPE_GFX16, TYPE_GFX16, TYPE_NONE, TYPE_NONE}},

	{"query", 					lib_query,		TYPE_NUMBER,  1, 1, {TYPE_STRING, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"send", 					lib_send,		TYPE_NUMBER,  3, 5, {TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_STRING}},
	{"recv", 					lib_recv,		TYPE_NUMBER,  3, 5, {TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_STRING}},
	{"sync", 					lib_sync,		TYPE_NUMBER,  3, 5, {TYPE_STRING, TYPE_STRING, TYPE_STRING, TYPE_NUMBER, TYPE_STRING}},

	{"pwm_init", 				lib_pwm_init,	TYPE_NONE,    1, 2, {TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE, TYPE_NONE}},
	{"pwm_write", 				lib_pwm_write,	TYPE_NONE,    2, 2, {TYPE_NUMBER, TYPE_NUMBER, TYPE_NONE, TYPE_NONE}},

	{"cron", 					lib_cron,		TYPE_NONE,    1, 1, {TYPE_NUMBER, TYPE_STRING, TYPE_NONE, TYPE_NONE}},

	#ifdef ENABLE_BATTERY
	{"button_pressed", 			lib_button_pressed,			TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"button_hold", 			lib_button_hold,			TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"button_released", 		lib_button_released,		TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"button_hold_released",	lib_button_hold_released,	TYPE_NUMBER,  0, 1, {TYPE_NUMBER, TYPE_NONE, TYPE_NONE, TYPE_NONE}},

	{"batt_charge_full",		lib_batt_charge_full,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_top",			lib_batt_charge_top,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_mid",			lib_batt_charge_mid,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_low",			lib_batt_charge_low,		TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
	{"batt_charge_critical",	lib_batt_charge_critical,	TYPE_NUMBER,  0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},

	#endif
	{"superconductor_enable",	lib_superconductor_enable,	TYPE_NONE,    0, 0, {TYPE_NONE, TYPE_NONE, TYPE_NONE, TYPE_NONE}},
};


#endif

// returns function table index
int syslib_lookup_func(uint32_t func_hash){

	for(int i = 0; i < cnt_of_array(sys_funcs); i++){

		if(hash_u32_string(sys_funcs[i].func_name) == func_hash){

			return i;
		}		
	}

	return -1;
}

int syslib_lookup_func_def(uint32_t func_hash, func_def_t *func_def){

	int index = syslib_lookup_func(func_hash);

	if(index < 0){

		return -1;
	}

	func_def->name_hash 	= func_hash;
	func_def->arg_count 	= 0;
	func_def->arg_count_min = sys_funcs[index].arg_count_min;
	func_def->arg_count_max = sys_funcs[index].arg_count_max;
	func_def->return_type 	= sys_funcs[index].return_type;	

	return index;
}

int syslib_call_func(vm_t *vm, uint32_t func_hash, uint8_t arg_count, int32_t *args, int32_t *ret_val){

	int index = syslib_lookup_func(func_hash);

	if(index < 0){

		return -1;
	}

	const sys_func_t *sys_func = &sys_funcs[index];

	// call function, if given!
	if(sys_func->func != 0){

		*ret_val = sys_func->func(vm, arg_count, args);
	}

	debug_printf("lib call: 0x%0x args: %d ret: %d (", func_hash, arg_count, *ret_val);
	for(int i = 0; i < arg_count; i++){

		debug_printf(" %d,", args[i]);	
	}
	
	debug_printf(")\n");

	return 0;
}

int syslib_create_obj(uint32_t obj_hash, uint32_t name_hash, int32_t vm_id){

	// debug_printf("create object %s -> %s size %d\n", hash_s_get_str(obj_hash), hash_s_get_str(name_hash), syslib_obj_size(obj_hash));
	debug_printf("create object %s -> %s\n", hash_s_get_str(obj_hash), hash_s_get_str(name_hash));

	#ifdef FX4_COMPILER
	if(obj_hash == HASH32(pixelarray)){
	#else
	if(obj_hash == __KV__pixelarray){
	#endif

		if(pixelarray_create(name_hash, vm_id) < 0){

			return -1;
		}
	}
	#ifdef FX4_COMPILER
	else if((obj_hash == HASH32(keyvalue)) && (name_hash == HASH32(kv))){
	#else
	else if((obj_hash == __KV__keyvalue) && (name_hash == __KV__kv)){
	#endif

		// system KV

	}
	else{

		return -1;
	}

	return 0;
}

int syslib_load_obj(uint32_t obj_hash, uint32_t field_hash, uint8_t datatype, int32_t *val){

	debug_printf("syslib_load_obj %s -> %s\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash));

	if(pixelarray_lookup(obj_hash, NULL) == 0){

		return 
		pixelarray_load_obj(
			obj_hash, 
			field_hash,
			val);	
	}
	#ifdef FX4_COMPILER
	else if(obj_hash == HASH32(kv)){
	#else
	else if(obj_hash == __KV__kv){
	#endif

		debug_printf("KV load\n");

		#ifndef FX4_COMPILER

		// catbus_meta_t meta = {0};

		// if(kv_i8_get_catbus_meta(field_hash, &meta) < 0){

		// 	return -1;
		// }

		void *data = (void *)val;
		
		catbus_type_t8 catbus_type = CATBUS_TYPE_GFX16;

		if( catbus_i8_array_get( 
            field_hash, 
            catbus_type, 
            0, 
            1, 
            data ) < 0 ){

			// log_v_debug_P( PSTR("kv lookup failed") );
        }

        // log_v_debug_P( PSTR("kv 0x%x %d"), val, *val );

    	#else 
    	if(field_hash == HASH32(kv_test_key)){

    		*val = kv_test_key;
    	}
    	else if(field_hash == HASH32(kv_test_array)){

    		*val = kv_test_array[0];
    	}
    	else{

    		*val = 456;	
    	}
		#endif
	}
	else{

		return -1;
	}

	return 0;
}

int syslib_load_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	int32_t *val){

	debug_printf("syslib_load_obj_n %s -> %s\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash));

	if(pixelarray_lookup(obj_hash, NULL) == 0){

		return 
		pixelarray_load_obj_n(
			obj_hash, 
			obj_ranks, 
			obj_indexes,
			field_hash,
			field_ranks,
			field_indexes,
			val);
	}
	#ifdef FX4_COMPILER
	else if(obj_hash == HASH32(kv)){
	#else
	else if(obj_hash == __KV__kv){
	#endif

		debug_printf("KV load N\n");

		#ifndef FX4_COMPILER

		// catbus_meta_t meta = {0};

		// if(kv_i8_get_catbus_meta(field_hash, &meta) < 0){

		// 	return -1;
		// }

		void *data = (void *)val;
		
		catbus_type_t8 catbus_type = CATBUS_TYPE_GFX16;

		if( catbus_i8_array_get( 
            field_hash, 
            catbus_type, 
            field_indexes[0], 
            1, 
            data ) < 0 ){
        }

    	#else 
    	if(field_hash == HASH32(kv_test_key)){

    		*val = kv_test_key;
    	}
    	else if(field_hash == HASH32(kv_test_array)){

    		*val = kv_test_array[field_indexes[0] % cnt_of_array(kv_test_array)];
    	}
    	else{

    		*val = 456;	
    	}
		#endif
	}
	else{

		return -1;
	}

	return 0;
}

int syslib_store_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t datatype,
	int32_t val){

	debug_printf("syslib_store_obj %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);


	if(pixelarray_lookup(obj_hash, NULL) == 0){

		return 
		pixelarray_store_obj(
			obj_hash, 
			field_hash,
			val);
	}
	#ifdef FX4_COMPILER
	else if(obj_hash == HASH32(kv)){
	#else
	else if(obj_hash == __KV__kv){
	#endif

		debug_printf("KV store\n");

		#ifndef FX4_COMPILER

		void *data = (void *)&val;
		int data_len = sizeof(val);

		char str[CATBUS_STRING_LEN];

		catbus_type_t8 catbus_type = CATBUS_TYPE_INT32;

		if(datatype == TYPE_FIXED16){

			catbus_type = CATBUS_TYPE_FIXED16;
		} 
		else if(datatype == TYPE_STRING){

			catbus_type = CATBUS_TYPE_STRING32;

			memset(str, 0, sizeof(str));
			kv_i8_get_name(val, str);

			data = str;
			data_len = strlen(str);
		}

		if( catbus_i8_array_set( 
            field_hash, 
            catbus_type, 
            0, 
            1, 
            data, 
            data_len ) < 0 ){


        }
    	#else
    	if(field_hash == HASH32(kv_test_key)){

    		kv_test_key = val;
    	}
    	else if(field_hash == HASH32(kv_test_array)){

    		kv_test_array[0] = val;
    	}
    	else{

    		
    	}

		#endif
	}
	else{

		return -1;
	}

	return 0;
}

int syslib_store_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes, 
	uint8_t datatype,
	int32_t val){

	debug_printf("syslib_store_obj_n %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);

	if(pixelarray_lookup(obj_hash, NULL) == 0){

		return 
		pixelarray_store_obj_n(
			obj_hash, 
			obj_ranks, 
			obj_indexes,
			field_hash,
			field_ranks,
			field_indexes,
			val);
	}
	#ifdef FX4_COMPILER
	else if(obj_hash == HASH32(kv)){
	#else
	else if(obj_hash == __KV__kv){
	#endif

		debug_printf("KV store\n");

		#ifndef FX4_COMPILER

		void *data = (void *)&val;
		int data_len = sizeof(val);

		// char str[CATBUS_STRING_LEN];

		catbus_type_t8 catbus_type = CATBUS_TYPE_INT32;

		if(datatype == TYPE_FIXED16){

			catbus_type = CATBUS_TYPE_FIXED16;
		} 
		// else if(datatype == TYPE_STRING){

		// 	catbus_type = CATBUS_TYPE_STRING32;

		// 	memset(str, 0, sizeof(str));
		// 	kv_i8_get_name(val, str);

		// 	data = str;
		// 	data_len = strlen(str);
		// }

		if( catbus_i8_array_set( 
            field_hash, 
            catbus_type, 
            field_indexes[0], 
            1, 
            data, 
            data_len ) < 0 ){


        }

    	#else

    	if(field_hash == HASH32(kv_test_key)){

    		kv_test_key = val;
    	}
    	else if(field_hash == HASH32(kv_test_array)){

    		kv_test_array[field_indexes[0] % cnt_of_array(kv_test_array)] = val;
    	}
    	else{

    		
    	}

		#endif
	}
	else{

		return -1;
	}

	return 0;
}


int syslib_move_obj(
	uint32_t target, 
	uint32_t source){

	pixel_array_t *pixarray_target = NULL;
	pixel_array_t *pixarray_source = NULL;

	if((pixelarray_lookup_ptr(target, &pixarray_target) == 0) &&
	   (pixelarray_lookup_ptr(source, &pixarray_source) == 0)){

		uint32_t name_hash = pixarray_target->name_hash;
		*pixarray_target = *pixarray_source;
		pixarray_target->name_hash = name_hash; // restore name

	}
	else{

		return -1;
	}

	return 0;
}


int syslib_op_obj(
	uint32_t obj_hash, 
	uint32_t field_hash, 
	uint8_t op, 
	int32_t val){

	if(pixelarray_lookup(obj_hash, NULL) == 0){

		return 
		pixelarray_op_obj(
			obj_hash, 
			field_hash, 
			op, 
			val);
	}
	#ifdef FX4_COMPILER
	else if(obj_hash == HASH32(kv)){
	#else
	else if(obj_hash == __KV__kv){
	#endif

		debug_printf("KV op\n");
	}
	else{

		return -1;
	}

	return 0;
}


int syslib_op_obj_n(
	uint32_t obj_hash, 
	uint8_t obj_ranks, 
	uint16_t *obj_indexes, 
	uint32_t field_hash, 
	uint8_t field_ranks, 
	uint16_t *field_indexes,
	uint8_t op, 
	int32_t val){

	debug_printf("syslib_op_obj_n %s -> %s = %d\n", hash_s_get_str(obj_hash), hash_s_get_str(field_hash), val);

	if(pixelarray_lookup(obj_hash, NULL) == 0){

		return 
		pixelarray_op_obj_n(
			obj_hash, 
			obj_ranks, 
			obj_indexes,
			field_hash,
			field_ranks,
			field_indexes,
			op,
			val);
	}
	else{

		return -1;
	}

	
	return 0;
}
