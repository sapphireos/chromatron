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

#define LED_GPIO 2
#define BUF_READY_GPIO 14

typedef struct{
    uint16_t intf_run_time;
    uint16_t intf_max_time;
    uint16_t vm_run_time;
    uint16_t vm_max_time;
    uint16_t wifi_run_time;
    uint16_t wifi_max_time;
    uint16_t mem_run_time;
    uint16_t mem_max_time;
} process_stats_t;

void intf_v_led_on( void );
void intf_v_led_off( void );
void intf_v_init( void );
void intf_v_process( void );
void intf_v_request_status( void );
void intf_v_request_rgb_pix0( void );
void intf_v_request_rgb_array( void );
void intf_v_request_vm_frame_sync( void );
void intf_v_get_mac( uint8_t mac[6] );
void intf_v_printf( const char *format, ... );
int8_t intf_i8_send_msg( uint8_t data_id, uint8_t *data, uint8_t len );
void intf_v_get_proc_stats( process_stats_t **stats );

#endif
