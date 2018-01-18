/*
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
 */

#include "target.h"

#ifdef ENABLE_CMD2

#include "cpu.h"

#include "system.h"
#include "memory.h"
#include "timers.h"
#include "threading.h"
#include "fs.h"
#include "sockets.h"
#include "config.h"
#include "cmd_usart.h"
#include "crc.h"
#include "keyvalue.h"

#include "command2.h"

#define NO_LOGGING
#include "logging.h"

// #define NO_EVENT_LOGGING
#include "event_log.h"

#define CMD2_DATA_BUF_SIZE ( CMD2_MAX_APP_DATA_LEN + sizeof(cmd2_header_t) )

#ifdef ENABLE_NETWORK
    #ifdef ENABLE_UDPX
    static socket_t udpx_sock;
    #endif
static socket_t udp_sock;
#endif


#ifdef ENABLE_NETWORK
#ifdef ENABLE_UDPX
    PT_THREAD( command2_server_thread( pt_t *pt, void *state ) );
#endif
PT_THREAD( command2_udp_server_thread( pt_t *pt, void *state ) );
#endif

#ifndef ENABLE_USB_UDP_TRANSPORT
PT_THREAD( command2_serial_thread( pt_t *pt, void *state ) );
#endif


void cmd2_v_init( void ){

    // return;

    #ifdef ENABLE_NETWORK

    udp_sock = sock_s_create( SOCK_DGRAM );
    ASSERT( udp_sock >= 0 );

    // // bind to server port
    sock_v_bind( udp_sock, CMD2_UDP_SERVER_PORT );

    #ifdef ENABLE_UDPX
    // create server thread
    thread_t_create( command2_server_thread,
                     PSTR("command_server"),
                     0,
                     0 );
    #endif

    // create server thread
    thread_t_create( command2_udp_server_thread,
                     PSTR("command_udp_server"),
                     0,
                     0 );

    #endif

    #ifndef ENABLE_USB_UDP_TRANSPORT
    // create serial thread
    thread_t_create( command2_serial_thread,
                     PSTR("command_serial_monitor"),
                     0,
                     0 );
    #endif
}

