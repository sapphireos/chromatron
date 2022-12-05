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

#include "sapphire.h"
#include "onewire.h"
#include "hal_onewire.h"

#include "driver/rmt.h"

// static rmt_channel_t example_tx_channel = RMT_CHANNEL_0;
// static rmt_channel_t example_rx_channel = RMT_CHANNEL_1;

void hal_onewire_v_init( void ){

    
    // rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(CONFIG_EXAMPLE_RMT_TX_GPIO, example_tx_channel);
    // rmt_tx_config.tx_config.carrier_en = true;
    // rmt_config(&rmt_tx_config);
    // rmt_driver_install(example_tx_channel, 0, 0);

}

void hal_onewire_v_reset( void ){


}

void hal_onewire_v_write_bit( uint8_t bit ){


}

uint8_t hal_onewire_u8_read_bit( void ){

    return 0;
}

