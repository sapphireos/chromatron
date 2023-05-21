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

#include "spi.h"

#include "rfm95w.h"
#include "util.h"
#include "logging.h"

#if defined(ESP32)

static uint8_t cs_gpio;
static uint8_t reset_gpio;

#define CHIP_ENABLE()       io_v_digital_write( cs_gpio, 0 ); _delay_us( 1 )
#define CHIP_DISABLE()      io_v_digital_write( cs_gpio, 1 ); _delay_us( 1 )

#define WRITE_BIT 0x80

PT_THREAD( lora_tx_thread( pt_t *pt, void *state ) );
PT_THREAD( lora_rx_thread( pt_t *pt, void *state ) );

/*

LoRa

Implementation notes for the SapphireOS LoRa driver:

Explicit mode only for packets.  Since the lowest spreading factor (6)
requires implicit mode, the effective lowest SF for this driver is 7.

We're aiming for robustness and range over raw speed.

Bandwidths below 62.5 khz are not recommended as the
HopeRF module does not have a TCXO.

The LowDataRateOptimize bit needs to be set for symbol rates longer than 16 ms.
That's symbol duration, not packet duration.
This would be an extremely low bit rate, 62.5 bps.

Payload capacity is 1 to 255 bytes.

No support for channel hopping at this time.

FIFO cannot be accessed in sleep mode.
Can only write to FIFO in standby mode.

The RFM95 has a 32MHz crystal.

Coding rate can change on the fly, as the packet header will indicate
the payload coding rate.  All other parameters (other than transmit power)
must be pre-configured.

The RFM95 uses the PA_BOOST pin.

*/

#define RFM95_SPI_CHANNEL USER_SPI_CHANNEL

// static uint32_t lora_rx;
// static int16_t lora_rssi;
// static uint32_t lora_tx;
// static uint32_t lora_rx_errors;


// KV_SECTION_META kv_meta_t rfm95w_kv[] = {
//     { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY,     &lora_rx,       0, "lora_rx" },
//     { CATBUS_TYPE_INT16,    0, KV_FLAGS_READ_ONLY,     &lora_rssi,     0, "lora_rssi" },
//     { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY,     &lora_tx,       0, "lora_tx" },
//     { CATBUS_TYPE_UINT32,   0, KV_FLAGS_READ_ONLY,     &lora_rx_errors,0, "lora_rx_errors" },
// };

// static bool rx_ready;
// static bool tx_busy;