// runs command, returns response in a memory buffer
// returns -1 if the command could not be processed
mem_handle_t cmd2_h_process_cmd( const cmd2_header_t *cmd,
                                 int16_t len,
                                 sock_addr_t *src_addr ){

    void *data = (void *)cmd + sizeof(cmd2_header_t);
    len -= sizeof(cmd2_header_t);

    mem_handle_t response_h = -1;

    // response buffer for commands that reply with little data
    uint8_t small_buffer[8];
    int16_t response_len = 0;

    if( cmd->cmd == CMD2_ECHO ){

        response_h = mem2_h_alloc( len + sizeof(cmd2_header_t) );

        if( response_h < 0 ){

            return -1;
        }

        memcpy( mem2_vp_get_ptr( response_h ) + sizeof(cmd2_header_t), data, len );
    }
    else if( cmd->cmd == CMD2_REBOOT ){

        sys_v_reboot_delay( SYS_MODE_NORMAL );
    }
    else if( cmd->cmd == CMD2_SAFE_MODE ){

        sys_v_reboot_delay( SYS_MODE_SAFE );
    }
    else if( cmd->cmd == CMD2_LOAD_FW ){

        sys_v_load_fw();
    }
    else if( cmd->cmd == CMD2_FORMAT_FS ){

        sys_v_reboot_delay( SYS_MODE_FORMAT );
    }

    else if( cmd->cmd == CMD2_GET_FILE_ID ){

        file_id_t8 *id = (file_id_t8 *)small_buffer;

        *id = fs_i8_get_file_id( data );

        response_len = sizeof(file_id_t8);
    }
    else if( cmd->cmd == CMD2_CREATE_FILE ){

        file_t f = fs_f_open( data, FS_MODE_CREATE_IF_NOT_FOUND );

        if( f >= 0 ){

            // close file handle if exists
            fs_f_close( f );
        }

        file_id_t8 *id = (file_id_t8 *)small_buffer;

        // return file id
        *id = fs_i8_get_file_id( data );

        response_len = sizeof(file_id_t8);
    }
    else if( cmd->cmd == CMD2_READ_FILE_DATA ){

        cmd2_file_request_t *req = (cmd2_file_request_t *)data;

        int32_t file_size = fs_i32_get_size_id( req->file_id );

        // bounds check
        if( (int32_t)( req->pos + req->len ) >= file_size ){

    		req->len = file_size - req->pos;
    	}

        // allocate memory
        response_h = mem2_h_alloc( req->len + sizeof(cmd2_header_t) );

        if( response_h < 0 ){

            return -1;
        }

        // converting signed to unsigned, return 0 means EOF or not exists
        fs_i16_read_id( req->file_id,
                        req->pos,
                        mem2_vp_get_ptr( response_h ) + sizeof(cmd2_header_t),
                        req->len );
    }
    else if( cmd->cmd == CMD2_WRITE_FILE_DATA ){

        cmd2_file_request_t *req = (cmd2_file_request_t *)data;
        data += sizeof(cmd2_file_request_t);

        // bounds check
        if( req->len > CMD2_DATA_BUF_SIZE ){

            req->len = CMD2_DATA_BUF_SIZE;
        }

        uint16_t *write_len = (uint16_t *)small_buffer;

        // converting signed to unsigned, return 0 means EOF or not exists
        *write_len = fs_i16_write_id( req->file_id, req->pos, data, req->len );

        response_len = sizeof(uint16_t);
    }
    else if( cmd->cmd == CMD2_REMOVE_FILE ){

        file_id_t8 *id = (file_id_t8 *)data;
        int8_t *status = (int8_t *)small_buffer;

        *status = fs_i8_delete_id( *id );

        response_len = sizeof(int8_t);
    }
    else if( cmd->cmd == CMD2_RESET_CFG ){

        cfg_v_default_all();
    }
    else if( cmd->cmd == CMD2_SET_REG_BITS ){

        cmd2_reg_bits_t *bits = (cmd2_reg_bits_t *)data;

        volatile uint8_t *reg = (uint8_t *)bits->reg;

        ATOMIC;

        *reg |= bits->mask;

        END_ATOMIC;
    }
    else if( cmd->cmd == CMD2_CLR_REG_BITS ){

        cmd2_reg_bits_t *bits = (cmd2_reg_bits_t *)data;

        volatile uint8_t *reg = (uint8_t *)bits->reg;

        ATOMIC;

        *reg &= ~bits->mask;

        END_ATOMIC;
    }
    else if( cmd->cmd == CMD2_GET_REG ){

        uint16_t *reg_addr = (uint16_t *)data;

        volatile uint8_t *reg = (uint8_t *)*reg_addr;

        ATOMIC;

        *(uint8_t *)small_buffer = *reg;

        response_len = sizeof(uint8_t);

        END_ATOMIC;
    }
    // else if( cmd->cmd == CMD2_SET_KV ){

    //     response_len = kv_i16_batch_len( data, len, TRUE );

    //     if( response_len < 0 ){

    //         return -1;
    //     }

    //     // allocate memory
    //     response_h = mem2_h_alloc( response_len + sizeof(cmd2_header_t) );

    //     if( response_h < 0 ){

    //         return -1;
    //     }

    //     kv_i16_batch_set( data,
    //                       len,
    //                       mem2_vp_get_ptr( response_h ) + sizeof(cmd2_header_t),
    //                       response_len );
    // }
    // else if( cmd->cmd == CMD2_GET_KV ){

    //     response_len = kv_i16_batch_len( data, len, FALSE );

    //     if( response_len < 0 ){

    //         log_v_debug_P( PSTR("KV 1") );
    //         return -1;
    //     }

    //     // allocate memory
    //     response_h = mem2_h_alloc( response_len + sizeof(cmd2_header_t) );

    //     if( response_h < 0 ){

    //         log_v_debug_P( PSTR("KV 2") );
    //         return -1;
    //     }

    //     kv_i16_batch_get( data,
    //                       len,
    //                       mem2_vp_get_ptr( response_h ) + sizeof(cmd2_header_t),
    //                       response_len );
    // }
    else if( cmd->cmd == CMD2_SET_SECURITY_KEY ){

        cmd2_set_sec_key_t *k = (cmd2_set_sec_key_t *)data;

        cfg_v_set_security_key( k->key_id, k->key );
    }
    else{

        goto clean_up;
    }

    // check for response
    if( ( response_len >= 0 ) && ( response_h < 0 ) ){

        response_h = mem2_h_alloc( response_len + sizeof(cmd2_header_t) );

        if( response_h < 0 ){

            return -1;
        }

        // set up header
        cmd2_header_t *header = (cmd2_header_t *)mem2_vp_get_ptr( response_h );

        header->cmd = cmd->cmd;
        uint8_t *data = (uint8_t *)( header + 1 );
        memcpy( data, small_buffer, response_len );
    }
    else if( response_h > 0 ){

        // attach header
        cmd2_header_t *header = (cmd2_header_t *)mem2_vp_get_ptr( response_h );

        header->cmd = cmd->cmd;
    }

clean_up:

    return response_h;
}


#ifdef ENABLE_NETWORK
#ifdef ENABLE_UDPX
PT_THREAD( command2_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    udpx_sock = sock_s_create( SOCK_UDPX_SERVER );
    ASSERT( udpx_sock >= 0 );

    // bind to server port
    sock_v_bind( udpx_sock, CMD2_SERVER_PORT );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( udpx_sock ) < 0 );

        sock_addr_t src_addr;
        sock_v_get_raddr( udpx_sock, &src_addr );

        // log_v_debug_P( PSTR("cmd2 size: %d"), sock_i16_get_bytes_read( udpx_sock ) );

        // process command
        mem_handle_t response_h = cmd2_h_process_cmd( sock_vp_get_data( udpx_sock ),
                                                      sock_i16_get_bytes_read( udpx_sock ),
                                                      &src_addr );

        // check if there is a response to send
        if( response_h >= 0 ){

            // log_v_debug_P( PSTR("cmd2 response: %d"), mem2_u16_get_size( response_h ) );

            sock_i16_sendto_m( udpx_sock,
                               response_h,
                               0 );

        }
        else{

            // log_v_debug_P( PSTR("cmd2 NO response") );
        }
    }

