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

// static bool boot_esp;

static bool loadfw_enable_1;
static bool loadfw_enable_2;
static bool loadfw_request;


KV_SECTION_META kv_meta_t coproc_cfg_kv[] = {
    // { CATBUS_TYPE_BOOL,      0, 0,  &boot_esp, 0,  "boot_esp" },
    { CATBUS_TYPE_BOOL,      0, 0,  &loadfw_request, 0,  "request_load_wifi_firmware" },

    { CATBUS_TYPE_UINT32,    0, 0,   0,                  cfg_i8_kv_handler,   "coproc_error_flags" },

    // backup wifi keys.
    // if these aren't present in the KV index, the config module won't find them.
    { CATBUS_TYPE_STRING32,  0, 0,   0,                  cfg_i8_kv_handler,   "wifi_ssid" },
    { CATBUS_TYPE_STRING32,  0, 0,   0,                  cfg_i8_kv_handler,   "wifi_password" },
};

static uint32_t fw_addr;
static uint32_t pix_transfer_count;

static uint32_t flash_start;
static uint32_t fw0_start;
static uint32_t fw0_end;
static uint32_t flash_size;
static uint32_t flash_addr;
static uint32_t flash_len;

static i2c_setup_t i2c_setup;
static uint8_t response[COPROC_BUF_SIZE];

void coproc_set_error_flags( uint32_t flags ){

    cfg_v_set( __KV__coproc_error_flags, &flags );
}

