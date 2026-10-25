#ifndef __SYNC4_H_
#define __SYNC4_H_

#include "target.h"

#define SYNC4_PROTOCOL_MAGIC        0x434e5953 // 'SYNC' in ASCII
#define SYNC4_PROTOCOL_VERSION      13
#define SYNC4_SERVER_PORT           32039

#define SYNC4_MAX_DATA				512
#define SYNC4_MAX_TRIES				5

#define SYNC4_DATA_TYPE_VM			1
#define SYNC4_DATA_TYPE_PIXELS		2

#define SYNC4_MAX_DATA_SERVERS      2


typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t padding;
} sync4_msg_header_t;

typedef struct __attribute__((packed)){
	sync4_msg_header_t header;
} sync4_msg_connect_t;
#define SYNC4_MSG_TYPE_CONNECT		1

typedef struct __attribute__((packed)){
	sync4_msg_header_t header;
	uint8_t vm_pages;
	uint8_t pixel_pages;
	uint32_t prog_hash;
	uint8_t seq_step;
} sync4_msg_ready_t;
#define SYNC4_MSG_TYPE_READY		2

typedef struct __attribute__((packed)){
	sync4_msg_header_t header;
	uint8_t page;
	uint8_t padding[3];
} sync4_msg_request_data_t;
#define SYNC4_MSG_TYPE_REQ_DATA		3

typedef struct __attribute__((packed)){
	sync4_msg_header_t header;
	uint8_t page;
	uint8_t total;
	uint8_t type; // see SYNC_DATA_TYPE_
	uint8_t padding;
} sync4_msg_data_t;
#define SYNC4_MSG_TYPE_DATA			4


typedef struct __attribute__((packed)){
	sync4_msg_header_t header;
	uint32_t net_time;
} sync4_msg_request_sync_t;
#define SYNC4_MSG_TYPE_REQ_SYNC		5

typedef struct __attribute__((packed)){
	sync4_msg_header_t header;
	uint64_t current_tick;
	uint64_t rng_seed;
	uint64_t frame_number;
	uint32_t net_time_client;
	uint32_t net_time_server;
	uint32_t prog_hash;
	uint8_t seq_step;
} sync4_msg_sync_t;
#define SYNC4_MSG_TYPE_SYNC			6



void sync4_v_init( void );
void sync4_v_reset( void );
bool sync4_b_is_sync( void );

bool sync4_b_is_leader( void );
bool sync4_b_is_follower( void );

void sync4_v_hold( void );
void sync4_v_unhold( void );

#endif
