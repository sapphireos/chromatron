// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

#include "sapphire.h"

#include "config.h"
#include "coprocessor.h"

#include "esp8266_loader.h"
#include "hal_esp8266.h"

#include "ffs_fw.h"
#include "pixel.h"

#include "app.h"

#ifdef ENABLE_ESP_UPGRADE_LOADER

#define CFG_PARAM_COPROC_LOAD_DISABLE          __KV__coproc_load_disable

KV_SECTION_META kv_meta_t coproc_cfg_kv[] = {
    { SAPPHIRE_TYPE_BOOL,      0, 0,  0, cfg_i8_kv_handler,  "coproc_load_disable" },
};
#endif



static uint32_t fw_addr;

// process a message
// assumes CRC is valid
void coproc_v_dispatch( 
    coproc_hdr_t *hdr, 
    const uint8_t *data,
    uint8_t *response_len,
    uint8_t response[COPROC_BUF_SIZE] ){

    uint8_t len = hdr->length;
    int32_t *params = (int32_t *)data;
    int32_t retval = 0;

    *response_len = sizeof(retval);

    if( hdr->opcode == OPCODE_TEST ){

        memcpy( response, data, len );
        *response_len = len;
    }
    else if( hdr->opcode == OPCODE_REBOOT ){

        sys_reboot();

        while(1);
    }
    else if( hdr->opcode == OPCODE_LOAD_DISABLE ){

        bool value = TRUE;
        cfg_v_set( CFG_PARAM_COPROC_LOAD_DISABLE, &value );
    }
    else if( hdr->opcode == OPCODE_IO_SET_MODE ){

        io_v_set_mode( params[0], params[1] );
    }
    else if( hdr->opcode == OPCODE_IO_GET_MODE ){

        retval = io_u8_get_mode( params[0] );
        memcpy( response, &retval, sizeof(retval) );
    }
    else if( hdr->opcode == OPCODE_IO_DIGITAL_WRITE ){

        io_v_digital_write( params[0], params[1] );
    }
    else if( hdr->opcode == OPCODE_IO_DIGITAL_READ ){

        retval = io_b_digital_read( params[0] );
        memcpy( response, &retval, sizeof(retval) );
    }
    else if( hdr->opcode == OPCODE_IO_READ_ADC ){

        retval = adc_u16_read_mv( params[0] );
        memcpy( response, &retval, sizeof(retval) );
    }
    else if( hdr->opcode == OPCODE_FW_CRC ){

        retval = ffs_fw_u16_crc();
        memcpy( response, &retval, sizeof(retval) );
    }
    else if( hdr->opcode == OPCODE_FW_ERASE ){
            
        // immediate (non threaded) erase of main fw partition
        ffs_fw_v_erase( 0, TRUE );
        fw_addr = 0;
    }
    else if( hdr->opcode == OPCODE_FW_LOAD ){
        
        ffs_fw_i32_write( 0, fw_addr, data, len );
        fw_addr += len;
    }   
    else if( hdr->opcode == OPCODE_FW_BOOTLOAD ){
        
        sys_v_reboot_delay( SYS_REBOOT_LOADFW );
    }
    else if( hdr->opcode == OPCODE_PIX_SET_COUNT ){
        
        pixel_v_set_pix_count( params[0] );        
    }
    else if( hdr->opcode == OPCODE_PIX_SET_MODE ){
        
        pixel_v_set_pix_mode( params[0] );        
    }
    else if( hdr->opcode == OPCODE_PIX_SET_DITHER ){
        
        pixel_v_set_pix_dither( params[0] );        
    }
    else if( hdr->opcode == OPCODE_PIX_SET_CLOCK ){
        
        pixel_v_set_pix_clock( params[0] );        
    }
    else if( hdr->opcode == OPCODE_PIX_SET_RGB_ORDER ){
        
        pixel_v_set_rgb_order( params[0] );        
    }
    else if( hdr->opcode == OPCODE_PIX_SET_APA102_DIM ){
        
        pixel_v_set_apa102_dimmer( params[0] );        
    }   
    else{

        log_v_debug_P( PSTR("bad opcode: 0x%02x"), hdr->opcode );
    }
}



