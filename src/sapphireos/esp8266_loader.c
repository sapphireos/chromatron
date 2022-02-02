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

#include "system.h"

#if !defined(ESP8266) && !defined(ESP32)

#include "fs.h"
#include "timers.h"
#include "config.h"
#include "watchdog.h"

#include "hal_esp8266.h"
#include "esp8266_loader.h"
#include "hal_status_led.h"

// #define NO_LOGGING
#include "logging.h"

#include "wifi_cmd.h"

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

#include "esp_stub.txt"


// Maximum block sized for RAM and Flash writes, respectively.
#define ESP_RAM_BLOCK   0x1800
#define ESP_FLASH_BLOCK 0x400

// Default baudrate. The ROM auto-bauds, so we can use more or less whatever we want.
#define ESP_ROM_BAUD    115200

// First byte of the application image
#define ESP_IMAGE_MAGIC 0xe9

// Initial state for the checksum routine
#define ESP_CHECKSUM_MAGIC 0xef

// OTP ROM addresses
#define ESP_OTP_MAC0    0x3ff00050
#define ESP_OTP_MAC1    0x3ff00054
#define ESP_OTP_MAC3    0x3ff0005c

// Flash sector size, minimum unit of erase.
#define ESP_FLASH_SECTOR 0x1000

#define ESP_CESANTA_FLASH_SECTOR_SIZE   4096

#define ESP_CESANTA_CMD_FLASH_ERASE     0
#define ESP_CESANTA_CMD_FLASH_WRITE     1
#define ESP_CESANTA_CMD_FLASH_READ      2
#define ESP_CESANTA_CMD_FLASH_DIGEST    3


typedef struct{
    uint8_t timeout;
    file_t fw_file;
    uint8_t tries;
} loader_thread_state_t;

PT_THREAD( wifi_loader_thread( pt_t *pt, loader_thread_state_t *state ) );



void slip_v_send_byte( uint8_t b ){

    if( b == SLIP_END ){

        hal_wifi_v_usart_send_char( SLIP_ESC );
        hal_wifi_v_usart_send_char( SLIP_ESC_END );
    }
    else if( b == SLIP_ESC ){

        hal_wifi_v_usart_send_char( SLIP_ESC );
        hal_wifi_v_usart_send_char( SLIP_ESC_ESC );
    }
    else{

        hal_wifi_v_usart_send_char( b );
    }
}

void slip_v_send_data( uint8_t *data, uint16_t len ){

    while( len > 0 ){

        slip_v_send_byte( *data );

        data++;
        len--;
    }
}

void esp_v_send_header( uint8_t op, uint16_t len, uint32_t checksum ){

    hal_wifi_v_usart_flush();

    hal_wifi_v_usart_send_char( SLIP_END );

    slip_v_send_byte( 0x00 );
    slip_v_send_byte( op );
    slip_v_send_byte( len & 0xff );
    slip_v_send_byte( len >> 8 );
    slip_v_send_byte( checksum & 0xff );
    slip_v_send_byte( checksum >> 8 );
    slip_v_send_byte( checksum >> 16 );
    slip_v_send_byte( checksum >> 24 );
}


void esp_v_command( uint8_t op, uint8_t *data, uint16_t len, uint32_t checksum ){

    hal_wifi_v_usart_flush();

    esp_v_send_header( op, len, checksum );

    slip_v_send_data( data, len );

    hal_wifi_v_usart_send_char( SLIP_END );
}

static const PROGMEM uint8_t sync_data[] = {
    0x07,
    0x07,
    0x12,
    0x20,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
    0x55,
};


void esp_v_send_sync( void ){

    uint8_t buf[sizeof(sync_data)];
    memcpy_P( buf, sync_data, sizeof(buf) );

    esp_v_command( ESP_SYNC, buf, sizeof(buf), 0 );
}

// int8_t esp_i8_sync( void ){

//     uint8_t buf[32];
//     memset( buf, 0xff, sizeof(buf) );

//     hal_wifi_v_usart_flush();

