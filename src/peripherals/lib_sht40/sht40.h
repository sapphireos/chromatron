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

#ifndef _SHT40_H_
#define _SHT40_H_


#define SHT40_I2C_ADDR			0x44

#define SHT40_CMD_MEAS_HP		0xFD
#define SHT40_CMD_MEAS_MP		0xF6
#define SHT40_CMD_MEAS_LP		0xE0
#define SHT40_CMD_READ_SERIAL	0x89
#define SHT40_CMD_RESET			0x94

void sht40_v_init( void );
uint32_t sht40_u32_read_serial( void );
void sht40_v_meas_raw( int16_t *temp, int16_t *RH );

#endif
