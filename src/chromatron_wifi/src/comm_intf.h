// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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
#define IRQ_GPIO 14
#define CTS_GPIO 15

typedef struct{
    uint16_t intf_run_time;
    uint16_t intf_max_time;
    uint16_t intf_avg_time;
    uint16_t wifi_run_time;
    uint16_t wifi_max_time;
    uint16_t wifi_avg_time;
    uint16_t mem_run_time;
    uint16_t mem_max_time;
    uint16_t mem_avg_time;
} process_stats_t;

#define PROCESS_STATS_AVG_FILTER ( 0.1 )

void intf_v_init( void );
void intf_v_process( void );

void intf_v_get_mac( uint8_t mac[6] );
int8_t intf_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len );
void intf_v_get_proc_stats( process_stats_t **stats );

#ifdef __cplusplus
extern "C"
{
#endif
void intf_v_led_on( void );
void intf_v_led_off( void );
#ifdef __cplusplus
}
#endif


#endif