int8_t rfm95w_i8_init( uint8_t cs, uint8_t reset ){

    cs_gpio = cs;
    reset_gpio = reset;

    // set up CS pin
    io_v_set_mode( cs_gpio, IO_MODE_OUTPUT );

    // set up reset pin
    io_v_set_mode( reset_gpio, IO_MODE_OUTPUT );
    io_v_digital_write( reset_gpio, 1 );
    
    CHIP_DISABLE();

    // spi_v_init( RFM95_SPI_CHANNEL, 10000000, 0 ); // !!! 10MHz can cause bit errors on the bus itself!
    spi_v_init( RFM95_SPI_CHANNEL, 1000000, 0 );

    // log_v_debug_P( PSTR("0x%x = 0x%x"), RFM95W_RegOpMode, rfm95w_u8_read_reg( RFM95W_RegOpMode ) );
    // log_v_debug_P( PSTR("0x%x = 0x%x"), RFM95W_RegPaConfig, rfm95w_u8_read_reg( RFM95W_RegPaConfig ) );

    // clear op mode register
    rfm95w_v_write_reg( RFM95W_RegOpMode, 0 );

    // radio wakes up in FSK mode and Standby state.
    // we need to change to LoRa mode, but we need
    // to go to sleep state before we can do that.
    rfm95w_v_set_mode( RFM95W_OP_MODE_SLEEP );

    _delay_ms( 10 );

    rfm95w_v_set_reg_bits( RFM95W_RegOpMode, RFM95W_BIT_LORA_MODE );

    // go back to standby
    rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );

    // set FIFO base addresses
    rfm95w_v_write_reg( RFM95W_RegFifoTxBaseAddr, 0 );
    rfm95w_v_write_reg( RFM95W_RegFifoRxBaseAddr, 0 );
    rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );

    uint8_t buf[4] = {1, 2, 3, 4};
    rfm95w_v_write_fifo( buf, sizeof(buf) );
    memset( buf, 0, sizeof(buf) );
    rfm95w_v_read_fifo( buf, sizeof(buf) );

    if( ( buf[0] != 1 ) ||
        ( buf[1] != 2 ) ||
        ( buf[2] != 3 ) ||
        ( buf[3] != 4 ) ){

        return -1;
    }

    log_v_debug_P( PSTR("LORA module detected") );


    rfm95w_v_set_preamble_length( 12 );

    // enable packet CRC
    rfm95w_v_set_reg_bits( RFM95W_RegModemConfig2, RFM95W_BIT_RxPayloadCrcOn );

    // set RF frequency
    // rfm95w_v_set_frequency( RFM95W_FREQ_MIN );
    rfm95w_v_set_frequency( 915000000 );

    // set PABOOST mode
    rfm95w_v_set_reg_bits( RFM95W_RegPaConfig, RFM95W_BIT_PABOOST );

    // set to explicit header mode
    rfm95w_v_clr_reg_bits( RFM95W_RegModemConfig, RFM95W_BIT_ImplicitHeader );

    // FAST:
    // rfm95w_v_set_spreading_factor( 7 );
    // rfm95w_v_set_coding_rate( RFM95W_CODING_4_5 );
    // rfm95w_v_set_bandwidth( RFM95W_BW_500000 );

    // SLOW:
    rfm95w_v_set_spreading_factor( 12 );
    rfm95w_v_set_coding_rate( RFM95W_CODING_4_8 );
    rfm95w_v_set_bandwidth( RFM95W_BW_125000 );

    // set low data rate optimization
    rfm95w_v_set_reg_bits( RFM95W_RegModemConfig3, RFM95W_BIT_LowDataRateOptimize );


    rfm95w_v_set_power( RFM95W_POWER_MAX );


    // log_v_debug_P( PSTR("0x%x = 0x%x"), RFM95W_RegOpMode, rfm95w_u8_read_reg( RFM95W_RegOpMode ) );
    // log_v_debug_P( PSTR("0x%x = 0x%x"), RFM95W_RegPaConfig, rfm95w_u8_read_reg( RFM95W_RegPaConfig ) );
    

    // for( uint8_t i = 0; i < 64; i++ ){
        
        // _delay_ms( 10 );

        // rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
        // rfm95w_v_write_fifo( 0, 255 );
        // rfm95w_v_write_reg( RFM95W_RegPayloadLength, 255 );
        // rfm95w_v_write_reg( RFM95W_RegIrqFlags, RFM95W_BIT_TxDone );

        // rfm95w_v_set_reg_bits( RFM95W_RegModemConfig2, RFM95W_BIT_TxContinuousMode );

        // rfm95w_v_set_mode( RFM95W_OP_MODE_TX );

        // SAFE_BUSY_WAIT( ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_TxDone ) == 0 );
    // }


    // rfm95w_v_set_mode( RFM95W_OP_MODE_RXCONT );

    // log_v_debug_P( PSTR("%d"), rfm95w_u8_get_rssi() );
    // log_v_debug_P( PSTR("%d"), rfm95w_u8_get_rssi() );
    // log_v_debug_P( PSTR("%d"), rfm95w_u8_get_rssi() );
    // log_v_debug_P( PSTR("%d"), rfm95w_u8_get_rssi() );


    // thread_t_create( lora_tx_thread,
    //                  PSTR("lora_tx"),
    //                  0,
    //                  0 );

    // thread_t_create( lora_rx_thread,
    //                  PSTR("lora_rx"),
    //                  0,
    //                  0 );

    return 0;
}