//     // ESP seems to miss the first sync for some reason,
//     // so we'll just send twice.
//     // it's not really a big deal from a timing standpoint since
//     // we'd try again in a few milliseconds, but if the wait response
//     // function is doing error logging, it saves us a pointless error
//     // message on every start up.
//     esp_v_send_sync();
//     esp_v_send_sync();

//     // blocking wait!
//     int8_t status = esp_i8_wait_response( buf, sizeof(buf), ESP_SYNC_TIMEOUT );

//     if( status < 0 ){

//         return status;
//     }

//     esp_response_t *resp = (esp_response_t *)buf;

//     if( resp->opcode != ESP_SYNC ){

//         return -10;
//     }

//     return 0;
// }


int8_t esp_i8_wait_response( uint8_t *buf, uint8_t len, uint32_t timeout ){

    int8_t status = -100;
    uint8_t next_byte = 0;
    
    // waiting for frame start
    int16_t byte = hal_wifi_i16_usart_get_char_timeout( timeout );

    if( byte != SLIP_END ){
        
        status = -1;
        goto end;        
    }

    next_byte++;

    while(1){

        // wait for byte
        int16_t byte = hal_wifi_i16_usart_get_char_timeout( timeout );
        while( byte < 0 ){

            status = -4;
            goto end;
        }

        uint8_t b = byte;
        next_byte++;

        if( b == SLIP_END ){

            status = 0;
            goto end;
        }
        else if( b == SLIP_ESC ){

            // wait for byte
            byte = hal_wifi_i16_usart_get_char_timeout( timeout );
            if( byte < 0 ){

                status = -5;
                goto end;
            }

            b = byte;
            next_byte++;

            if( b == SLIP_ESC_END ){

                *buf = SLIP_END;
                buf++;
                len--;
            }
            else if( b == SLIP_ESC_ESC ){

                *buf = SLIP_ESC;
                buf++;
                len--;
            }
            else{

                status = -6;
                goto end;
            }
        }
        else{

            if( len == 0 ){

                status = -3;
                goto end;
            }

            *buf = b;
            buf++;
            len--;
        }
    }

end:
    return status;
}

