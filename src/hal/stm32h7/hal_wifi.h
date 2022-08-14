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

#ifndef _HAL_WIFI_H
#define _HAL_WIFI_H

#include "netmsg.h"


#include "wifi_cmd.h"


#define WIFI_WATCHDOG_TIMEOUT   8

int8_t wifi_i8_send_msg( uint8_t data_id, uint8_t *data, uint16_t len );
int8_t wifi_i8_receive_msg( uint8_t data_id, uint8_t *data, uint16_t max_len, uint16_t *bytes_read );

extern int8_t wifi_i8_msg_handler( uint8_t data_id, uint8_t *data, uint16_t len ) __attribute__((weak));

#endif

