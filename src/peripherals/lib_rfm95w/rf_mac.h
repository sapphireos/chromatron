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

#include "rfm95w.h"

#define RF_MAC_N_BEACON_CH  4

#define RF_MAC_MAX_TX_Q     8
#define RF_MAC_MAX_RX_Q     8

#define RF_MAC_MOD_LORA     0
#define RF_MAC_MOD_FSK      1
#define RF_MAC_MOD_GFSK     2

#define RF_MAC_MAGIC 0x12345678

typedef struct __attribute__((packed)){
    uint32_t magic;
    uint8_t flags;
    uint64_t src_addr;
} rf_mac_header_0_t;


typedef struct{
    uint8_t modulation;
    uint8_t code_rate;
    uint8_t spreading_factor;
    uint8_t bandwidth;
} rf_mac_coding_t;

#define RF_MAC_BEACON_CODE  0


typedef struct{
    uint64_t dest_addr;
    uint8_t len;
} rf_mac_tx_pkt_t;

typedef struct{
    int16_t rssi;
    uint8_t len;
} rf_mac_rx_pkt_t;


int8_t rf_mac_i8_init( void );
void rf_mac_v_set_code( uint8_t code );
int8_t rf_mac_i8_send( uint64_t dest_addr, uint8_t *data, uint8_t len );
int8_t rf_mac_i8_get_rx( rf_mac_rx_pkt_t *pkt, uint8_t *ptr, uint8_t max_len );
bool rf_mac_b_rx_available( void );

#endif