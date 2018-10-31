// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#ifdef ENABLE_WIFI

#include "fs.h"
#include "timers.h"
#include "config.h"
#include "watchdog.h"

#include "hal_esp8266.h"
#include "esp8266_loader.h"

// #define NO_LOGGING
#include "logging.h"


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


int8_t esp_i8_wait_response( uint8_t *buf, uint8_t len, uint32_t timeout ){

    int8_t status = -100;
    uint32_t start_time = tmr_u32_get_system_time_us();

    uint8_t next_byte = 0;
    // hal_wifi_v_clear_rx_buffer();
    // hal_wifi_v_enable_rx_dma( FALSE );

    volatile uint8_t *rx_dma_buf = hal_wifi_u8p_get_rx_dma_buf_ptr();

    // waiting for frame start
    while( rx_dma_buf[next_byte] != SLIP_END ){

        if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

            status = -1;
            goto end;
        }
    }

    next_byte++;

    while(1){

        if( len == 0 ){

            status = -3;
            goto end;
        }

        // wait for byte
        while( hal_wifi_u16_dma_rx_bytes() == next_byte ){

            if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

                status = -4;
                goto end;
            }
        }

        uint8_t b = rx_dma_buf[next_byte];
        next_byte++;

        if( b == SLIP_END ){

            status = 0;
            goto end;
        }
        else if( b == SLIP_ESC ){

            // wait for byte
            while( hal_wifi_u16_dma_rx_bytes() == next_byte ){

                if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

                    status = -5;
                    goto end;
                }
            }

            b = rx_dma_buf[next_byte];
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

            *buf = b;
            buf++;
            len--;
        }
    }

end:

    if( status != 0 ){

        log_v_debug_P( PSTR("loader error: %d"), status );
        log_v_debug_P( PSTR("%2x %2x %2x %2x %2x %2x %2x %2x"), rx_dma_buf[0], rx_dma_buf[1], rx_dma_buf[2], rx_dma_buf[3], rx_dma_buf[4], rx_dma_buf[5], rx_dma_buf[6], rx_dma_buf[7] );
    }

    hal_wifi_v_disable_rx_dma();

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

    // make sure file is at position 0!
    fs_v_seek( file, 0 );

    int32_t file_len = fs_i32_get_size( file );

    // file image will have md5 checksum appended, so throw away last 16 bytes
    file_len -= MD5_LEN;

    if( file_len < 0 ){

        return -1;
    }

    uint32_t cfg_file_len;
    cfg_i8_get( CFG_PARAM_WIFI_FW_LEN, &cfg_file_len );

    // check for config and file length mismatch
    if( cfg_file_len != (uint32_t)file_len ){

        return -1;
    }

    hal_wifi_v_clear_rx_buffer();
    hal_wifi_v_enable_rx_dma( FALSE );

    // cast rx_dma_buf to a volatile pointer.
    // otherwise, GCC will attempt to be clever and cache
    // the first byte of rx_dma_buf, which will cause
    // the check for SLIP_END to fail.
    volatile uint8_t *buf = hal_wifi_u8p_get_rx_dma_buf_ptr();

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

    for( uint8_t i = 0; i < 100; i++ ){

        _delay_us( 50 );

        if( buf[5] == SLIP_END ){

            break;
        }
    }

    if( !( ( buf[0] == SLIP_END ) &&
           ( buf[1] == 0 ) &&
           ( buf[2] == 0 ) &&
           ( buf[3] == 0 ) &&
           ( buf[4] == 0 ) &&
           ( buf[5] == SLIP_END ) ) ){

        log_v_debug_P( PSTR("error") );

        hal_wifi_v_disable_rx_dma();
        return -1;
    }

    // This buffer eats a lot of stack.
    // At some point we could rework this to use the rx_dma_buf,
    // or load the data in smaller chunks.
    uint8_t file_buf[256];
    int32_t len = 0;

    while( len < file_len ){

        wdg_v_reset();

        memset( file_buf, 0xff, sizeof(file_buf) );

        int16_t read = fs_i16_read( file, file_buf, sizeof(file_buf) );

        if( read < 0 ){

            hal_wifi_v_disable_rx_dma();
            return -2;
        }

        if( ( file_buf[0] == ESP_IMAGE_MAGIC ) && ( len == 0 ) ){

            file_buf[2] = 0;
            file_buf[3] = 0;
        }

        hal_wifi_v_usart_send_data( file_buf, sizeof(file_buf) );

        len += sizeof(file_buf);

        if( ( len % 1024 ) == 0 ){

            hal_wifi_v_clear_rx_buffer();
            hal_wifi_v_enable_rx_dma( FALSE );

            for( uint8_t i = 0; i < 250; i++ ){

                _delay_ms( 1 );

                // checking that last response packet got sent,
                // but not actually checking value
                if( buf[0] == SLIP_END ){

                    _delay_ms( 1 );

                    break;
                }
            }
        }
    }

    hal_wifi_v_disable_rx_dma();

    return 0;
}

// Cesanta protocol
int8_t esp_i8_md5( uint32_t len, uint8_t digest[MD5_LEN] ){

    hal_wifi_v_clear_rx_buffer();
    hal_wifi_v_enable_rx_dma( FALSE );

    esp_digest_t cmd;
    cmd.addr = 0;
    cmd.len = len;
    cmd.block_size = 0;


    hal_wifi_v_usart_send_char( SLIP_END );
    slip_v_send_byte( ESP_CESANTA_CMD_FLASH_DIGEST );
    hal_wifi_v_usart_send_char( SLIP_END );

    hal_wifi_v_usart_send_char( SLIP_END );
    slip_v_send_data( (uint8_t *)&cmd, sizeof(cmd) );
    hal_wifi_v_usart_send_char( SLIP_END );

    // cast rx_dma_buf to a volatile pointer.
    // otherwise, GCC will attempt to be clever and cache
    // the first byte of rx_dma_buf, which will cause
    // the check for SLIP_END to fail.
    volatile uint8_t *buf = hal_wifi_u8p_get_rx_dma_buf_ptr();

    for( uint8_t i = 0; i < 250; i++ ){

        _delay_ms( 2 );

        // checking that last response packet got sent,

        if( buf[0] == SLIP_END ){

            _delay_ms( 5 ); // wait for rest of response

            break;
        }
    }

    if( buf[0] != SLIP_END ){

        hal_wifi_v_disable_rx_dma();
        return -2;
    }


    memset( digest, 0xff, MD5_LEN );
    uint8_t md5_idx = 0;

    // parse response
    for( uint8_t i = 1; i < WIFI_UART_RX_BUF_SIZE; i++ ){

        if( md5_idx >= MD5_LEN ){

            break;
        }

        if( buf[i] == SLIP_END ){

            break;
        }
        else if( buf[i] == SLIP_ESC ){

            i++;

            if( buf[i] == SLIP_ESC_END ){

                digest[md5_idx] = SLIP_END;
                md5_idx++;
            }
            else if( buf[i] == SLIP_ESC_ESC ){

                digest[md5_idx] = SLIP_ESC;
                md5_idx++;
            }
        }
        else{

            digest[md5_idx] = buf[i];
            md5_idx++;
        }
    }

    hal_wifi_v_disable_rx_dma();
    return 0;
}

void esp_v_flash_end( void ){

    uint32_t cmd = 1; // do not reboot

    esp_v_command( ESP_FLASH_END, (uint8_t *)&cmd, sizeof(cmd), 0 );
}

#endif