int8_t esp_i8_load_cesanta_stub( void ){

    uint8_t buf[32];
    memset( buf, 0xff, sizeof(buf) );
    esp_response_t *resp = (esp_response_t *)buf;
    uint8_t *resp_body = (uint8_t *)( resp + 1 );

    uint32_t param = ESP_CESANTA_BAUD;

    #define STUB_CODE_SIZE ( sizeof(esp_stub_code) + sizeof(param) )

    esp_mem_begin_t mem_begin;
    mem_begin.size = STUB_CODE_SIZE;
    mem_begin.num_blocks = 1;
    mem_begin.block_size = STUB_CODE_SIZE;
    mem_begin.offset = ESP_STUB_PARAMS_START;

    esp_v_command( ESP_MEM_BEGIN, (uint8_t *)&mem_begin, sizeof(mem_begin), 0 );

    if( esp_i8_wait_response( buf, sizeof(buf), ESP_CMD_TIMEOUT ) < 0 ){

        return -1;
    }

    if( ( resp_body[0] != 0 ) || ( resp_body[1] != 0 ) ){

        log_v_debug_P( PSTR("resp: %x %x %x %lx %x %x"),
                resp->resp,
                resp->opcode,
                resp->len,
                resp->val,
                resp_body[0],
                resp_body[1]
                );

        return -2;
    }

    uint8_t checksum = ESP_CHECKSUM_MAGIC;

    // check sum over param
    checksum ^= ( param >> 24 ) & 0xff;
    checksum ^= ( param >> 16 ) & 0xff;
    checksum ^= ( param >> 8 )  & 0xff;
    checksum ^= ( param >> 0 )  & 0xff;

    for( uint16_t i = 0; i < sizeof(esp_stub_code); i++ ){

        checksum ^= pgm_read_byte( &esp_stub_code[i] );
    }

    esp_v_send_header( ESP_MEM_DATA, sizeof(esp_mem_block_t) + STUB_CODE_SIZE, checksum );

    esp_mem_block_t mem_block;
    mem_block.size = STUB_CODE_SIZE;
    mem_block.seq = 0;
    mem_block.reserved0 = 0;
    mem_block.reserved1 = 0;

    slip_v_send_data( (uint8_t *)&mem_block, sizeof(mem_block) );
    slip_v_send_data( (uint8_t *)&param, sizeof(param) );

    for( uint16_t i = 0; i < sizeof(esp_stub_code); i++ ){

        uint8_t b = pgm_read_byte( &esp_stub_code[i] );

        slip_v_send_byte( b );
    }

    hal_wifi_v_usart_send_char( SLIP_END );

    if( esp_i8_wait_response( buf, sizeof(buf), ESP_CMD_TIMEOUT ) < 0 ){

        return -3;
    }

    if( ( resp_body[0] != 0 ) || ( resp_body[1] != 0 ) ){

        return -4;
    }

    uint8_t stub_data[sizeof(esp_stub_data)];
    memcpy_P( stub_data, esp_stub_data, sizeof(stub_data) );
    mem_begin.size = sizeof(stub_data);
    mem_begin.num_blocks = 1;
    mem_begin.block_size = sizeof(stub_data);
    mem_begin.offset = ESP_STUB_DATA_START;

    esp_v_command( ESP_MEM_BEGIN, (uint8_t *)&mem_begin, sizeof(mem_begin), 0 );

    if( esp_i8_wait_response( buf, sizeof(buf), ESP_CMD_TIMEOUT ) < 0 ){

        return -5;
    }

    if( ( resp_body[0] != 0 ) || ( resp_body[1] != 0 ) ){

        return -6;
    }

    checksum = ESP_CHECKSUM_MAGIC;

    for( uint16_t i = 0; i < sizeof(stub_data); i++ ){

        checksum ^= stub_data[i];
    }

    mem_block.size = sizeof(stub_data);

    esp_v_send_header( ESP_MEM_DATA, sizeof(esp_mem_block_t) + sizeof(stub_data), checksum );
    slip_v_send_data( (uint8_t *)&mem_block, sizeof(mem_block) );
    slip_v_send_data( stub_data, sizeof(stub_data) );

    hal_wifi_v_usart_send_char( SLIP_END );

    if( esp_i8_wait_response( buf, sizeof(buf), ESP_CMD_TIMEOUT ) < 0 ){

        return -7;
    }

    if( ( resp_body[0] != 0 ) || ( resp_body[1] != 0 ) ){

        return -8;
    }

    esp_mem_finish_t mem_finish;
    mem_finish.zero = 0;
    mem_finish.entrypoint = ESP_STUB_ENTRY;

    esp_v_command( ESP_MEM_END, (uint8_t *)&mem_finish, sizeof(mem_finish), 0 );

    if( esp_i8_wait_response( buf, sizeof(buf), ESP_CMD_TIMEOUT ) < 0 ){

        return -9;
    }

    if( ( resp_body[0] != 0 ) || ( resp_body[1] != 0 ) ){

        return -10;
    }

    return 0;
}


