/*
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
 */

#ifndef _RF_MAC_H_
#define _RF_MAC_H_

#define RF_MAC_N_BEACON_CH  4

#define RF_MAC_MOD_LORA     0
#define RF_MAC_MOD_FSK      1
#define RF_MAC_MOD_GFSK     2

typedef struct{
    uint8_t modulation;
    uint8_t code_rate;
    uint8_t spreading_factor;
    uint8_t bandwidth;
} rf_mac_coding_t;

#define RF_MAC_BEACON_CODE  0


int8_t rf_mac_i8_init( void );
void rf_mac_v_set_code( uint8_t code );


#endif