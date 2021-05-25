/*
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
 */

#ifndef __SUPERCONDUCTOR_H
#define __SUPERCONDUCTOR_H

#define SC_SYNC_INTERVAL	1000

#define SC_MAX_BANKS		4

typedef struct{
	uint8_t msg_type;
	uint8_t reserved[3];
} sc_msg_hdr_t;

typedef struct{
	sc_msg_hdr_t hdr;
	catbus_hash_t32 banks[SC_MAX_BANKS];
} sc_msg_init_t;
#define SC_MSG_TYPE_INIT	1

void sc_v_init( void );

#endif