PT_THREAD( app_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    hal_wifi_v_init();
    
    #ifdef ENABLE_ESP_UPGRADE_LOADER

    if( !cfg_b_get_boolean( CFG_PARAM_COPROC_LOAD_DISABLE ) ){

        // run firmware loader    
        wifi_v_start_loader();

        THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

        if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

            log_v_debug_P( PSTR("ESP load failed") );

            THREAD_EXIT( pt );
        }
    }

    #endif
    
// THREAD_EXIT( pt );

    hal_wifi_v_enter_normal_mode();

    TMR_WAIT( pt, 300 );

    // note these pins get driven when the ESP8266 boots up.
    // which means if it resets, then we short out the coprocessor.
    // 1) We don't need CTS, so lets set it to input.
    // 2) We do need TXD, but only when transmitting.  So we'll leave it as input until we need it.
    // WIFI_USART_TXD_PORT.DIRCLR          = ( 1 << WIFI_USART_TXD_PIN );
    // WIFI_CTS_PORT.DIRCLR                = ( 1 << WIFI_CTS_PIN );

// THREAD_EXIT( pt );
    // reset baud rate
    // usart_v_set_double_speed( &WIFI_USART, FALSE );
    // usart_v_set_baud( &WIFI_USART, BAUD_115200 );

    TMR_WAIT( pt, 4000 );

    hal_wifi_v_usart_flush();


    // wait for sync
    while( hal_wifi_i16_usart_get_char() != COPROC_SYNC ){

        sys_v_wdt_reset();
    }
    // if we don't get a sync, the watchdog timer will restart the entire system.

    // send confirmation
    // Set TXD to output for transmission
    // WIFI_USART_TXD_PORT.DIRSET          = ( 1 << WIFI_USART_TXD_PIN );
    hal_wifi_v_usart_send_char( COPROC_SYNC );

    hal_wifi_v_usart_flush();

    // wait for transmit complete
    // _delay_us( 5 );

    // reset IO to input
    // WIFI_USART_TXD_PORT.DIRCLR          = ( 1 << WIFI_USART_TXD_PIN );

    log_v_debug_P( PSTR("sync") );

    // main message loop
    while(1){

        THREAD_YIELD( pt );

        THREAD_WAIT_WHILE( pt, !hal_wifi_b_usart_rx_available() );

        coproc_hdr_t hdr;
        coproc_v_receive_block( (uint8_t *)&hdr );

        uint8_t buf[COPROC_BUF_SIZE];

        uint8_t n_blocks = 0;
        if( hdr.length > 0 ){

            n_blocks = 1 + ( hdr.length - 1 ) / 4;
        }

        uint8_t i = 0;
        while( n_blocks > 0 ){
            
            coproc_v_receive_block( &buf[i] );
                
            n_blocks--;

            i += COPROC_BLOCK_LEN;
        }

        uint8_t response[COPROC_BUF_SIZE];
        uint8_t response_len = 0;

        // run command
        coproc_v_dispatch( &hdr, buf, &response_len, response );

        hdr.length = response_len;

        // Set TXD to output for transmission
        // WIFI_USART_TXD_PORT.DIRSET          = ( 1 << WIFI_USART_TXD_PIN );

        coproc_v_send_block( (uint8_t *)&hdr );

        int16_t data_len = response_len;
        i = 0;
        while( data_len > 0 ){

            coproc_v_send_block( &response[i] );    
            
            i += COPROC_BLOCK_LEN;
            data_len -= COPROC_BLOCK_LEN;
        }

        // wait for transmit complete
        // _delay_us( 5 );

        // Set TXD to input to protect against a short in case the ESP8266 resets
        // WIFI_USART_TXD_PORT.DIRCLR          = ( 1 << WIFI_USART_TXD_PIN );
    }

PT_END( pt );	
}



void app_v_init( void ){

    #ifndef ENABLE_WIFI
    thread_t_create( app_thread,
                     PSTR("coprocessor"),
                     0,
                     0 );
    #endif
}