// Cesanta protocol
int8_t esp_i8_load_flash( file_t file ){

    int32_t file_len;

    fs_v_seek( file, ESP_FW_INFO_ADDRESS );

    fs_i16_read( file, &file_len, sizeof(file_len) );

    log_v_debug_P( PSTR("loader fw len: %lu"), file_len );

    fs_v_seek( file, 0 );

    if( file_len <= 0 ){

        return -1;
    }

    // file length must be a multiple of flash sector size!
    if( ( file_len % ESP_FLASH_SECTOR ) != 0 ){

        return -2;
    }

    uint8_t buf[6];
    memset( buf, 0, sizeof(buf) );

    hal_wifi_v_usart_flush();

    esp_write_flash_t cmd;
    cmd.addr = 0;
    cmd.len = file_len;
    cmd.erase = 1; // erase before write

    hal_wifi_v_usart_send_char( SLIP_END );
    slip_v_send_byte( ESP_CESANTA_CMD_FLASH_WRITE );
    hal_wifi_v_usart_send_char( SLIP_END );

    hal_wifi_v_usart_send_char( SLIP_END );
    slip_v_send_data( (uint8_t *)&cmd, sizeof(cmd) );
    hal_wifi_v_usart_send_char( SLIP_END );

    // wait for buffer
    hal_wifi_i8_usart_receive( buf, sizeof(buf), 5000 );    

    if( !( ( buf[0] == SLIP_END ) &&
           ( buf[1] == 0 ) &&
           ( buf[2] == 0 ) &&
           ( buf[3] == 0 ) &&
           ( buf[4] == 0 ) &&
           ( buf[5] == SLIP_END ) ) ){
        
        return -3;
    }

    uint8_t file_buf[256];
    int32_t len = 0;

    while( len < file_len ){

        wdg_v_reset();

        memset( file_buf, 0xff, sizeof(file_buf) );

        int16_t read = fs_i16_read( file, file_buf, sizeof(file_buf) );

        if( read < 0 ){

            return -4;
        }

        hal_wifi_v_usart_flush();

        hal_wifi_v_usart_send_data( file_buf, sizeof(file_buf) );

        len += sizeof(file_buf);

        if( ( len % 1024 ) == 0 ){

            // checking that last response packet got sent,
            // but not actually checking value

            // note the long delay, some load commands have been observed
            // to take at least 1/3 of a second to run
            if( hal_wifi_i16_usart_get_char_timeout( 1000000 ) == SLIP_END ){

                _delay_ms( 1 );
            }
            else{

                return -5;
            }
        }
    }

    return 0;
}

// Cesanta protocol
int8_t esp_i8_md5( uint32_t len, uint8_t digest[MD5_LEN] ){

    esp_digest_t cmd;
    cmd.addr = 0;
    cmd.len = len;
    cmd.block_size = 0;

    memset( digest, 0, MD5_LEN );

    if( len == 0 ){

        return 0;
    }

    hal_wifi_v_usart_flush();

    hal_wifi_v_usart_send_char( SLIP_END );
    slip_v_send_byte( ESP_CESANTA_CMD_FLASH_DIGEST );
    hal_wifi_v_usart_send_char( SLIP_END );

    hal_wifi_v_usart_send_char( SLIP_END );
    slip_v_send_data( (uint8_t *)&cmd, sizeof(cmd) );
    hal_wifi_v_usart_send_char( SLIP_END );

    // checking that response packet got sent
    if( hal_wifi_i16_usart_get_char_timeout( 500000 ) != SLIP_END ){

        return -2;
    }

    memset( digest, 0xff, MD5_LEN );
    uint8_t md5_idx = 0;

    // parse response
    while( md5_idx < MD5_LEN ){

        int16_t byte = hal_wifi_i16_usart_get_char_timeout( 10000 );

        if( byte < 0 ){

            return -3;
        }
        else if( byte == SLIP_END ){

            break;
        }
        else if( byte == SLIP_ESC ){

            byte = hal_wifi_i16_usart_get_char_timeout( 10000 );

            if( byte < 0 ){

               return -4;
            }
            else if( byte == SLIP_ESC_END ){

                digest[md5_idx] = SLIP_END;
                md5_idx++;
            }
            else if( byte == SLIP_ESC_ESC ){

                digest[md5_idx] = SLIP_ESC;
                md5_idx++;
            }
        }
        else{

            digest[md5_idx] = byte;
            md5_idx++;
        }
    }

    return 0;
}

void esp_v_flash_end( void ){

    uint32_t cmd = 1; // do not reboot

    esp_v_command( ESP_FLASH_END, (uint8_t *)&cmd, sizeof(cmd), 0 );
}

static int8_t loader_status;


