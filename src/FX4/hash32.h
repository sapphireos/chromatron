
#ifndef HASH32_H
#define HASH32_H

#include <stdint.h>

#ifdef FX4_COMPILER
#define HASH32(s) hash_u32_string(#s)
#endif

uint32_t hash_u32_data( uint8_t *data, uint16_t len );
uint32_t hash_u32_string( char *src );
char* hash_s_get_str(uint32_t hash);
void hash_v_print(void);
int hash_get_count(void);
char* hash_get_string_at_index(int index);

#endif
