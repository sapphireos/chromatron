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


#ifndef _EVENTS_H
#define _EVENTS_H



#define EVENT_ID_NONE                           0
#define EVENT_ID_LOG_INIT                       1
#define EVENT_ID_LOG_RECORD                     2

#define EVENT_ID_SYS_ASSERT                     10
#define EVENT_ID_SIGNAL                         11
#define EVENT_ID_WATCHDOG_KICK                  12

#define EVENT_ID_DEBUG_0                        20
#define EVENT_ID_DEBUG_1                        21
#define EVENT_ID_DEBUG_2                        22
#define EVENT_ID_DEBUG_3                        23
#define EVENT_ID_DEBUG_4                        24
#define EVENT_ID_DEBUG_5                        25
#define EVENT_ID_DEBUG_6                        26
#define EVENT_ID_DEBUG_7                        27

#define EVENT_ID_CPU_SLEEP                      200
#define EVENT_ID_CPU_WAKE                       201

#define EVENT_ID_FFS_GARBAGE_COLLECT            400
#define EVENT_ID_FFS_WEAR_LEVEL                 401

#define EVENT_ID_TIMER_ALARM_MISS               500

#define EVENT_ID_MEM_DEFRAG                     600

#define EVENT_ID_CMD_START                      700
#define EVENT_ID_CMD_TIMEOUT                    701
#define EVENT_ID_CMD_CRC_ERROR                  702

#define EVENT_ID_THREAD_ID                      800
#define EVENT_ID_FUNC_ENTER                     801
#define EVENT_ID_FUNC_EXIT                      802
#define EVENT_ID_STACK_POINTER                  803

#endif