static uint32_t map_flash_addr( uint32_t flash_addr ){

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

    return addr;
}

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

    if( hdr->opcode == OPCODE_LOADFW_1 ){

        // check for valid sequence
        if( !loadfw_enable_1 && !loadfw_enable_2 ){

            log_v_debug_P( PSTR("load 1"));

            loadfw_enable_1 = TRUE;
        }
        else{
        
            // reset load fw sequence
            loadfw_enable_1 = FALSE;
            loadfw_enable_2 = FALSE;
            loadfw_request = FALSE;
        }
    }
    else if( hdr->opcode == OPCODE_LOADFW_2 ){

        // check for valid sequence
        if( loadfw_enable_1 && !loadfw_enable_2 ){

            log_v_debug_P( PSTR("load 2"));

            loadfw_enable_2 = TRUE;    
        }
        else{
        
            // reset load fw sequence
            loadfw_enable_1 = FALSE;
            loadfw_enable_2 = FALSE;
            loadfw_request = FALSE;
        }
    }
    else if( hdr->opcode == OPCODE_SAFE_MODE ){

        // let watchdog reset
        // this should report safe mode.
        while(1);
    }
    else if( hdr->opcode == OPCODE_REBOOT ){

        // check for valid sequence
        if( loadfw_enable_1 && loadfw_enable_2 ){

            log_v_debug_P( PSTR("load 3"));

            // load fw
            loadfw_request = TRUE;

            return;
        }   
        else{

            log_v_debug_P( PSTR("reboot"));

            sys_reboot();

            while(1);
        }
    }    
    else{

        // reset load fw sequence
        loadfw_enable_1 = FALSE;
        loadfw_enable_2 = FALSE;
        loadfw_request = FALSE;
    }

    if( hdr->opcode == OPCODE_TEST ){

        memcpy( response, data, len );
        *response_len = len;
    }
    else if( hdr->opcode == OPCODE_GET_RESET_SOURCE ){

        *retval = sys_u8_get_reset_source();
    }
    else if( hdr->opcode == OPCODE_GET_ERROR_FLAGS ){

        cfg_i8_get( __KV__coproc_error_flags, retval );
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

        #ifdef ENABLE_FFS
        char *s = (char *)data;

        log_v_debug_P( PSTR("ESP8266: %s"), s );
        #endif
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
    else if( hdr->opcode == OPCODE_IO_USART_GET_CHUNK ){

        *response_len = usart_u8_get_bytes( USER_USART, response, params[0] );
    }
    else if( hdr->opcode == OPCODE_IO_USART_RX_SIZE ){

        *retval = usart_u8_bytes_available( USER_USART );
    }
    else if( hdr->opcode == OPCODE_IO_USART_SET_BAUD ){

        // translate baud rate setting:
        if( params[0] == 2400 ){

            params[0] = BAUD_2400;
        }
        else if( params[0] == 4800 ){

            params[0] = BAUD_4800;
        }
        else if( params[0] == 9600 ){

            params[0] = BAUD_9600;
        }
        else if( params[0] == 14400 ){

            params[0] = BAUD_14400;
        }
        else if( params[0] == 19200 ){

            params[0] = BAUD_19200;
        }
        else if( params[0] == 28800 ){

            params[0] = BAUD_28800;
        }
        else if( params[0] == 38400 ){

            params[0] = BAUD_38400;
        }
        else if( params[0] == 57600 ){

            params[0] = BAUD_57600;
        }
        else if( params[0] == 76800 ){

            params[0] = BAUD_76800;
        }
        else if( params[0] == 115200 ){

            params[0] = BAUD_115200;
        }
        else if( params[0] == 230400 ){

            params[0] = BAUD_230400;
        }
        else if( params[0] == 250000 ){

            params[0] = BAUD_250000;
        }
        else if( params[0] == 460800 ){

            params[0] = BAUD_460800;
        }
        else if( params[0] == 500000 ){

            params[0] = BAUD_500000;
        }
        else if( params[0] == 1000000 ){

            params[0] = BAUD_1000000;
        }
        else if( params[0] == 2000000 ){

            params[0] = BAUD_2000000;
        }
        else if( params[0] == 74880 ){

            params[0] = BAUD_74880;
        }
        else{

            // invalid setting, which will assert in the driver,
            // so just bail out
            return;
        }

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
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_LEN ){
        
        flash_len = params[0];

        if( flash_len > COPROC_FLASH_XFER_LEN ){

            flash_len = COPROC_FLASH_XFER_LEN;
        }
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_ERASE ){

        uint32_t addr = map_flash_addr( flash_addr );

        flash25_v_write_enable();
        flash25_v_erase_4k( addr );
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_READ ){

        uint8_t buf[COPROC_BUF_SIZE];
        memset( buf, 0, sizeof(buf) );
    
        uint32_t addr = map_flash_addr( flash_addr );

        flash25_v_read( addr, buf, flash_len );

        memcpy( response, buf, flash_len );

        *response_len = flash_len;
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_READ2 ){

        flash_addr = params[0];
        flash_len = params[1];

        if( flash_len > COPROC_FLASH_XFER_LEN ){

            flash_len = COPROC_FLASH_XFER_LEN;
        }

        uint8_t buf[COPROC_BUF_SIZE];
        memset( buf, 0, sizeof(buf) );
    
        uint32_t addr = map_flash_addr( flash_addr );

        flash25_v_read( addr, buf, flash_len );

        memcpy( response, buf, flash_len );

        *response_len = flash_len;
    }
    else if( hdr->opcode == OPCODE_IO_FLASH25_WRITE ){

        uint32_t addr = map_flash_addr( flash_addr );

        flash25_v_write( addr, data, flash_len );
    }
}


PT_THREAD( usart_recovery_thread( pt_t *pt, void *state ) )
{           
PT_BEGIN( pt );  

    cmd_usart_v_init();

    while(1){

        status_led_v_set( 0, STATUS_LED_RED );
        status_led_v_set( 1, STATUS_LED_BLUE );

        TMR_WAIT( pt, 250 );

        status_led_v_set( 0, STATUS_LED_BLUE );
        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 250 );

        if( loadfw_request ){

            wifi_v_start_loader( loadfw_request );

            THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

            if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

                while(1){

                    status_led_v_set( 0, STATUS_LED_RED );
                    status_led_v_set( 0, STATUS_LED_BLUE );

                    TMR_WAIT( pt, 250 );

                    status_led_v_set( 0, STATUS_LED_BLUE );
                    status_led_v_set( 1, STATUS_LED_RED );

                    TMR_WAIT( pt, 250 );
                }
            }

            sys_reboot();
        }
    }
 