uint8_t rfm95w_u8_read_reg( uint8_t addr ){

    CHIP_ENABLE();

    spi_u8_send( RFM95_SPI_CHANNEL, addr );

    uint8_t temp = spi_u8_send( RFM95_SPI_CHANNEL, 0 );

    CHIP_DISABLE();

    return temp;
}

void rfm95w_v_write_reg( uint8_t addr, uint8_t data ){

    // set write command
    addr |= WRITE_BIT;

    CHIP_ENABLE();

    spi_u8_send( RFM95_SPI_CHANNEL, addr );
    spi_u8_send( RFM95_SPI_CHANNEL, data );

    CHIP_DISABLE();
}

void rfm95w_v_set_reg_bits( uint8_t addr, uint8_t mask ){

    uint8_t data = rfm95w_u8_read_reg( addr );
    data |= mask;
    rfm95w_v_write_reg( addr, data );
}

void rfm95w_v_clr_reg_bits( uint8_t addr, uint8_t mask ){

    uint8_t data = rfm95w_u8_read_reg( addr );
    data &= ~mask;
    rfm95w_v_write_reg( addr, data );
}

void rfm95w_v_read_fifo( uint8_t *buf, uint8_t len ){

    rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );

    CHIP_ENABLE();

    spi_u8_send( RFM95_SPI_CHANNEL, RFM95W_RegFifo );
    spi_v_read_block( RFM95_SPI_CHANNEL, buf, len );

    CHIP_DISABLE();
}

void rfm95w_v_write_fifo( uint8_t *buf, uint8_t len ){

    // rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );

    CHIP_ENABLE();

    spi_u8_send( RFM95_SPI_CHANNEL, RFM95W_RegFifo | WRITE_BIT );
    spi_v_write_block( RFM95_SPI_CHANNEL, buf, len );

    CHIP_DISABLE();

    // rfm95w_v_write_reg( RFM95W_RegPayloadLength, len );
}

void rfm95w_v_clear_fifo( void ){

    uint8_t buf[RFM95W_FIFO_LEN];    
    memset( buf, 0, sizeof(buf) );

    rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );

    rfm95w_v_write_fifo( buf, sizeof(buf) );
}

void rfm95w_v_dump_fifo( void ){

    uint8_t buf[RFM95W_FIFO_LEN];    

    rfm95w_v_read_fifo( buf, sizeof(buf) );

    uint16_t index = 0;

    log_v_debug_P( PSTR("fifo:") );

    for( uint8_t i = 0; i < 8; i++ ){

        log_v_debug_P( PSTR("0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x"),
            buf[index + 0],
            buf[index + 1],
            buf[index + 2],
            buf[index + 3],
            buf[index + 4],
            buf[index + 5],
            buf[index + 6],
            buf[index + 7]
        );

        index += 8;
    }
}

void rfm95w_v_set_mode( uint8_t mode ){

    uint8_t data = rfm95w_u8_read_reg( RFM95W_RegOpMode );

    data &= ~RFM95W_OP_MODE_MASK;
    data |= ( mode & RFM95W_OP_MODE_MASK );

    rfm95w_v_write_reg( RFM95W_RegOpMode, data );
}


/*

Note that if the lowest spreading factor (6) is used, there are 
special considerations.  See section 4.1.1.2 in the data sheet.

*/

void rfm95w_v_set_spreading_factor( uint8_t data ){

    if( data < RFM95W_SPREADING_MIN ){

        return;
    }
    else if( data > RFM95W_SPREADING_MAX ){

        return;
    }

    rfm95w_v_clr_reg_bits( RFM95W_RegModemConfig2, RFM95W_MASK_SF << RFM95W_SHIFT_SF );
    rfm95w_v_set_reg_bits( RFM95W_RegModemConfig2, ( data & RFM95W_MASK_SF ) << RFM95W_SHIFT_SF );
}


