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

#define MQTT_TOPIC_LIST_LEN     16

void mqtt_client_v_init( void );

#define MQTT_MSG_MAGIC    0x5454514d // 'MQTT'
#define MQTT_MSG_VERSION  1
typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t msg_type;
    uint8_t version;
    uint16_t reserved;
} mqtt_msg_header_t;

typedef struct __attribute__((packed)){
    catbus_hash_t32 hashes[MQTT_TOPIC_LIST_LEN];
} mqtt_topic_list_t;


typedef struct __attribute__((packed)){
    mqtt_msg_header_t header;
    mqtt_topic_list_t topic;
    catbus_type_t8 payload_type;
    uint8_t payload;
} mqtt_msg_publish_t;
#define MQTT_MSG_PUBLISH     1

typedef struct __attribute__((packed)){
    mqtt_msg_header_t header;
    mqtt_topic_list_t topic;
    catbus_type_t8 payload_type;
    uint8_t payload;
} mqtt_msg_rx_data_t;
#define MQTT_MSG_RX_DATA     2


int8_t mqtt_client_i8_publish( mqtt_topic_list_t *topic, catbus_type_t8 type, const void *data );

// #define CONTROLLER_IDLE_TIMEOUT         10
// #define CONTROLLER_FOLLOWER_TIMEOUT     20
// #define CONTROLLER_ELECTION_TIMEOUT     5


// #define CONTROLLER_MSG_MAGIC    0x4C525443 // 'CTRL'
// #define CONTROLLER_MSG_VERSION  1
// typedef struct __attribute__((packed)){
//     uint32_t magic;
//     uint8_t msg_type;
//     uint8_t version;
//     uint16_t reserved;
// } controller_header_t;


// #define CONTROLLER_FLAGS_IS_LEADER      0x0001

// typedef struct __attribute__((packed)){
//     controller_header_t header;
//     uint16_t flags;
//     uint16_t priority;
//     uint16_t follower_count;
// }  controller_msg_announce_t;
// #define CONTROLLER_MSG_ANNOUNCE     1


// typedef struct __attribute__((packed)){
//     controller_header_t header;
// }  controller_msg_drop_t;
// #define CONTROLLER_MSG_DROP         2

// typedef struct __attribute__((packed)){
//     controller_header_t header;
//     catbus_query_t query;
//     uint16_t service_flags;
// }  controller_msg_status_t;
// #define CONTROLLER_MSG_STATUS       3

// #define CONTROLLER_SERVICE_NET_TIME     0x0001
// #define CONTROLLER_SERVICE_NTP_TIME     0x0002
// #define CONTROLLER_SERVICE_GFX_SYNC     0x0004
// #define CONTROLLER_SERVICE_LINK         0x0008


// typedef struct __attribute__((packed)){
//     controller_header_t header;
// }  controller_msg_leave_t;
// #define CONTROLLER_MSG_LEAVE        4

#endif