PT_END( pt );   
}


uint8_t current_opcode;


PT_THREAD( app_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    status_led_v_set( 0, STATUS_LED_RED );
    status_led_v_set( 0, STATUS_LED_GREEN );
    status_led_v_set( 0, STATUS_LED_BLUE );

    uint8_t reset_source = sys_u8_get_reset_source();

    if( reset_source == RESET_SOURCE_WATCHDOG ){

        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 1000 );

        status_led_v_set( 0, STATUS_LED_RED );

        status_led_v_set( 1, STATUS_LED_GREEN );
        TMR_WAIT( pt, 250 );        
        status_led_v_set( 0, STATUS_LED_GREEN );
        TMR_WAIT( pt, 250 );        

        status_led_v_set( 1, STATUS_LED_GREEN );
        TMR_WAIT( pt, 250 );        
        status_led_v_set( 0, STATUS_LED_GREEN );
        TMR_WAIT( pt, 250 );        

        status_led_v_set( 1, STATUS_LED_GREEN );
        TMR_WAIT( pt, 250 );        
        status_led_v_set( 0, STATUS_LED_GREEN );
        TMR_WAIT( pt, 250 );        
    }

    
    flash_start     = FLASH_FS_FILE_SYSTEM_START + ( (uint32_t)ffs_block_u16_total_blocks() * FLASH_FS_ERASE_BLOCK_SIZE );
    flash_size      = ( flash25_u32_capacity() - flash_start ) + ( (uint32_t)FLASH_FS_FIRMWARE_2_SIZE_KB * 1024 );
    fw0_start       = FLASH_FS_FIRMWARE_0_PARTITION_START;
    fw0_end         = fw0_start + ( (uint32_t)FLASH_FS_FIRMWARE_2_SIZE_KB * 1024 );
    
    hal_wifi_v_init();

    // loadfw_request = TRUE;
     
    // run firmware loader    
    wifi_v_start_loader( loadfw_request );

    THREAD_WAIT_WHILE( pt, wifi_i8_loader_status() == ESP_LOADER_STATUS_BUSY );

    if( wifi_i8_loader_status() == ESP_LOADER_STATUS_FAIL ){

        log_v_debug_P( PSTR("ESP load failed") );

        // flash LEDs, but then try to start ESP chip anyway.
        // if the coprocessor sync fails, we'll go into recovery mode
        status_led_v_set( 0, STATUS_LED_RED );
        status_led_v_set( 1, STATUS_LED_BLUE );

        TMR_WAIT( pt, 250 );

        status_led_v_set( 0, STATUS_LED_BLUE );
        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 250 );
        
        status_led_v_set( 0, STATUS_LED_RED );
        status_led_v_set( 1, STATUS_LED_BLUE );

        TMR_WAIT( pt, 250 );

        status_led_v_set( 0, STATUS_LED_BLUE );
        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 250 );
        
        status_led_v_set( 0, STATUS_LED_RED );
        status_led_v_set( 1, STATUS_LED_BLUE );

        TMR_WAIT( pt, 250 );

        status_led_v_set( 0, STATUS_LED_BLUE );
        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 250 );
    }

    if( loadfw_request ){

        sys_reboot();
    }


