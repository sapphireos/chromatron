 // <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#ifndef _COMM_INTF_H
#define _COMM_INTF_H


#define MAX_TX_Q_SIZE           16


#define LED_GPIO 2
#define BUF_READY_GPIO 14

extern "C"{
    #include "catbus_common.h"
    #include "gfx_lib.h"
}

typedef struct{
    uint16_t intf_run_time;
    uint16_t intf_max_time;
    uint16_t intf_avg_time;
    uint16_t vm_run_time;
    uint16_t vm_max_time;
    uint16_t vm_avg_time;
    uint16_t wifi_run_time;
    uint16_t wifi_max_time;
    uint16_t wifi_avg_time;
    uint16_t mem_run_time;
    uint16_t mem_max_time;
    uint16_t mem_avg_time;
} process_stats_t;

#define PROCESS_STATS_AVG_FILTER ( 0.1 )

void intf_v_led_on( void );
void intf_v_led_off( void );
void intf_v_init( void );
void intf_v_process( void );
void intf_v_request_status( void );
void intf_v_request_vm_info( void );
#ifndef USE_HSV_BRIDGE
void intf_v_request_rgb_pix0( void );
void intf_v_request_rgb_array( void );
#else
void intf_v_request_hsv_array( void );
#endif
void intf_v_request_vm_frame_sync( void );
void intf_v_get_mac( uint8_t mac[6] );
int8_t intf_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len );
void intf_v_get_proc_stats( process_stats_t **stats );
void intf_v_send_kv( catbus_hash_t32 hash );

#endif
