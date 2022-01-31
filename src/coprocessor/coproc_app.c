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

#include "sapphire.h"

#include "config.h"
#include "coprocessor.h"

#include "esp8266_loader.h"
#include "hal_esp8266.h"
#include "status_led.h"
#include "hal_cmd_usart.h"

#include "flash25.h"
#include "flash_fs.h"
#include "ffs_fw.h"
#include "ffs_block.h"
#include "pixel.h"

#include "coproc_app.h"

static bool boot_esp;

KV_SECTION_META kv_meta_t coproc_cfg_kv[] = {
    { CATBUS_TYPE_BOOL,      0, 0,  &boot_esp, 0,  "boot_esp" },

    // backup wifi keys.
    // if these aren't present in the KV index, the config module won't find them.

    
    { CATBUS_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_ssid" },
    { CATBUS_TYPE_STRING32,      0, 0,                          0,                  cfg_i8_kv_handler,   "wifi_password" },
};


static uint32_t fw_addr;
static uint32_t pix_transfer_count;

static uint32_t flash_start;
static uint32_t fw0_start;
static uint32_t fw0_end;
// static uint32_t ee_start;
// static uint32_t ee_end;
static uint32_t flash_size;
static uint32_t flash_addr;
static uint32_t flash_len;

static i2c_setup_t i2c_setup;

// process a message
// assumes CRC is valid
void coproc_v_dispatch( 
    coproc_hdr_t *hdr, 
    const uint8_t *data,
    uint8_t *response_len,
    uint8_t response[COPROC_BUF_SIZE] ){

    uint8_t len = hdr->length;
    int32_t *params = (int32_t *)data;

    // set up default return value
    int32_t *retval = (int32_t *)response;
    *retval = 0;

    *response_len = sizeof(int32_t);

    if( hdr->opcode == OPCODE_TEST ){

        memcpy( response, data, len );
        *response_len = len;
    }
    else if( hdr->opcode == OPCODE_REBOOT ){

        sys_reboot();

        while(1);
    }
    else if( hdr->opcode == OPCODE_GET_RESET_SOURCE ){

        *retval = sys_u8_get_reset_source();
    }
    else if( hdr->opcode == OPCODE_GET_BOOT_MODE ){

        *retval = sys_m_get_startup_boot_mode();
    }
    else if( hdr->opcode == OPCODE_GET_WIFI ){

        uint8_t buf[WIFI_SSID_LEN + WIFI_PASS_LEN];
        memset( buf, 0, sizeof(buf) );

        cfg_i8_get( CFG_PARAM_WIFI_SSID, buf );
        cfg_i8_get( CFG_PARAM_WIFI_PASSWORD, &buf[WIFI_SSID_LEN] );

        *response_len = sizeof(buf);
        memcpy( response, buf, sizeof(buf) );
    }
    else if( hdr->opcode == OPCODE_DEBUG_PRINT ){

        char *s = (char *)data;

        log_v_debug_P( PSTR("ESP8266: %s"), s );

        // log_v_debug_P( PSTR("ESP8266: %d %x %x %x %x"), len, data[0], data[1], data[2], data[3] );
    }
    else if( hdr->opcode == OPCODE_IO_SET_MODE ){

        io_v_set_mode( params[0], params[1] );
    }
    else if( hdr->opcode == OPCODE_IO_GET_MODE ){

        *retval = io_u8_get_mode( params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_DIGITAL_WRITE ){

        io_v_digital_write( params[0], params[1] );
    }
    else if( hdr->opcode == OPCODE_IO_DIGITAL_READ ){

        *retval = io_b_digital_read( params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_READ_ADC ){

        *retval = adc_u16_read_mv( params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_WRITE_PWM ){
        
        pwm_v_write( params[0], params[1] );        
    }
    else if( hdr->opcode == OPCODE_FW_CRC ){

        *retval = ffs_fw_u16_crc();
    }
    else if( hdr->opcode == OPCODE_FW_ERASE ){
        
        status_led_v_set( 1, STATUS_LED_TEAL );

        // immediate (non threaded) erase of main fw partition
        ffs_fw_v_erase( 0, TRUE );
        fw_addr = 0;
    }
    else if( hdr->opcode == OPCODE_FW_LOAD ){
        
        ffs_fw_i32_write( 0, fw_addr, data, len );
        fw_addr += len;
    }   
    else if( hdr->opcode == OPCODE_FW_BOOTLOAD ){
        
        sys_v_load_fw();
        cpu_reboot();

        while(1);
    }
    else if( hdr->opcode == OPCODE_FW_VERSION ){

        sys_v_get_fw_version( (char *)response );
        *response_len = FW_VER_LEN;
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
    else if( hdr->opcode == OPCODE_PIX_LOAD ){

        pix_transfer_count = *params;

        pixel_v_set_pix_count( pix_transfer_count );
    }   
    // #ifndef ENABLE_USB_UDP_TRANSPORT
    else if( hdr->opcode == OPCODE_IO_CMD_IS_RX_CHAR ){

        *retval = cmd_usart_b_received_char();
    }
    else if( hdr->opcode == OPCODE_IO_CMD_SEND_CHAR ){

        cmd_usart_v_send_char( params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_CMD_SEND_DATA ){

        uint32_t tx_len = params[0];
        uint8_t *tx_data = (uint8_t *)&params[1];

        cmd_usart_v_send_data( tx_data, tx_len );
    }
    else if( hdr->opcode == OPCODE_IO_CMD_GET_CHAR ){

        *retval = cmd_usart_i16_get_char();
    }
    else if( hdr->opcode == OPCODE_IO_CMD_GET_DATA ){

        uint32_t rx_len = params[0];
        uint8_t *rx_data = (uint8_t *)&retval[1];

        *retval = cmd_usart_u8_get_data( rx_data, rx_len );
    }
    else if( hdr->opcode == OPCODE_IO_CMD_RX_SIZE ){

        *retval = cmd_usart_u16_rx_size();
    }
    else if( hdr->opcode == OPCODE_IO_CMD_FLUSH ){

        cmd_usart_v_flush();        
    }
    else if( hdr->opcode == OPCODE_IO_USART_INIT ){

        usart_v_init( USER_USART );
    }
    else if( hdr->opcode == OPCODE_IO_USART_SEND_CHAR ){

        usart_v_send_byte( USER_USART, params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_USART_GET_CHAR ){

        *retval = usart_i16_get_byte( USER_USART );
    }
    else if( hdr->opcode == OPCODE_IO_USART_RX_SIZE ){

        *retval = usart_u8_bytes_available( USER_USART );
    }
    else if( hdr->opcode == OPCODE_IO_USART_SET_BAUD ){

        usart_v_set_baud( USER_USART, params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_I2C_INIT ){

        i2c_v_init( params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_I2C_SET_PINS ){

        i2c_v_set_pins( params[0], params[1] );
    }
    else if( hdr->opcode == OPCODE_IO_I2C_SETUP ){

        i2c_setup = *(i2c_setup_t *)params;
    }
    else if( hdr->opcode == OPCODE_IO_I2C_WRITE ){

        i2c_v_write( i2c_setup.dev_addr, data, i2c_setup.len );
    }
    else if( hdr->opcode == OPCODE_IO_I2C_READ ){

        i2c_v_read( i2c_setup.dev_addr, response, i2c_setup.len );   
        *response_len = i2c_setup.len;
    }
    else if( hdr->opcode == OPCODE_IO_I2C_MEM_WRITE ){

        i2c_v_mem_write( i2c_setup.dev_addr, i2c_setup.mem_addr, i2c_setup.addr_size, data, i2c_setup.len, i2c_setup.delay_ms );        
    }
    else if( hdr->opcode == OPCODE_IO_I2C_MEM_READ ){

        i2c_v_mem_read( i2c_setup.dev_addr, i2c_setup.mem_addr, i2c_setup.addr_size, response, i2c_setup.len, i2c_setup.delay_ms );           
        *response_len = i2c_setup.len;
    }
    else if( hdr->opcode == OPCODE_IO_I2C_WRITE_REG8 ){

        i2c_v_write_reg8( params[0], params[1], params[2] );
    }
    else if( hdr->opcode == OPCODE_IO_I2C_READ_REG8 ){

        *retval = i2c_u8_read_reg8( params[0], params[1] );   
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_SIZE ){

        *retval = flash_size;
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_ADDR ){
        
        flash_addr = params[0];

        // log_v_debug_P( PSTR("addr: %lu"), flash_addr );
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_LEN ){
        
        flash_len = params[0];

        // log_v_debug_P( PSTR("len: %lu"), flash_len );

        if( flash_len > COPROC_BUF_SIZE ){

            flash_len = COPROC_BUF_SIZE;
        }
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_ERASE ){

        uint32_t addr;

        // check if address is block 0
        if( flash_addr < FLASH_FS_ERASE_BLOCK_SIZE ){

            addr = flash_start + flash_addr;
        }
        // check if address is within firmware partition 0
        else if( ( flash_addr >= fw0_start ) && ( flash_addr < fw0_end ) ){

            uint32_t pos = flash_addr - fw0_start;

            addr = FLASH_FS_FIRMWARE_2_PARTITION_START + pos;
        }
        // main FS partition
        else{

            uint32_t pos = flash_addr - fw0_end;

            addr = flash_start + pos + FLASH_FS_ERASE_BLOCK_SIZE;
        }

        flash25_v_write_enable();
        flash25_v_erase_4k( addr );

        // if( ( flash_addr >= ee_start ) && ( flash_addr < ee_end ) ){

        //     uint8_t temp = 0;
        //     flash25_v_read( addr, &temp, 1 );

        //     log_v_debug_P( PSTR("erase: 0x%0lx/0x%0lx -> 0x%02lx"), (uint32_t)addr, (uint32_t)flash_addr, temp );
        // }


        // // log_v_debug_P( PSTR("erase") );

        // // check if address is within firmware partition 0
        // if( ( flash_addr >= fw0_start ) && ( flash_addr <= fw0_end ) ){

        //     // redirect to coprocessor's firmware 2 partition
        //     uint32_t addr = ( flash_addr - fw0_start ) + FLASH_FS_FIRMWARE_2_PARTITION_START;

        //     flash25_v_erase_4k( addr );
        // }
        // else{

        //     flash25_v_erase_4k( flash_start + flash_addr - fw0_end );    
        // }
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_READ ){

        uint8_t buf[COPROC_BUF_SIZE];
        memset( buf, 0, sizeof(buf) );
    
        uint32_t addr;

        // check if address is block 0
        if( flash_addr < FLASH_FS_ERASE_BLOCK_SIZE ){

            addr = flash_start + flash_addr;
        }
        // check if address is within firmware partition 0
        else if( ( flash_addr >= fw0_start ) && ( flash_addr < fw0_end ) ){

            uint32_t pos = flash_addr - fw0_start;

            addr = FLASH_FS_FIRMWARE_2_PARTITION_START + pos;
        }
        // main FS partition
        else{

            uint32_t pos = flash_addr - fw0_end;

            addr = flash_start + pos + FLASH_FS_ERASE_BLOCK_SIZE;
        }    


        flash25_v_read( addr, buf, flash_len );

        memcpy( response, buf, flash_len );

        *response_len = flash_len;

        // if( ( flash_addr >= ee_start ) && ( flash_addr < ee_end ) ){

        //     log_v_debug_P( PSTR("read: 0x%0lx/0x%0lx -> 0x%02lx"), (uint32_t)addr, (uint32_t)flash_addr, (uint32_t)response[0] );
        // }

        // log_v_debug_P( PSTR("read: 0x%02x"), response[0] );
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_WRITE ){

        uint32_t addr;

        // check if address is block 0
        if( flash_addr < FLASH_FS_ERASE_BLOCK_SIZE ){

            addr = flash_start + flash_addr;
        }
        // check if address is within firmware partition 0
        else if( ( flash_addr >= fw0_start ) && ( flash_addr < fw0_end ) ){

            uint32_t pos = flash_addr - fw0_start;

            addr = FLASH_FS_FIRMWARE_2_PARTITION_START + pos;
        }
        // // check if address is within EEPROM partition
        // else if( ( flash_addr >= ee_start ) && ( flash_addr <= ee_end ) ){

        //     uint32_t pos = flash_addr - ee_start;

        //     addr = FLASH_FS_FIRMWARE_2_PARTITION_START + pos;
        // }
        // main FS partition
        else{

            uint32_t pos = flash_addr - fw0_end;

            addr = flash_start + pos + FLASH_FS_ERASE_BLOCK_SIZE;
        }

        flash25_v_write( addr, data, flash_len );
        // flash25_v_write( addr, data, flash_len ); // why does this work?

        // log_v_debug_P( PSTR("write: 0x%0lx/0x%0lx = 0x%02lx"), 
        //     (uint32_t)addr, (uint32_t)flash_addr, (uint32_t)data[0] );


        // if( ( flash_addr >= ee_start ) && ( flash_addr < ee_end ) ){
            // uint8_t temp = 0;
            // flash25_v_read( addr, &temp, 1 );

            // log_v_debug_P( PSTR("write: 0x%0lx/0x%0lx = 0x%02lx -> 0x%02lx"), 
            //     (uint32_t)addr, (uint32_t)flash_addr, (uint32_t)data[0], (uint32_t)temp );

        // }
    }
    // #endif
    // else{

    //     log_v_debug_P( PSTR("bad opcode: 0x%02x"), hdr->opcode );
    // }
}

PT_THREAD( app_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
    
    flash_start     = FLASH_FS_FILE_SYSTEM_START + ( (uint32_t)ffs_block_u16_total_blocks() * FLASH_FS_ERASE_BLOCK_SIZE );
    flash_size      = ( flash25_u32_capacity() - flash_start ) + ( (uint32_t)FLASH_FS_FIRMWARE_2_SIZE_KB * 1024 );
    fw0_start       = FLASH_FS_FIRMWARE_0_PARTITION_START;
    fw0_end         = fw0_start + ( (uint32_t)FLASH_FS_FIRMWARE_2_SIZE_KB * 1024 );
    // ee_start        = fw0_end;
    // ee_end          = ee_start + ( (uint32_t)128 * 1024 ); // this must match the esp8266 target EE config

    // log_v_debug_P( PSTR("start: %lu size: %lu fw0: %lu -> %lu"), flash_start, flash_size, fw0_start, fw0_end );


    // flash25_v_write_enable();
    // flash25_v_erase_4k( flash_start );
    // uint8_t byte = 2;
    // #define ADDR (flash_start + 16)

    // // uint8_t read1 = flash25_u8_read_byte( ADDR );


    // flash25_v_write( ADDR, &byte, 1 );
    
    // uint8_t read2 = flash25_u8_read_byte( ADDR );

    // flash25_v_write( ADDR, &byte, 1 );
    
    // uint8_t read3 = flash25_u8_read_byte( ADDR );

    // log_v_debug_P( PSTR("%d %d"), 
    //     (int16_t)read2,
    //     (int16_t)read3 );

    // // log_v_debug_P( PSTR("%d %d %d"), 
    // //     (int16_t)read1,
    // //     (int16_t)read2,
    // //     (int16_t)read3 );


    hal_wifi_v_init();
    
    // run firmware loader    
    wifi_v_start_loader();

    THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

    if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

        log_v_debug_P( PSTR("ESP load failed") );

        cmd_usart_v_init();

        while(1){

            status_led_v_set( 0, STATUS_LED_RED );
            status_led_v_set( 1, STATUS_LED_BLUE );

            TMR_WAIT( pt, 250 );

            status_led_v_set( 0, STATUS_LED_BLUE );
            status_led_v_set( 1, STATUS_LED_RED );

            TMR_WAIT( pt, 250 );
        }

        THREAD_EXIT( pt );
    }

    status_led_v_set( 1, STATUS_LED_WHITE );

    log_v_debug_P( PSTR("fs ver: %d"), flash25_u8_read_byte( flash_start + 16 ) );

cmd_usart_v_init();
// THREAD_EXIT( pt );
// static bool temp;
// temp = FALSE;
// while( !boot_esp ){

//     if( flash25_u8_read_byte( flash_start + 16 ) != 2 && !temp ){

//         temp = TRUE;
//         log_v_debug_P( PSTR("WTF fs ver: %d"), flash25_u8_read_byte( flash_start + 16 ) );
//     }

//     THREAD_YIELD( pt );
// }
THREAD_WAIT_WHILE( pt, !boot_esp );
// TMR_WAIT( pt, 2000 );

    log_v_debug_P( PSTR("fs ver: %d"), flash25_u8_read_byte( flash_start + 16 ) );

    status_led_v_set( 1, STATUS_LED_YELLOW );

    hal_wifi_v_enter_normal_mode();

    TMR_WAIT( pt, 300 );

    hal_wifi_v_usart_flush();

    // wait for sync
    while( hal_wifi_i16_usart_get_char() != COPROC_SYNC ){

        sys_v_wdt_reset();
    }

    // send confirmation
    hal_wifi_v_usart_send_char( COPROC_SYNC );

    // await protocol version
    while( !hal_wifi_b_usart_rx_available() );
    
    uint8_t version = hal_wifi_i16_usart_get_char();

    if( version != 0 ){

        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 1000 );

        // watchdog timeout here
        while(1);
    }

    // echo version back
    hal_wifi_v_usart_send_char( version );

    hal_wifi_v_usart_flush();

    log_v_debug_P( PSTR("sync") );


    // main message loop
    while(1){

        THREAD_YIELD( pt );

        THREAD_WAIT_WHILE( pt, !hal_wifi_b_usart_rx_available() );

        coproc_hdr_t hdr;
        coproc_v_receive_block( (uint8_t *)&hdr );

        ASSERT( hdr.sof == COPROC_SOF );

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

        coproc_v_send_block( (uint8_t *)&hdr );

        int16_t data_len = response_len;
        i = 0;
        while( data_len > 0 ){

            coproc_v_send_block( &response[i] );    
            
            i += COPROC_BLOCK_LEN;
            data_len -= COPROC_BLOCK_LEN;
        }

        if( pix_transfer_count > 0 ){

            uint8_t *r = pixel_u8p_get_red();
            uint8_t *g = pixel_u8p_get_green();
            uint8_t *b = pixel_u8p_get_blue();
            uint8_t *d = pixel_u8p_get_dither();

            while( pix_transfer_count > 0 ){

                uint8_t temp_r, temp_g, temp_b, temp_d;

                while( !hal_wifi_b_usart_rx_available() );
                temp_r = hal_wifi_i16_usart_get_char();
                while( !hal_wifi_b_usart_rx_available() );
                temp_g = hal_wifi_i16_usart_get_char();
                while( !hal_wifi_b_usart_rx_available() );
                temp_b = hal_wifi_i16_usart_get_char();
                while( !hal_wifi_b_usart_rx_available() );
                temp_d = hal_wifi_i16_usart_get_char();                

                pix_transfer_count--;

                if( ( pix_transfer_count % COPROC_PIX_WAIT_COUNT ) == 0 ){

                    hal_wifi_v_usart_send_char( COPROC_SYNC );
                }

                ATOMIC;
                *r++ = temp_r;
                *g++ = temp_g;
                *b++ = temp_b;
                *d++ = temp_d;
                END_ATOMIC;
            }
        }
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

