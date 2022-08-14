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

#include "system.h"
#include "timers.h"

#include "hal_dma.h"

/*

DMA accelerated memcpy and CRC routines

*/

void dma_v_memcpy( void *dest, const void *src, uint16_t len ){

    DMA.INTFLAGS = DMA_CHTRNIF | DMA_CHERRIF;

    DMA.DMA_CH.REPCNT = 1;
    DMA.DMA_CH.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_INC_gc;
    DMA.DMA_CH.TRIGSRC = DMA_CH_TRIGSRC_OFF_gc;
    DMA.DMA_CH.TRFCNT = len;

    DMA.DMA_CH.DESTADDR0 = ( ( (uint16_t)dest ) >> 0 ) & 0xFF;
    DMA.DMA_CH.DESTADDR1 = ( ( (uint16_t)dest ) >> 8 ) & 0xFF;
    DMA.DMA_CH.DESTADDR2 = 0;

    DMA.DMA_CH.SRCADDR0 = ( ( (uint16_t)src ) >> 0 ) & 0xFF;
    DMA.DMA_CH.SRCADDR1 = ( ( (uint16_t)src ) >> 8 ) & 0xFF;
    DMA.DMA_CH.SRCADDR2 = 0;

    DMA.DMA_CH.CTRLA = DMA_CH_ENABLE_bm | DMA_CH_BURSTLEN_4BYTE_gc;
    DMA.DMA_CH.CTRLA |= DMA_CH_TRFREQ_bm;

    BUSY_WAIT( ( DMA.INTFLAGS & DMA_CHTRNIF ) == 0 );

    DMA.DMA_CH.CTRLA = 0;
}


uint16_t dma_u16_crc( uint8_t *ptr, uint16_t len ){

    // reset CRC to 0xFFFF and IO interface
    CRC.CTRL = CRC_RESET_RESET1_gc;
    asm volatile("nop"); // nop to allow one cycle delay for CRC to reset.

    // change initial value to 0x1D0F, as this is what our existing
    // tools are using.
    CRC.CHECKSUM1 = 0x1D;
    CRC.CHECKSUM0 = 0x0F;
    CRC.CTRL = CRC_SOURCE_DMAC3_gc;


    dma_v_memcpy( ptr, ptr, len );

    return ( (uint16_t)CRC.CHECKSUM1 << 8 ) | CRC.CHECKSUM0;
}


#include "timers.h"
#include "logging.h"
#include "crc.h"

void dma_test( void ){

    uint64_t start;
    uint64_t elapsed;
    uint16_t crc0 = 0;
    uint16_t crc1 = 0;

    uint8_t buf[127];
    uint8_t buf2[127];
    int8_t compare = -100;

    memset( buf2, 0, sizeof(buf2) );
    for( uint8_t i = 0; i < sizeof(buf); i++ ){

        buf[i] = i;
    }

    start = tmr_u64_get_system_time_us();

    for( uint8_t i = 0; i < 64; i++ ){

        memcpy( buf2, buf, sizeof(buf2) );
    }

    elapsed = tmr_u64_get_system_time_us() - start;

    compare = memcmp( buf, buf2, sizeof(buf) );
    log_v_debug_P( PSTR("memcpy: %u  %d"), (uint16_t)elapsed, compare );


    memset( buf2, 0, sizeof(buf2) );
    for( uint8_t i = 0; i < sizeof(buf); i++ ){

        buf[i] = i;
    }


    start = tmr_u64_get_system_time_us();

    for( uint8_t i = 0; i < 64; i++ ){

        dma_v_memcpy( buf2, buf, sizeof(buf2) );
    }

    elapsed = tmr_u64_get_system_time_us() - start;

    compare = memcmp( buf, buf2, sizeof(buf) );
    log_v_debug_P( PSTR("dma_v_memcpy: %u  %d"), (uint16_t)elapsed, compare );




    memset( buf, 0, sizeof(buf) );

    start = tmr_u64_get_system_time_us();

    for( uint8_t i = 0; i < 64; i++ ){

        crc0 = crc_u16_block( buf, sizeof(buf) );
    }

    elapsed = tmr_u64_get_system_time_us() - start;

    log_v_debug_P( PSTR("crc_u16_block: %u"), elapsed );


    memset( buf, 0, sizeof(buf) );

    start = tmr_u64_get_system_time_us();

    for( uint8_t i = 0; i < 64; i++ ){

        crc1 = dma_u16_crc( buf, sizeof(buf) );
    }

    elapsed = tmr_u64_get_system_time_us() - start;

    log_v_debug_P( PSTR("dma_u16_crc: %u"), elapsed );


    log_v_debug_P( PSTR("crc0: 0x%04x crc1: 0x%04x"), crc0, crc1 );
}
