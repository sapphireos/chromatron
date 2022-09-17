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

#ifndef _MEMTYPES_H
#define _MEMTYPES_H

typedef uint8_t mem_type_t8;
#define MEM_TYPE_UNKNOWN            0
#define MEM_TYPE_NETMSG             1
#define MEM_TYPE_THREAD             5
#define MEM_TYPE_FILE               6
#define MEM_TYPE_SOCKET             7
#define MEM_TYPE_SOCKET_BUFFER      8
#define MEM_TYPE_LOG_BUFFER         10
#define MEM_TYPE_CMD_BUFFER         11
#define MEM_TYPE_CMD_REPLY_BUFFER   12
#define MEM_TYPE_FILE_HANDLE        13
#define MEM_TYPE_FS_BLOCKS          14
#define MEM_TYPE_KV_LINK            15
#define MEM_TYPE_KV_SEND            16
#define MEM_TYPE_KV_RX_CACHE        17
#define MEM_TYPE_CATBUS_LINK        15
#define MEM_TYPE_CATBUS_SEND        16
#define MEM_TYPE_CATBUS_RX_CACHE    17
#define MEM_TYPE_KVDB_ENTRY         18
#define MEM_TYPE_SUBSCRIBED_KEYS    19
#define MEM_TYPE_VM_DATA            20
#define MEM_TYPE_CRON_JOB           21
#define MEM_TYPE_ELECTION           22
#define MEM_TYPE_DNS_QUERY          23
#define MEM_TYPE_MSGFLOW            24
#define MEM_TYPE_MSGFLOW_ARQ_BUF    25
#define MEM_TYPE_SERVICE            26
#define MEM_TYPE_LINK_CONSUMER      27
#define MEM_TYPE_LINK_PRODUCER      28
#define MEM_TYPE_LINK_REMOTE        29
#define MEM_TYPE_KV_OPT             30

#endif
