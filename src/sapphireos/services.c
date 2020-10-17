/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#include "system.h"
#include "wifi.h"
#include "config.h"
#include "timers.h"
#include "threading.h"
#include "random.h"
#include "list.h"

#include "services.h"

#if defined(ENABLE_NETWORK) && defined(ENABLE_SERVICES)

// #define NO_LOGGING
#include "logging.h"

#define STATE_LISTEN        0
#define STATE_CONNECTED     1
#define STATE_SERVER        2
#define STATE_CANDIDATE     3

typedef struct __attribute__((packed)){
    uint32_t service;
    uint32_t group;    
    uint16_t priority;
    uint16_t port;
    uint32_t uptime;
} service_offer_t;

typedef struct __attribute__((packed)){
    uint32_t service;
    uint32_t group;   

    uint16_t server_priority;
    uint16_t server_port;
    ip_addr4_t server_ip;

    bool is_team;
    uint8_t state;
    uint8_t timeout;
} service_state_t;

typedef struct __attribute__((packed)){
    service_state_t service;

    // additional local information
    uint16_t local_priority;
    uint16_t local_port;
    uint32_t local_uptime;

    // additional leader information (main information is in service)
    uint64_t leader_device_id;
    uint32_t leader_uptime;
} team_state_t;


static list_t offers_list;
static list_t available_list;


void services_v_init( void ){

    list_v_init( &offers_list );
    list_v_init( &available_list );

    #if !defined(ENABLE_NETWORK) || !defined(ENABLE_WIFI)
    return;
    #endif

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }



}

void services_v_listen( uint32_t id, uint32_t group ){

}

void services_v_offer( uint32_t id, uint32_t group, uint16_t priority, uint16_t port ){

}

void services_v_join_team( uint32_t id, uint32_t group, uint16_t priority, uint16_t port ){

}

void services_v_cancel( uint32_t id, uint32_t group ){

}

bool services_b_is_available( uint32_t id, uint32_t group ){

    return FALSE;
}

bool services_b_is_leader( uint32_t id, uint32_t group ){

    return FALSE;
}

sock_addr_t services_a_get( uint32_t service, uint32_t group ){

    sock_addr_t addr;
    memset( &addr, 0, sizeof(addr) );


    return addr;
}

ip_addr4_t services_a_get_ip( uint32_t service, uint32_t group ){

    return ip_a_addr(0,0,0,0);
}

#endif
