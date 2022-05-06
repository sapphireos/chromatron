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

#include "config.h"

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

static list_t tx_q;
static list_t rx_q;


PT_THREAD( rf_thread( pt_t *pt, void *state ) );


int8_t rf_mac_i8_init( void ){

    list_v_init( &tx_q );
    list_v_init( &rx_q );

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_ELITE ){

        if( rfm95w_i8_init( IO_PIN_27_A10, IO_PIN_26_A0 ) < 0 ){

            return -1;
        }
    }
    else{

        return -1;
    }


    thread_t_create( rf_thread,
                     PSTR("rf_mac"),
                     0,
                     0 );

    
    return 0;
}

void rf_mac_v_set_code( uint8_t code ){

    if( code >= cnt_of_array(codebook) ){

        return;
    }

    current_code = code;
}

int8_t rf_mac_i8_send( uint64_t dest_addr, uint8_t *data, uint8_t len ){

    if( list_u8_count( &tx_q ) >= RF_MAC_MAX_TX_Q ){

        return -1;
    }

    uint8_t buf[sizeof(rf_mac_tx_pkt_t) + RFM95W_FIFO_LEN];

    rf_mac_tx_pkt_t *pkt = (rf_mac_tx_pkt_t *)buf;
    uint8_t *ptr = &buf[sizeof(rf_mac_tx_pkt_t)];

    pkt->dest_addr = dest_addr;
    pkt->len = len;
    memcpy( ptr, data, len );

    list_node_t ln = list_ln_create_node( buf, len + sizeof(rf_mac_tx_pkt_t) );

    if( ln < 0 ){

        return -2;
    }

    list_v_insert_head( &tx_q, ln );

    return 0;
}

// return -1 if q is empty
// otherwise, return number of q elements remaining
int8_t rf_mac_i8_get_rx( rf_mac_rx_pkt_t *pkt, uint8_t *ptr, uint8_t max_len ){

    uint8_t count = list_u8_count( &rx_q );

    if( count == 0 ){

        return -1;
    }

    list_node_t ln = list_ln_remove_tail( &rx_q );
    rf_mac_rx_pkt_t *rx_pkt = list_vp_get_data( ln );
    uint8_t *data = (uint8_t *)( rx_pkt + 1 );

    list_v_release_node( ln );

    if( rx_pkt->len > max_len ){

        return -2;
    }

    *pkt = *rx_pkt;
    memcpy( ptr, data, rx_pkt->len );

    return count - 1;
}

bool rf_mac_b_rx_available( void ){

    return !list_b_is_empty( &rx_q );
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



static void reset_fifo( void ){

    rfm95w_v_write_reg( RFM95W_RegFifoTxBaseAddr, 0 );
    rfm95w_v_write_reg( RFM95W_RegFifoRxBaseAddr, 0 );
    rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
}



PT_THREAD( rf_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    rfm95w_v_set_preamble_length( RFM95W_PREAMBLE_MIN );
    
    while( 1 ){

        TMR_WAIT( pt, 10 );

        // set up for receive
        rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );
        configure_code();
        rfm95w_v_set_frequency( beacon_channels[0] );

        rfm95w_v_clear_irq_flags();

        rfm95w_v_set_mode( RFM95W_OP_MODE_RXCONT );

        THREAD_WAIT_WHILE( pt, !rfm95w_b_is_rx_done() && list_b_is_empty( &tx_q ) );

        // check if receiving, but not complete:
        if( rfm95w_b_is_rx_busy() ){

            // wait until done
            THREAD_WAIT_WHILE( pt, !rfm95w_b_is_rx_done() );
        }

        // check if receive or tx path
        if( rfm95w_b_is_rx_done() ){
            // RECEIVE

            if( !rfm95w_b_is_rx_ok() ){

                // error
                bool payload_crc_error = ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_PayloadCrcError ) != 0;
                bool header_error = ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_ValidHeader ) == 0;

                // lora_rx_errors++;

                if( payload_crc_error ){

                    log_v_debug_P( PSTR("payload crc error") );    
                }

                if( header_error ){

                    log_v_debug_P( PSTR("header error") );    
                }
                

                continue;
            }


            // retrieve packet from FIFO
            uint8_t buf[RFM95W_FIFO_LEN];
            uint8_t rx_len = rfm95w_u8_read_rx_len();
            
            if( rx_len >= sizeof(buf) ){

                // invalid length
                continue;
            }

            rfm95w_v_read_fifo( buf, rx_len );

            reset_fifo();


            if( list_u8_count( &rx_q ) >= RF_MAC_MAX_RX_Q ){

                // q is overloaded
                continue;
            }

            int16_t pkt_rssi = rfm95w_i16_get_packet_rssi();

            rf_mac_rx_pkt_t rx_pkt;

            rx_pkt.rssi = pkt_rssi;
            rx_pkt.len = rx_len;

            list_node_t ln = list_ln_create_node( 0, rx_len + sizeof(rf_mac_rx_pkt_t) );

            if( ln < 0 ){

                continue;
            }

            uint8_t *ptr = list_vp_get_data( ln );

            memcpy( ptr, &rx_pkt, sizeof(rx_pkt) );
            ptr += sizeof(rx_pkt);
            memcpy( ptr, buf, rx_len );

            list_v_insert_head( &rx_q, ln );
        }
        else if( !list_b_is_empty( &tx_q ) ){
            // TRANSMIT

            rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );

            reset_fifo();

            list_node_t ln = list_ln_remove_tail( &tx_q );

            rf_mac_tx_pkt_t *pkt = (rf_mac_tx_pkt_t *)list_vp_get_data( ln );
            uint8_t *data = (uint8_t *)( pkt + 1 );

            rf_mac_header_0_t header = {
                RF_MAC_MAGIC,
                0,
                cfg_u64_get_device_id(),
            };

            rfm95w_v_write_fifo( (uint8_t *)&header, sizeof(header) );
            rfm95w_v_write_fifo( data, pkt->len );

            rfm95w_v_write_reg( RFM95W_RegPayloadLength, sizeof(header) + pkt->len );

            list_v_release_node( ln );

            if( pkt->dest_addr == 0 ){

                // transmit to base station


                // need to select a channel
                rfm95w_v_set_frequency( beacon_channels[0] );

                // set up coding
                configure_code();

                rfm95w_v_write_fifo( data, pkt->len );
            }
            else{

                continue;
            }

            rfm95w_v_set_power( RFM95W_POWER_MAX );

            rfm95w_v_transmit();

            THREAD_WAIT_WHILE( pt, !rfm95w_b_is_tx_done() );
        }
    }

PT_END( pt );
}




#endif