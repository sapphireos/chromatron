// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2022  Jeremy Billheimer
// 
// 
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published
//     by the Free Software Foundation, either version 3 of the License,
//     or (at your option) any later version.
// 
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
// 
//     You should have received a copy of the GNU General Public License
//     along with this program. If not, see <http://www.gnu.org/licenses/>.
// 
// </license>

#ifndef _HAL_SIM_H
#define _HAL_SIM_H

#include <stdint.h>

// the poll() array type is only used by pointer here: including <poll.h>
// in a HAL header would drag the host headers into every sim source file
struct pollfd;

// sim platform init: call after timers are initialized and before the
// thread scheduler starts
void hal_sim_v_init( void );

// idle the process for at most `ms` milliseconds, waking early if a
// simulator socket has data.  This is where the simulator gives the host
// CPU back: the scheduler calls it through cpu_v_sleep() when every
// thread is waiting.
void hal_sim_v_idle( uint32_t ms );

// collect the open simulator UDP sockets into a poll() array; returns
// the number of descriptors filled in (implemented in hal_wifi.c)
int8_t hal_wifi_i8_poll_fds( struct pollfd *pfds, int8_t max_fds );

// longest single idle sleep, in ms: bounds the worst case wakeup latency
// for anything the scheduler is not tracking with an alarm
#define SIM_IDLE_MAX_MS 100

#endif
