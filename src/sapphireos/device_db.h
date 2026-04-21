
#ifndef __DEVICE_DB_H
#define __DEVICE_DB_H

#include "catbus.h"

#define DEVICE_DB_TIMEOUT 120

#define DEVICE_DB_PORT    44638

#define DEVICE_DB_MAGIC	  0x8127159A

#define DEVICE_DB_TICK    2 // seconds

typedef struct __attribute__((packed)){
	// gfx info
	uint16_t gfx_master_dimmer;
	uint16_t gfx_sub_dimmer;
	uint32_t gfx_sync_group;
	uint8_t gfx_enable;
	uint16_t pixel_power;
} device_gfx_t;

typedef struct __attribute__((packed)){
	// battery info
	uint16_t batt_volts;
	uint16_t batt_charge_current;
	int8_t batt_temp;
	uint8_t batt_status;
	uint32_t light_level;
} device_batt_t;

typedef struct __attribute__((packed)){
	// solar info
	uint16_t solar_volts;
	uint16_t solar_charge_current;
	int8_t ambient_temp;
	int8_t case_temp;
} device_solar_t;

typedef struct __attribute__((packed)){
	uint32_t magic;
	uint32_t flags;

	// basic info
	catbus_query_t query;
	uint64_t uptime;
	uint8_t mode;
	int8_t rssi;

	device_gfx_t gfx;
	device_batt_t batt;
	device_solar_t solar;
} device_msg_t;


// internal database structure
typedef struct __attribute__((packed)){
	catbus_query_t tags;
    ip_addr4_t ip;
	uint32_t gfx_sync_group;
	uint64_t uptime;
	uint8_t mode;
	int16_t timeout;
} device_data_t;


void device_db_v_init( void );
uint8_t device_db_u8_count( void );

bool device_db_b_is_all_query( void );

void device_db_v_set_query( const catbus_query_t *query );
void device_db_v_get_query( catbus_query_t *query );

bool device_db_b_has_hash( catbus_hash_t32 hash );

void device_db_v_reset_iter( void );
device_data_t* device_db_p_get_next( void );
device_data_t* device_db_p_get_next_query( catbus_query_t *query );
device_data_t* device_db_p_get_ipaddr( ip_addr4_t ip );
uint8_t device_db_u8_query_count( catbus_query_t *query );

// void device_db_v_get_file_hash_list( catbus_file_hash_list_callback_t callback );
// void device_db_v_get_key( catbus_hash_t32 hash, catbus_get_key_callback_t callback );
// void device_db_v_set_key( catbus_hash_t32 hash, catbus_type_t8 type, uint8_t *data, uint16_t data_len );

// void device_db_v_process_announce( const catbus_msg_announce_t *announce, const sock_addr_t *raddr );

#endif
