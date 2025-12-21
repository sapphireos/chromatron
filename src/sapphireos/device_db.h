
#ifndef __DEVICE_DB_H
#define __DEVICE_DB_H

#include "catbus.h"

#define DEVICE_TIMEOUT 120



typedef struct __attribute__((packed)){
    catbus_query_t tags;
    ip_addr4_t ip;
    uint8_t mode;
    uint32_t uptime;
    int8_t rssi;
    int8_t wifi_channel;
    uint8_t cpu_percent;
    uint16_t used_heap;
    uint16_t pixel_power;
    char os_version[OS_VER_LEN];
} _device_status_t;

typedef struct __attribute__((packed)){
	_device_status_t status;
	uint16_t timeout;
} device_status_t;


void device_db_v_init( void );
uint8_t device_db_u8_count( void );

bool device_db_b_is_all_query( void );

void device_db_v_set_query( const catbus_query_t *query );
void device_db_v_get_query( catbus_query_t *query );

bool device_db_b_has_hash( catbus_hash_t32 hash );

void device_db_v_sort_name( void );

void device_db_v_reset_iter( void );
device_status_t* device_db_p_get_next( void );
device_status_t* device_db_p_get_next_query( catbus_query_t *query );
device_status_t* device_db_p_get_ipaddr( ip_addr4_t ip );
uint8_t device_db_u8_query_count( catbus_query_t *query );

void device_db_v_get_file_hash_list( catbus_file_hash_list_callback_t callback );
void device_db_v_get_key( catbus_hash_t32 hash, catbus_get_key_callback_t callback );
void device_db_v_set_key( catbus_hash_t32 hash, catbus_type_t8 type, uint8_t *data, uint16_t data_len );

void device_db_v_process_announce( catbus_msg_announce_t *annouce );

#endif
