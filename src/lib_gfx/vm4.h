#ifndef _VM4_H_
#define _VM4_H_

#define VM4_MAX_VMS		4

void vm4_v_init( void );

void vm4_v_reset( uint8_t vm_id );

void vm4_v_add_published_var( uint16_t index, catbus_hash_t32 hash, catbus_type_t8 type, uint16_t count, uint8_t flags, uint8_t vm_id );

void vm4_v_run_prog( char name[FFS_FILENAME_LEN], uint8_t vm_id );
bool vm4_b_is_vm_running( uint8_t vm_id );

int8_t vm4_i8_run_coroutine( uint16_t addr, uint8_t vm_id );

#endif
