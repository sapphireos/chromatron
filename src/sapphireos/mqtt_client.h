/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program.  If not, see <http://www.gnu.org/licenses/>.
// 
// </license>
 */

#ifndef __MQTT_CLIENT_H
#define __MQTT_CLIENT_H

#include "catbus.h"

#define MQTT_MAX_TOPIC_LEN      128
#define MQTT_MAX_PAYLOAD_LEN    256

#define MQTT_BRIDGE_PORT        44899


void mqtt_client_v_init( void );

#define MQTT_MSG_MAGIC    0x5454514d // 'MQTT'
#define MQTT_MSG_VERSION  1
typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t version;
    uint8_t msg_type;
    uint8_t qos;
    uint8_t flags;
} mqtt_msg_header_t;

// typedef struct __attribute__((packed)){
//     catbus_hash_t32 hashes[MQTT_TOPIC_LIST_LEN];
// } mqtt_topic_list_t;

// typedef struct __attribute__((packed)){
//     mqtt_msg_header_t header;
//     catbus_hash_t32 topic_hash;
//     char topic[MQTT_MAX_TOPIC_LEN];
// } mqtt_msg_register_topic_t;
// #define MQTT_MSG_REGISTER_TOPIC     10

typedef struct __attribute__((packed)){
    mqtt_msg_header_t header;
    // uint8_t topic_len;
    // topic data
    // uint16_t payload_len;
    // payload
} mqtt_msg_publish_t;
#define MQTT_MSG_PUBLISH            20
#define MQTT_MSG_PUBLISH_KV         21

typedef struct __attribute__((packed)){
    mqtt_msg_header_t header;
    catbus_query_t tags;
    uint8_t mode;
    uint32_t uptime;
    int8_t rssi;
    int8_t wifi_channel;
    uint8_t cpu_percent;
    uint16_t used_heap;
    uint16_t pixel_power;
} mqtt_msg_publish_status_t;
#define MQTT_MSG_PUBLISH_STATUS     22



typedef struct __attribute__((packed)){
    mqtt_msg_header_t header;
    // uint8_t topic_len;
    // topic data
} mqtt_msg_subscribe_t;
#define MQTT_MSG_SUBSCRIBE          30
#define MQTT_MSG_SUBSCRIBE_KV       31


typedef void ( *mqtt_on_publish_callback_t )( char *topic, uint8_t *data, uint16_t data_len );

bool mqtt_b_match_topic( const char *topic, const char *sub );

int8_t mqtt_client_i8_publish( const char *topic, const void *data, uint16_t data_len, uint8_t qos, bool retain );
int8_t mqtt_client_i8_publish_kv( const char *topic, const char *key, uint8_t qos, bool retain );
int8_t mqtt_client_i8_subscribe( const char *topic, uint8_t qos, mqtt_on_publish_callback_t callback );
int8_t mqtt_client_i8_subscribe_kv( const char *topic, const char *key, uint8_t qos );





// int8_t mqtt_client_i8_publish_data( const char *topic, catbus_meta_t *meta, const void *data, uint8_t qos, bool retain );


#endif