void rfm95w_v_set_coding_rate( uint8_t data ){
        
    if( data < RFM95W_CODING_MIN ){

        return;
    }
    else if( data > RFM95W_CODING_MAX ){

        return;
    }

    rfm95w_v_clr_reg_bits( RFM95W_RegModemConfig, RFM95W_MASK_CODING_RATE << RFM95W_SHIFT_CODING_RATE );
    rfm95w_v_set_reg_bits( RFM95W_RegModemConfig, ( data & RFM95W_MASK_CODING_RATE ) << RFM95W_SHIFT_CODING_RATE );   
}

void rfm95w_v_set_bandwidth( uint8_t data ){
        
    rfm95w_v_clr_reg_bits( RFM95W_RegModemConfig, RFM95W_MASK_BW << RFM95W_SHIFT_BW );
    rfm95w_v_set_reg_bits( RFM95W_RegModemConfig, ( data & RFM95W_MASK_BW ) << RFM95W_SHIFT_BW );   
}

void rfm95w_v_set_power( int8_t data ){

    if( data < RFM95W_POWER_MIN ){

        data = RFM95W_POWER_MIN;
    }
    else if( data > RFM95W_POWER_MAX ){

        data = RFM95W_POWER_MAX;
    }

    data -= 2;
    
    rfm95w_v_clr_reg_bits( RFM95W_RegPaConfig, RFM95W_MASK_OUT_POWER );
    rfm95w_v_set_reg_bits( RFM95W_RegPaConfig, data & RFM95W_MASK_OUT_POWER );

    // log_v_debug_P( PSTR("%x"), rfm95w_u8_read_reg( RFM95W_RegPaConfig ) );
}

// can only set frequency in sleep or standby mode
void rfm95w_v_set_frequency( uint32_t freq ){
        
    if( freq < RFM95W_FREQ_MIN ){

        return;
    }
    else if( freq > RFM95W_FREQ_MAX ){

        return;
    }

    uint32_t data = ( (uint64_t)freq * 524288 ) / RFM95W_FOSC;

    // log_v_debug_P( PSTR("%lu %lu"), freq, data );

    rfm95w_v_write_reg( RFM95W_RegFrMsb, ( data >> 16 ) & 0xff );
    rfm95w_v_write_reg( RFM95W_RegFrMid, ( data >> 8  ) & 0xff );
    rfm95w_v_write_reg( RFM95W_RegFrLsb, ( data >> 0  ) & 0xff );

    // log_v_debug_P( PSTR("%x"), rfm95w_u8_read_reg( RFM95W_RegFrMsb ) );
    // log_v_debug_P( PSTR("%x"), rfm95w_u8_read_reg( RFM95W_RegFrMid ) );
    // log_v_debug_P( PSTR("%x"), rfm95w_u8_read_reg( RFM95W_RegFrLsb ) );
    
}

int16_t rfm95w_i16_get_rssi( void ){

    int16_t rssi = rfm95w_u8_read_reg( RFM95W_RegRssiValue );

    return -157 + rssi;
}


int16_t rfm95w_i16_get_packet_rssi( void ){

    int16_t rssi = rfm95w_u8_read_reg( RFM95W_RegPktRssiValue );

    return -157 + rssi;
}

int16_t rfm95w_i16_get_packet_snr( void ){

    int8_t snr = rfm95w_u8_read_reg( RFM95W_RegPktSnrValue );

    return snr / 4;
}

void rfm95w_v_set_preamble_length( uint16_t data ){

    if( data < RFM95W_PREAMBLE_MIN ){

        data = RFM95W_PREAMBLE_MIN;
    }
    
    data -= 4;
        
    rfm95w_v_write_reg( RFM95W_RegPreambleMsb, ( data >> 8 ) & 0xff );
    rfm95w_v_write_reg( RFM95W_RegPreambleLsb, ( data >> 0 ) & 0xff );
}


void rfm95w_v_set_sync_word( uint8_t data ){

    rfm95w_v_write_reg( RFM95W_RegSyncWord, data );
}

void rfm95w_v_clear_irq_flags( void ){

    rfm95w_v_write_reg( RFM95W_RegIrqFlags, 0xff );   
}

bool rfm95w_b_is_rx_done( void ){

    return ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_RxDone ) != 0;
}

