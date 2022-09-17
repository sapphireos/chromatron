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


#ifndef _EVENTS_H
#define _EVENTS_H



#define EVENT_ID_EVT_LOG_INIT                   __KV__evt_log_init
#define EVENT_ID_EVT_LOG_RECORD                 __KV__evt_log_record
#define EVENT_ID_LOG_RECORD                     __KV__log_record

#define EVENT_ID_SYS_ASSERT                     __KV__sys_assert
#define EVENT_ID_SIGNAL                         __KV__signal
#define EVENT_ID_WATCHDOG_KICK                  __KV__watchdog_kick

#define EVENT_ID_FFS_GARBAGE_COLLECT            __KV__ffs_garbage_collect
#define EVENT_ID_FFS_WEAR_LEVEL                 __KV__ffs_wear_level

#define EVENT_ID_MEM_DEFRAG                     __KV__mem_defrag

#endif
