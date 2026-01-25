#ifndef __LINK4_H__
#define __LINK4_H__


#define LINK4_PORT                           44634

#define LINK4_VERSION                        4
#define LINK4_MAGIC                          0x4b4e494c // 'LINK'

#define LINK4_MIN_TICK_RATE                  50
#define LINK4_MAX_TICK_RATE                  2000

#define LINK4_RETRANSMIT_RATE                2000
#define LINK4_RETRANSMIT_RATE_FAST           100

#define LINK4_MAX_LINKS						 16

typedef list_node_t link4_handle_t;

typedef uint8_t link4_aggregation_t8;
#define LINK4_AGG_LAST						0
#define LINK4_AGG_MIN						1
#define LINK4_AGG_MAX						2
#define LINK4_AGG_SUM						3
#define LINK4_AGG_AVG						4

typedef uint8_t link4_mode_t8;
#define LINK4_MODE_SEND						0
#define LINK4_MODE_RECV						1
#define LINK4_MODE_REMOTE_SEND				2
#define LINK4_MODE_REMOTE_RECV				3

typedef uint16_t link4_rate_t16;
#define LINK4_RATE_MIN                      50
#define LINK4_RATE_1000ms                   1000
#define LINK4_RATE_MAX                      30000

typedef uint8_t link4_opcode_t8;
#define LINK4_OPCODE_LOCAL_SEND				0
#define LINK4_OPCODE_LOCAL_RECV				1
#define LINK4_OPCODE_REMOTE_SEND			2
#define LINK4_OPCODE_REMOTE_RECV			3

typedef struct __attribute__((packed)){
    link4_mode_t8 mode;
    link4_aggregation_t8 aggregation;
    link4_rate_t16 rate;
    catbus_hash_t32 source_key;
    catbus_hash_t32 dest_key;
    catbus_hash_t32 tag;
    catbus_query_t query;
} link4_t;

typedef struct __attribute__((packed)){
    link4_t link;
    uint16_t timeout;
    uint16_t transmit_timer;
    mem_handle_t database_h;
} link4_state_t;

typedef struct __attribute__((packed)){
    int32_t value;
  	ip_addr4_t ip;
  	uint16_t timeout;
} link4_data_t; // 10 bytes

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t msg_type;
    uint8_t version;
} link4_msg_header_t;

typedef struct __attribute__((packed)){
    link4_msg_header_t header;
    int32_t value;
} link4_msg_send_t;
#define LINK4_MSG_TYPE_SEND        		1

void link4_v_init( void );
link4_handle_t link4_l_create( 
    link4_mode_t8 mode, 
    catbus_hash_t32 source_key, 
    catbus_hash_t32 dest_key, 
    catbus_query_t *query,
    catbus_hash_t32 tag,
    link4_rate_t16 rate,
    link4_aggregation_t8 aggregation );

#endif