PT_END( pt );
}
#endif
#endif


PT_THREAD( command2_udp_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( udp_sock ) < 0 );

        sock_addr_t src_addr;
        sock_v_get_raddr( udp_sock, &src_addr );

        // log_v_debug_P( PSTR("cmd2 size: %d"), sock_i16_get_bytes_read( udp_sock ) );

        // process command
        mem_handle_t response_h = cmd2_h_process_cmd( sock_vp_get_data( udp_sock ),
                                                      sock_i16_get_bytes_read( udp_sock ),
                                                      &src_addr );

        // check if there is a response to send
        if( response_h >= 0 ){

            // log_v_debug_P( PSTR("cmd2 response: %d"), mem2_u16_get_size( response_h ) );

            sock_i16_sendto_m( udp_sock,
                               response_h,
                               0 );
        }
        else{

            // log_v_debug_P( PSTR("cmd2 NO response") );
        }
    }

PT_END( pt );
}


#ifndef ENABLE_USB_UDP_TRANSPORT
static mem_handle_t handle;
static uint16_t count;
static uint16_t idx;
static uint32_t timeout;

PT_THREAD( command2_serial_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_YIELD( pt );

        handle = -1;
        count = 0;
        idx = 0;

        // wait for a frame start indicator
        THREAD_WAIT_WHILE( pt, cmd_usart_i16_get_char() != CMD2_SERIAL_SOF );

        // EVENT( EVENT_ID_CMD_START, 0 );

        // wait for header
        THREAD_WAIT_WHILE( pt, cmd_usart_u8_rx_size() < sizeof(cmd2_serial_frame_header_t) );

        // copy header
        cmd2_serial_frame_header_t header;
        uint8_t *ptr = (uint8_t *)&header;

        for( uint8_t i = 0; i < sizeof(header); i++ ){

            *ptr = cmd_usart_i16_get_char();
            ptr++;
        }

        // check header
        if( ( header.len != ~header.inverted_len ) ||
            ( header.len > CMD2_MAX_PACKET_LEN ) ){

            continue;
        }

        count = header.len + sizeof(uint16_t);

        // allocate memory, with room for CRC
        handle = mem2_h_alloc2( count, MEM_TYPE_CMD_BUFFER );

        // check if allocation succeeded
        if( handle < 0 ){

            continue;
        }

        timeout = tmr_u32_get_system_time_ms();

        while( idx < count ){

            THREAD_WAIT_WHILE( pt, ( cmd_usart_u8_rx_size() == 0 ) &&
                                   ( tmr_u32_elapsed_time_ms( timeout ) <= 250 ) );

            if( tmr_u32_elapsed_time_ms( timeout ) > 250 ){

                goto timeout;
            }

            uint8_t *buf = mem2_vp_get_ptr( handle ) + idx;

            *buf = cmd_usart_i16_get_char();
            idx++;
        }

        // check CRC
        if( crc_u16_block( mem2_vp_get_ptr( handle ),
                           mem2_u16_get_size( handle ) ) != 0 ){

            // send NAK
            cmd_usart_v_send_char( CMD2_SERIAL_NAK );

            // EVENT( EVENT_ID_CMD_CRC_ERROR, 0 );

            goto cleanup;
        }

        // send ACK
        cmd_usart_v_send_char( CMD2_SERIAL_ACK );

        sock_addr_t src;
        memset( &src, 0, sizeof(src) );

        // run command processor
        mem_handle_t response_h = cmd2_h_process_cmd( mem2_vp_get_ptr( handle ),
                                                      mem2_u16_get_size( handle ) - sizeof(uint16_t),
                                                      &src );

        // check response
        if( response_h >= 0 ){

            // transmit frame header
            cmd2_serial_frame_header_t header;

            header.len          = mem2_u16_get_size( response_h );
            header.inverted_len = ~header.len;

            cmd_usart_v_send_data( (uint8_t *)&header, sizeof(header) );

            // set up packet transfer
            uint16_t len = header.len;
            uint8_t *ptr = mem2_vp_get_ptr( response_h );

            while( len > 0 ){

                // send byte
                cmd_usart_v_send_char( *ptr );

                // adjust pointer and count
                ptr++;
                len--;
            }

            // compute CRC for data
            uint16_t crc = crc_u16_block( mem2_vp_get_ptr( response_h ),
                                          header.len );

            // switch CRC to big endian
            crc = HTONS(crc);

            // send CRC
            cmd_usart_v_send_data( (uint8_t *)&crc, sizeof(crc) );

            // release memory
            mem2_v_free( response_h );
        }

goto cleanup;

timeout:
    cmd_usart_v_flush();

cleanup:

        // release memory
        mem2_v_free( handle );
        handle = -1;

    }

PT_END( pt );
}
#endif
#endif