void wifi_v_start_loader( void ){

    thread_t_create_critical( THREAD_CAST(wifi_loader_thread),
                              PSTR("wifi_loader"),
                              0,
                              sizeof(loader_thread_state_t) );

    loader_status = ESP_LOADER_STATUS_BUSY;
}

int8_t wifi_i8_loader_status( void ){

    return loader_status;
}


PT_THREAD( wifi_loader_thread( pt_t *pt, loader_thread_state_t *state ) )
{
PT_BEGIN( pt );

    loader_status = ESP_LOADER_STATUS_BUSY;

    state->fw_file = 0;
    state->tries = WIFI_LOADER_MAX_TRIES;

    log_v_debug_P( PSTR("wifi loader starting") );
    
restart:

    // check if we've exceeded our retries
    if( state->tries == 0 ){

        // wifi is hosed, give up.
        goto error;
    }

    state->tries--;

    hal_wifi_v_enter_boot_mode();
    
    if( state->fw_file == 0 ){

        state->fw_file = fs_f_open_P( PSTR("wifi_firmware.bin"), FS_MODE_READ_ONLY );
    }

    if( state->fw_file <= 0 ){

        goto run_wifi;
    }

    // delay while wifi boots up
    TMR_WAIT( pt, 250 );

    state->timeout = 10;
    while( state->timeout > 0 ){

        hal_wifi_v_usart_flush();

        state->timeout--;

        if( state->timeout == 0 ){

            log_v_debug_P( PSTR("wifi loader timeout") );

            sys_v_reboot_delay(SYS_MODE_SAFE);
            THREAD_RESTART( pt );
        }

        uint8_t buf[32];
        memset( buf, 0xff, sizeof(buf) );

        esp_v_send_sync();

        // blocking wait!
        int8_t status = esp_i8_wait_response( buf, sizeof(buf), ESP_SYNC_TIMEOUT );

        if( status == 0 ){

            esp_response_t *resp = (esp_response_t *)buf;

            if( resp->opcode == ESP_SYNC ){

                break;
            }
        }

        TMR_WAIT( pt, 5 );
    }

    // delay, as Sync will output several responses
    TMR_WAIT( pt, 50 );

    int8_t status = esp_i8_load_cesanta_stub();

    if( status < 0 ){

        log_v_debug_P( PSTR("error %d"), status );

        TMR_WAIT( pt, 500 );
        THREAD_RESTART( pt );
    }

    // change baud rate
    hal_wifi_v_usart_set_baud( ESP_CESANTA_BAUD_USART_SETTING );
    hal_wifi_v_usart_flush();    


    uint8_t buf[4];
    esp_i8_wait_response( buf, sizeof(buf), 100000 );    

    if( ( buf[0] != 'O' ) ||
        ( buf[1] != 'H' ) ||
        ( buf[2] != 'A' ) ||
        ( buf[3] != 'I' ) ){

     log_v_debug_P( PSTR("error: 0x%02x 0x%02x 0x%02x 0x%02x"),
                buf[0],
                buf[1],
                buf[2],
                buf[3] );

        TMR_WAIT( pt, 500 );
        THREAD_RESTART( pt );
    }

    status_led_v_set( 1, STATUS_LED_BLUE );

    trace_printf( "Cesanta flasher ready!\r\n" );
    // log_v_debug_P( PSTR("Cesanta flasher ready!") );

    uint32_t file_len = 0;
    fs_v_seek( state->fw_file, ESP_FW_INFO_ADDRESS );
    fs_i16_read( state->fw_file, &file_len, sizeof(file_len) );
    log_v_debug_P( PSTR("wifi fw len: %lu"), file_len );

    uint8_t wifi_digest[MD5_LEN];

    int8_t md5_status = esp_i8_md5( file_len, wifi_digest );
    if( md5_status < 0 ){

        log_v_debug_P( PSTR("error %d"), md5_status );

        goto restart;
    }
    
    fs_v_seek( state->fw_file, file_len );
    
    uint8_t file_digest[MD5_LEN];
    memset( file_digest, 0, MD5_LEN );
    fs_i16_read( state->fw_file, file_digest, MD5_LEN );

    // check if we've never loaded the wifi before
    if( ( file_len == 0 ) && 
        ( fs_i32_get_size( state->fw_file ) < 128 ) ){

        goto error;
    }

    log_v_debug_P( PSTR("file md5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
            file_digest[0],
            file_digest[1],
            file_digest[2],
            file_digest[3],
            file_digest[4],
            file_digest[5],
            file_digest[6],
            file_digest[7],
            file_digest[8],
            file_digest[9],
            file_digest[10],
            file_digest[11],
            file_digest[12],
            file_digest[13],
            file_digest[14],
            file_digest[15] );

    log_v_debug_P( PSTR("esp md5:  %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
            wifi_digest[0],
            wifi_digest[1],
            wifi_digest[2],
            wifi_digest[3],
            wifi_digest[4],
            wifi_digest[5],
            wifi_digest[6],
            wifi_digest[7],
            wifi_digest[8],
            wifi_digest[9],
            wifi_digest[10],
            wifi_digest[11],
            wifi_digest[12],
            wifi_digest[13],
            wifi_digest[14],
            wifi_digest[15] );

    if( memcmp( file_digest, wifi_digest, MD5_LEN ) == 0 ){

        // all 3 match, run wifi
        // log_v_debug_P( PSTR("Wifi firmware image valid") );

        goto run_wifi;
    }
    else{
        // wifi does not match file - need to load

        log_v_debug_P( PSTR("Wifi firmware image fail") );

        goto load_image;
    }

    if( memcmp( wifi_digest, file_digest, MD5_LEN ) == 0 ){

        // in this case, file matches wifi, so our wifi image is valid
        // and so is our file.
        // but our cfg is mismatched.
        // so we'll restore it and then run the wifi
        log_v_debug_P( PSTR("Wifi MD5 mismatch, restored from file") );

        goto run_wifi;
    }

    // probably don't want to actually assert here...

    // try to run anyway
    goto run_wifi;



load_image:
    
    // #ifdef ENABLE_USB
    // usb_v_detach();
    // #endif

    log_v_debug_P( PSTR("Loading wifi image...") );

    status = esp_i8_load_flash( state->fw_file );
    if( status < 0 ){

        log_v_debug_P( PSTR("error %d"), status );
        goto error;
    }

    memset( wifi_digest, 0xff, MD5_LEN );

    status = esp_i8_md5( file_len, wifi_digest );
    if( status < 0 ){

        log_v_debug_P( PSTR("error %d"), status );
        goto restart;
    }

    // verify
    if( memcmp( wifi_digest, file_digest, MD5_LEN ) != 0 ){

        log_v_debug_P( PSTR("load md5: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"),
                wifi_digest[0],
                wifi_digest[1],
                wifi_digest[2],
                wifi_digest[3],
                wifi_digest[4],
                wifi_digest[5],
                wifi_digest[6],
                wifi_digest[7],
                wifi_digest[8],
                wifi_digest[9],
                wifi_digest[10],
                wifi_digest[11],
                wifi_digest[12],
                wifi_digest[13],
                wifi_digest[14],
                wifi_digest[15] );

        goto error;
    }

    log_v_debug_P( PSTR("Wifi flash load done") );

    if( state->fw_file > 0 ){

        fs_f_close( state->fw_file );
    }

    goto run_wifi;
    

error:

    loader_status = ESP_LOADER_STATUS_FAIL;

    hal_wifi_v_reset(); // hold chip in reset

    status_led_v_set( 1, STATUS_LED_RED );

    if( state->fw_file > 0 ){

        fs_f_close( state->fw_file );
    }


    log_v_debug_P( PSTR("wifi load fail") );

    THREAD_EXIT( pt );

run_wifi:
    
    loader_status = ESP_LOADER_STATUS_OK;

    status_led_v_set( 1, STATUS_LED_GREEN );

    
PT_END( pt );
}


#endif
