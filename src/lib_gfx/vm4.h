#ifndef _VM4_H_
#define _VM4_H_

#include "bytecode.h"
#include "threading.h"

#define VM4_MAX_VMS		4

#define VM4_SIGNAL_0	SIGNAL_SYS_0
#define VM4_SIGNAL_1	SIGNAL_SYS_1
#define VM4_SIGNAL_2	SIGNAL_SYS_2
#define VM4_SIGNAL_3	SIGNAL_SYS_3

void vm4_v_init( void );

void vm4_v_reset( uint8_t vm_id );

void vm4_v_add_published_var( uint16_t index, catbus_hash_t32 hash, catbus_type_t8 type, uint16_t count, uint8_t flags, uint8_t vm_id );

void vm4_v_run_prog( char name[FFS_FILENAME_LEN], uint8_t vm_id );
bool vm4_b_is_vm_running( uint8_t vm_id );

int8_t vm4_i8_run_function( uint32_t func_meta, uint8_t vm_id );

// uint32_t vm4_u32_get_sync_data_hash( void );
// uint16_t vm4_u16_get_sync_data_len( void );
// int32_t* vm4_i32p_get_sync_data( void );
// vm_t* vm4_p_get_vm_state( void );
// uint64_t vm4_u64_get_sync_tick( void );
// uint32_t vm4_u32_get_sync_time( void );
// void vm4_v_sync( uint32_t net_time, uint64_t sync_tick );

void vm4_v_signal( void );

void vm4_v_freeze_vm( uint8_t vm_id );
void vm4_v_unfreeze_vm( uint8_t vm_id );

void vm4_v_get_vm_state( vm_t *vm, uint8_t vm_id );

#endif