bool rfm95w_b_is_rx_busy( void ){

    uint8_t modem_stat = rfm95w_u8_read_reg( RFM95W_RegModemStat );

    return ( ( modem_stat & RFM95W_BIT_SignalDetected ) != 0 ) ||
           ( ( modem_stat & RFM95W_BIT_SignalSync ) != 0 );
}

bool rfm95w_b_is_rx_ok( void ){

    uint8_t payload_crc_error = ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_PayloadCrcError ) != 0;
    uint8_t header_error = ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_ValidHeader ) == 0;

    return !( payload_crc_error || header_error );
}

bool rfm95w_b_is_tx_done( void ){

    return ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_TxDone ) != 0;
}

// bool rfm95w_b_is_rx_ready( void ){

//     return rx_ready;
// }

// bool rfm95w_b_is_tx_busy( void ){

//     return tx_busy;
// }

// bool rfm95w_b_is_tx_ready( void ){

//     return !rfm95w_b_is_rx_busy() && !tx_busy;
// }

uint8_t rfm95w_u8_read_rx_len( void ){

    return rfm95w_u8_read_reg( RFM95W_RegRxNbBytes );
}

void rfm95w_v_get_rx_data( uint8_t *data, uint8_t len ){

    rfm95w_v_read_fifo( data, len );

    // rx_ready = FALSE;

    rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
    rfm95w_v_set_mode( RFM95W_OP_MODE_SLEEP );
    rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );
}

// bool rfm95w_b_transmit( uint8_t *data, uint8_t len ){

//     // check if radio is busy
//     if( rfm95w_b_is_tx_busy() ){

//         return FALSE;
//     }

//     if( rfm95w_b_is_rx_busy() ){

//         return FALSE;
//     }

//     // standby mode so a receive packet doesn't corrupt the fifo
//     rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );

//     rfm95w_v_write_fifo( data, len );
//     rfm95w_v_clear_irq_flags();
//     rfm95w_v_set_mode( RFM95W_OP_MODE_TX );

//     // tx_busy = TRUE;

//     return TRUE;
// }

void rfm95w_v_transmit( void ){

    rfm95w_v_clear_irq_flags();
    rfm95w_v_set_mode( RFM95W_OP_MODE_TX );
}


// PT_THREAD( lora_tx_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
    
//     while( 1 ){

//         // THREAD_WAIT_WHILE( pt, !tx_busy );

        
//         uint8_t buf[255];
//         for( uint8_t i = 0; i < sizeof(buf); i++ ){

//             buf[i] = i;
//         }


//         rfm95w_b_transmit( buf, sizeof(buf) );

//         THREAD_WAIT_WHILE( pt, !rfm95w_b_is_tx_done() );


//         TMR_WAIT( pt, 10 );


//         // // return to receiver mode
//         // rfm95w_v_clear_irq_flags();
//         // rfm95w_v_set_mode( RFM95W_OP_MODE_RXCONT );

//         // lora_tx++;
//         // tx_busy = FALSE;


//         // // THREAD_YIELD( pt );

//         // // TMR_WAIT( pt, 1000 );

//         // uint8_t buf[255];
//         // for( uint8_t i = 0; i < sizeof(buf); i++ ){

//         //     buf[i] = i;
//         // }

//         // rfm95w_v_write_fifo( buf, sizeof(buf) );
//         // rfm95w_v_write_reg( RFM95W_RegIrqFlags, RFM95W_BIT_TxDone );
//         // rfm95w_v_set_mode( RFM95W_OP_MODE_TX );


//         // // rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );

//         // // rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
//         // // rfm95w_v_write_fifo( 0, 255 );
//         // // rfm95w_v_write_reg( RFM95W_RegPayloadLength, 255 );
//         // // rfm95w_v_write_reg( RFM95W_RegIrqFlags, RFM95W_BIT_TxDone );

//         // // // rfm95w_v_set_mode( RFM95W_OP_MODE_FSTX );

//         // // // _delay_ms(1);