// // DEBUG:
// status_led_v_set( 1, STATUS_LED_WHITE );
// cmd_usart_v_init();
// THREAD_WAIT_WHILE( pt, !boot_esp );


    status_led_v_set( 1, STATUS_LED_YELLOW );

    hal_wifi_v_enter_normal_mode();

    TMR_WAIT( pt, 300 );

    hal_wifi_v_usart_flush();

    uint32_t start_time = tmr_u32_get_system_time_ms();

    // wait for sync
    while( hal_wifi_i16_usart_get_char() != COPROC_SYNC ){

        sys_v_wdt_reset();

        if( tmr_u32_elapsed_time_ms( start_time ) > 4000 ){

            log_v_debug_P( PSTR("sync timeout") );

            // start USB recovery mode
            thread_t_create( usart_recovery_thread,
                     PSTR("usart_recovery"),
                     0,
                     0 );

            THREAD_EXIT( pt );
        }
    }

    // send confirmation
    hal_wifi_v_usart_send_char( COPROC_SYNC );

    // await protocol version
    while( !hal_wifi_b_usart_rx_available() );
    
    uint8_t version = hal_wifi_i16_usart_get_char();

    if( version != 0 ){

        status_led_v_set( 1, STATUS_LED_RED );

        TMR_WAIT( pt, 1000 );

        coproc_set_error_flags( COPROC_ERROR_SYNC_FAIL );

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

        current_opcode = 0;

        coproc_hdr_t hdr;
        coproc_v_receive_block( (uint8_t *)&hdr, TRUE );

        ASSERT( hdr.sof == COPROC_SOF );

        current_opcode = hdr.opcode;

        uint8_t buf[COPROC_BUF_SIZE];

        uint8_t n_blocks = 0;
        if( hdr.length > 0 ){

            n_blocks = 1 + ( hdr.length - 1 ) / 4;
        }

        uint8_t i = 0;
        while( n_blocks > 0 ){
            
            coproc_v_receive_block( &buf[i], FALSE );
                
            n_blocks--;

            i += COPROC_BLOCK_LEN;
        }

        uint8_t response_len = 0;

        // run command
        coproc_v_dispatch( &hdr, buf, &response_len, response );

        if( loadfw_request && loadfw_enable_1 && loadfw_enable_2 ){

            log_v_debug_P( PSTR("valid load sequence") );

            THREAD_RESTART( pt );
        }

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
                uint8_t pix_buf[4];
                memset( pix_buf, 0, sizeof(pix_buf) );

                // for( uint8_t i = 0; i < cnt_of_array(buf); i++ ){
                    
                //     uint32_t start = tmr_u32_get_system_time_ms();
                //     while( !hal_wifi_b_usart_rx_available() && ( tmr_u32_elapsed_time_ms( start ) < 500 ) );

                //     if( !hal_wifi_b_usart_rx_available() ){

                //         coproc_set_error_flags( COPROC_ERROR_PIX_STALL );
                //         while(1);
                //     }

                //     buf[i] = hal_wifi_i16_usart_get_char();
                // }

                // temp_r = hal_wifi_i16_usart_get_char();

                // while( !hal_wifi_b_usart_rx_available() );
                // temp_g = hal_wifi_i16_usart_get_char();
                
                // while( !hal_wifi_b_usart_rx_available() );
                // temp_b = hal_wifi_i16_usart_get_char();
                
                // while( !hal_wifi_b_usart_rx_available() );
                // temp_d = hal_wifi_i16_usart_get_char();                

                pix_transfer_count--;

                if( ( pix_transfer_count % COPROC_PIX_WAIT_COUNT ) == 0 ){

                    hal_wifi_v_usart_send_char( COPROC_SYNC );
                }

                temp_r = buf[0];
                temp_g = buf[1];
                temp_b = buf[2];
                temp_d = buf[3];

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

    // reset load fw sequence
    loadfw_enable_1 = FALSE;
    loadfw_enable_2 = FALSE;
    loadfw_request = FALSE;

    #ifndef ENABLE_WIFI
    thread_t_create( app_thread,
                     PSTR("coprocessor"),
                     0,
                     0 );
    #endif
}

