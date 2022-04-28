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

#ifdef ESP32

#include "rfm95w.h"
#include "rf_mac.h"

#include "flash_fs.h"
#include "hal_boards.h"

/*



Basic power consumptions:
Rx: 10-12 mA

Tx:
7 dBm:  20 mA
13 dBm: 29 mA
17 dBm: 87 mA

20 dBm is technically available (120 mA), but requires a 1% duty cycle.
We will stick to 17 dBm for simplicity.

Minimum bandwidth is 62.5 KHz.
(below that requires a TCXO, which we don't have)

We are using Band 1 (900 MHz)


FCC requirements:
https://www.sunfiretesting.com/LoRa-FCC-Certification-Guide/

At 17 dBm, we are good on power limits.

For digital spread spectrum with no FHSS, (15.247(a)(2)), 
we must occupy at least 500 KHz of bandwidth at -6 dB.
Our 500 KHz bandwidth probably hits this, but none of the others do.

Below 500 KHz, we must use FHSS.

FHSS requirements:
Channels must be 25 KHz apart at least, or 20 dB bandwidth.

If 20dB bandwidth < 250 KHz, we need >= 50 channels using
at most 400 msec per channel in a 20 second span.
- bandwidths below 250 KHz probably qualify here.

If 20dB bandwidth > 250 KHz, we need >= 25 channels using
at most 400 msec per channel in a 10 second span.
- our 250 KHz bandwidth setting should cover this.

The 50 channel/20 second program should cover everything.


With Lora turned on, technically we qualify for hybrid mode operation.
Technically channel hopping is required, but with no minimum number of channels.
Per channel dwell must be < 400 msec in a 0.4 * channel_count second time period.

LoRaWAN uses hybrid mode with 8 channels on the gateway.

In theory, we could use 2 channels in hybrid mode as long as
we comply with the dwell times and ensure pseudorandom channel
selection.


Lora bitrate is SF * ( BW / (2**SF) ) * CR

Lora

SF  BW      CR    Bitrate  Airtime (64 bytes, 512 bytes) (ms)
12  62.5    4/8   91       5626  45008
12  500     4/8   732      699   5592
12  62.5    4/5   146      3506  28048
12  500     4/5   73       7013  56104

6   62.5    4/8   2929     174   1392
6   500     4/8   23437    21    168
6   62.5    4/5   4687     109   872
6   500     4/5   37500    13    104


For initial low rate telemetry, using a single receiver channel:
500 KHz BW, SF12, 4/8 CR = 732 bps @ -130 dBm
64 bytes = approx 0.7 seconds.



*/

static const uint32_t beacon_channels[RF_MAC_N_BEACON_CH] = {
    905000000,
    910000000,
    915000000,
    920000000,
};

static const rf_mac_coding_t codebook[] = {
    // beacon coding
    { RF_MAC_MOD_LORA, RFM95W_CODING_4_8, 12, RFM95W_BW_500000 }, // 732 bps
};

static uint8_t current_code;

int8_t rf_mac_i8_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_ELITE ){

        if( rfm95w_i8_init( IO_PIN_27_A10, IO_PIN_26_A0 ) < 0 ){

            return -1;
        }

        return 0;
    }

    // no radio present
    return -1;
}

void rf_mac_v_set_code( uint8_t code ){

    if( code >= cnt_of_array(codebook) ){

        return;
    }

    current_code = code;
}


static void reset_fifo( void ){

    rfm95w_v_write_reg( RFM95W_RegFifoTxBaseAddr, 0 );
    rfm95w_v_write_reg( RFM95W_RegFifoRxBaseAddr, 0 );
    rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
}

static uint16_t calc_symbol_duration( uint8_t bw, uint8_t sf ){

    uint32_t bandwidth = 0;

    if( bw == RFM95W_BW_62500 ){

        bandwidth = 62500;
    }
    else if( bw == RFM95W_BW_125000 ){

        bandwidth = 125000;
    }
    else if( bw == RFM95W_BW_250000 ){

        bandwidth = 250000;
    }
    else if( bw == RFM95W_BW_500000 ){

        bandwidth = 50000;
    }
    else{

        ASSERT( FALSE );
    }

    uint32_t symbol_rate = bandwidth / ( 1 << sf );

    return 10000 / symbol_rate;
}

static void configure_code( void ){

    // assumes radio is in standby!

    if( codebook[current_code].modulation == RF_MAC_MOD_LORA ){

        rfm95w_v_set_reg_bits( RFM95W_RegOpMode, RFM95W_BIT_LORA_MODE );
    }
    else{

        ASSERT( FALSE );
    }

    rfm95w_v_set_spreading_factor( codebook[current_code].spreading_factor );
    rfm95w_v_set_coding_rate( codebook[current_code].code_rate );
    rfm95w_v_set_bandwidth( codebook[current_code].bandwidth );


    rfm95w_v_clr_reg_bits( RFM95W_RegDetectOptimize, RFM95W_MASK_DetectionOptimize );

    if( codebook[current_code].spreading_factor == 6 ){

        // note that header must be implicit on SF6

        rfm95w_v_set_reg_bits( RFM95W_RegDetectOptimize, RFM95W_MASK_DetectionOptimize & 0x05 );
        rfm95w_v_write_reg( RFM95W_RegDetectionThreshold, 0x0C );
    }
    else{

        rfm95w_v_set_reg_bits( RFM95W_RegDetectOptimize, RFM95W_MASK_DetectionOptimize & 0x03 );
        rfm95w_v_write_reg( RFM95W_RegDetectionThreshold, 0x0A );
    }

    if( calc_symbol_duration( codebook[current_code].bandwidth, codebook[current_code].spreading_factor ) >= 16 ){

        rfm95w_v_set_reg_bits( RFM95W_RegModemConfig3, RFM95W_BIT_LowDataRateOptimize );    
    }
    else{

        rfm95w_v_clr_reg_bits( RFM95W_RegModemConfig3, RFM95W_BIT_LowDataRateOptimize );
    }
}


#endif