//         // // // rfm95w_v_set_reg_bits( RFM95W_RegModemConfig2, RFM95W_BIT_TxContinuousMode );

//         // // rfm95w_v_set_mode( RFM95W_OP_MODE_TX );

//         // THREAD_WAIT_WHILE( pt, ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_TxDone ) == 0 );
//     }

// PT_END( pt );
// }


// PT_THREAD( lora_rx_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
    
//     while( 1 ){
        
//         THREAD_YIELD( pt );


//         rfm95w_v_clear_irq_flags();
//         rfm95w_v_set_mode( RFM95W_OP_MODE_RXCONT );

//         THREAD_WAIT_WHILE( pt, !rfm95w_b_is_rx_done() );

//         if( !rfm95w_b_is_rx_ok() ){

//             // error
//             // uint8_t payload_crc_error = ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_PayloadCrcError ) != 0;
//             // uint8_t header_error = ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_ValidHeader ) == 0;

//             lora_rx_errors++;

//             continue;
//         }

//         lora_rx++;
//         rx_ready = TRUE;
//         lora_rssi = rfm95w_i16_get_packet_rssi();


//         THREAD_WAIT_WHILE( pt, rx_ready );




//         // THREAD_YIELD( pt );

//         // rfm95w_v_write_reg( RFM95W_RegIrqFlags, 0xff  );
//         // rfm95w_v_set_mode( RFM95W_OP_MODE_RXCONT );

//         // THREAD_WAIT_WHILE( pt, ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_RxDone ) == 0 );

//         // uint8_t flags = rfm95w_u8_read_reg( RFM95W_RegIrqFlags );
//         // uint8_t rx_len = rfm95w_u8_read_reg( RFM95W_RegRxNbBytes );
//         // uint8_t modem_stat = rfm95w_u8_read_reg( RFM95W_RegModemStat );
//         // int16_t pkt_rssi = -157 + rfm95w_u8_read_reg( RFM95W_RegPktRssiValue );


//         // uint8_t buf[16];
//         // memset( buf, 0, sizeof(buf) );

//         // rfm95w_v_read_fifo( buf, sizeof(buf) );

//         // bool data_valid = TRUE;
//         // for( uint8_t i = 0; i < sizeof(buf); i++ ){

//         //     if( buf[i] != i ){

//         //         log_v_debug_P( PSTR("invalid data: %x != %x"), buf[i], i );

//         //         data_valid = FALSE;

//         //         break;
//         //     }
//         // }

//         // log_v_debug_P( PSTR("Flags: 0x%02x len: %d Stat: 0x%02x RSSI: %d Data: %d"), flags, rx_len, modem_stat, pkt_rssi, data_valid );
    

//         // reset FIFO
//         // rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
//         // rfm95w_v_set_mode( RFM95W_OP_MODE_SLEEP );
//         // rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );


//         // TMR_WAIT( pt, 100 );


//         // rfm95w_v_set_mode( RFM95W_OP_MODE_STANDBY );

//         // rfm95w_v_write_reg( RFM95W_RegFifoAddrPtr, 0 );
//         // rfm95w_v_write_fifo( 0, 255 );
//         // rfm95w_v_write_reg( RFM95W_RegPayloadLength, 255 );
//         // rfm95w_v_write_reg( RFM95W_RegIrqFlags, RFM95W_BIT_TxDone );

//         // // rfm95w_v_set_mode( RFM95W_OP_MODE_FSTX );

//         // // _delay_ms(1);

//         // // rfm95w_v_set_reg_bits( RFM95W_RegModemConfig2, RFM95W_BIT_TxContinuousMode );

//         // rfm95w_v_set_mode( RFM95W_OP_MODE_TX );

//         // THREAD_WAIT_WHILE( pt, ( rfm95w_u8_read_reg( RFM95W_RegIrqFlags ) & RFM95W_BIT_TxDone ) == 0 );
//     }

// PT_END( pt );
// }

#else

void rfm95w_v_init( uint8_t cs, uint8_t reset ){

    
}

#endif