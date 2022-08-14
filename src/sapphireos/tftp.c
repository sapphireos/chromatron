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


/*

Trivial File Transfer Protocol

Notes and limitations:
Only the "octet" (binary) mode is supported.  Mode strings are ignored.


The following is from RFC 1350:

TFTP Formats

   Type   Op #     Format without header

          2 bytes    string   1 byte     string   1 byte
          -----------------------------------------------
   RRQ/  | 01/02 |  Filename  |   0  |    Mode    |   0  |
   WRQ    -----------------------------------------------
          2 bytes    2 bytes       n bytes
          ---------------------------------
   DATA  | 03    |   Block #  |    Data    |
          ---------------------------------
          2 bytes    2 bytes
          -------------------
   ACK   | 04    |   Block #  |
          --------------------
          2 bytes  2 bytes        string    1 byte
          ----------------------------------------
   ERROR | 05    |  ErrorCode |   ErrMsg   |   0  |
          ----------------------------------------

Initial Connection Protocol for reading a file

   1. Host  A  sends  a  "RRQ"  to  host  B  with  source= A's TID,
      destination= 69.

   2. Host B sends a "DATA" (with block number= 1) to host  A  with
      source= B's TID, destination= A's TID.



Longest packet will be 4 bytes header + 512 bytes data = 516 bytes

*/

#if 0

#include "cpu.h"

#include "fs.h"
#include "timers.h"
#include "threading.h"
#include "sockets.h"
#include "system.h"

#include "tftp.h"


typedef struct{
    socket_t sock;
    sock_addr_t raddr;
    file_t file;
    tftp_cmd_t tftp_cmd;
    uint16_t next_block;
    uint16_t bytes_read;
    uint32_t timeout;
} tftp_read_state_t;

typedef struct{
    socket_t sock;
    sock_addr_t raddr;
    file_t file;
    tftp_cmd_t tftp_cmd;
    uint16_t next_block;
    uint32_t timeout;
    tftp_ack_t tftp_ack;
} tftp_write_state_t;


static void send_error( uint16_t err_code, socket_t sock );
static void build_ack( tftp_ack_t *ack, uint16_t block_number );

#define MINIMUM_CMD_PKT_SIZE 6
#define MINIMUM_ACK_PKT_SIZE 4
#define MINIMUM_DATA_PKT_SIZE 4


PT_THREAD( tftp_server_thread( pt_t *pt, void *state ) );
PT_THREAD( tftp_read_thread( pt_t *pt, tftp_read_state_t *state ) );
PT_THREAD( tftp_write_thread( pt_t *pt, tftp_write_state_t *state ) );


// initialize the TFTP server
void tftp_v_init( void ){

	thread_t_create( tftp_server_thread,
                     PSTR("tftp_server"),
                     0,
                     0 );
}


/*****************************************************
 TFTP Server Main
*****************************************************/
PT_THREAD( tftp_server_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	static socket_t sock;

	// create socket
	sock = sock_s_create( SOS_SOCK_DGRAM );

	ASSERT( sock >= 0 );

	// bind to port
	sock_v_bind( sock, TFTP_SERVER_PORT );

	while(1){

        THREAD_YIELD( pt );

		// wait for data
        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        // verify packet length
        if( sock_i16_get_bytes_read( sock ) < MINIMUM_CMD_PKT_SIZE ){

            send_error( TFTP_ERR_ILLEGAL_OP, sock );

            continue;
        }

        // map data into tftp command packet structure
        tftp_cmd_t *tftp_cmd = sock_vp_get_data( sock );

        // switch endianess of the opcode
		tftp_cmd->opcode = HTONS( tftp_cmd->opcode );

		// received a read request
		if( tftp_cmd->opcode == TFTP_RRQ ){

            tftp_read_state_t thread_state;

            // set up the thread state
            sock_v_get_raddr( sock, &thread_state.raddr );
            thread_state.tftp_cmd = *tftp_cmd;

            // create the reader thread
            thread_t_create( THREAD_CAST( tftp_read_thread ),
                             PSTR("tftp_read"),
                             &thread_state,
                             sizeof(tftp_read_state_t) );
        }
        // received a write request
        else if( tftp_cmd->opcode == TFTP_WRQ ){

            tftp_read_state_t thread_state;

            // set up the thread state
            sock_v_get_raddr( sock, &thread_state.raddr );
            thread_state.tftp_cmd = *tftp_cmd;

            // create the write thread
            thread_t_create( THREAD_CAST( tftp_write_thread ),
                             PSTR("tftp_write"),
                             &thread_state,
                             sizeof(tftp_write_state_t) );
        }
        // received something else
        else{

            send_error( TFTP_ERR_ILLEGAL_OP, sock );

            continue;
        }
    }

PT_END( pt );
}

/*****************************************************
 TFTP Read
*****************************************************/
PT_THREAD( tftp_read_thread( pt_t *pt, tftp_read_state_t *state ) )
{
PT_BEGIN( pt );

    // create socket
    state->sock = sock_s_create( SOS_SOCK_DGRAM );

    // if socket creation fails, kill thread.  client will have to retry
    if( state->sock < 0 ){

        THREAD_EXIT( pt );
    }

    // bind socket to remote address
    sock_v_set_raddr( state->sock, &state->raddr );

    // wait on FS
    THREAD_WAIT_WHILE( pt, fs_b_busy() );

    // attempt to open file
    state->file = fs_f_open( state->tftp_cmd.filename, FS_MODE_READ_ONLY );

    // check if file exists
    if( state->file < 0 ){

        // send error and bail
        send_error( TFTP_ERR_FILE_NOT_FOUND, state->sock );

        goto clean_up_sock;
    }

    // init block counter
    state->next_block = 1;

    while(1){

        THREAD_YIELD( pt );

        // wait on FS
        THREAD_WAIT_WHILE( pt, fs_b_busy() );

        // create data packet
        tftp_data_t tftp_data;

        tftp_data.opcode = HTONS( TFTP_DATA );
        tftp_data.block = HTONS( state->next_block );

        state->bytes_read = fs_i16_read( state->file, tftp_data.data, 512 );

        // send packet
        // this might fail, but since UDP is unreliable anyway, even if
        // resource allocation fails and we don't even transmit the message,
        // the client should try again anyway.
        sock_i16_sendto( state->sock,
                         &tftp_data,
                         state->bytes_read + MINIMUM_DATA_PKT_SIZE, 0 );

        // set a timeout
        state->timeout = tmr_u32_get_system_time() + TFTP_TIMEOUT_MS;

        // wait for response
        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( state->sock ) < 0 ) &&
                               ( tmr_i8_compare_time( state->timeout ) > 0 ) );

        // check for timeout or invalid packet length
        if( sock_i16_get_bytes_read( state->sock ) < MINIMUM_ACK_PKT_SIZE ){

            // check if this was the last block, if so, then the client
            // probably received the data packet, sent an ack, and then
            // terminated.  We didn't receive the ack, but it is probably safe
            // to assume that the client would have been retying within our
            // timeout interval if it didn't receive something.
            if( state->bytes_read == 512 ){

                send_error( TFTP_ERR_TIMED_OUT, state->sock );
            }

            goto clean_up;
        }

        // map to tftp packet
        tftp_ack_t *tftp_ack = sock_vp_get_data( state->sock );

        // check opcode
        if( HTONS(tftp_ack->opcode) == TFTP_ACK ){

            // check block number
            if( HTONS(tftp_ack->block) == state->next_block ){

                // increment next block
                state->next_block++;

                // check for end of file
                if( state->bytes_read < 512 ){

                    // end of file reached, we're done
                    goto clean_up;
                }
            }
            else{

                // move file marker backwards so we can resend
                fs_v_seek( state->file,
                           fs_i32_tell( state->file ) - state->bytes_read );
            }
        }
        else{

            // invalid opcode
            send_error( TFTP_ERR_ILLEGAL_OP, state->sock );

            goto clean_up;
        }
    }

clean_up:
    fs_f_close( state->file );

clean_up_sock:
    sock_v_release( state->sock );
    // this will also clear the socket's data handle, if it had one

PT_END( pt );
}

/*****************************************************
 TFTP Write
*****************************************************/
PT_THREAD( tftp_write_thread( pt_t *pt, tftp_write_state_t *state ) )
{
PT_BEGIN( pt );

    // create socket
    state->sock = sock_s_create( SOS_SOCK_DGRAM );

    // if socket creation fails, kill thread.  client will have to retry
    if( state->sock < 0 ){

        THREAD_EXIT( pt );
    }

    // bind socket to remote address
    sock_v_set_raddr( state->sock, &state->raddr );

    // wait on FS
    THREAD_WAIT_WHILE( pt, fs_b_busy() );

    // open file
    state->file = fs_f_open( state->tftp_cmd.filename,
                             FS_MODE_WRITE_OVERWRITE |
                             FS_MODE_CREATE_IF_NOT_FOUND );

    // check if file was opened or created
    if( state->file < 0 ){

        // send error and bail
        send_error( TFTP_ERR_ACCESS_VIOLATION, state->sock );

        goto clean_up_sock;
    }

    // build an ack packet and send it
    build_ack( &state->tftp_ack, 0 );
    sock_i16_sendto( state->sock,
                     &state->tftp_ack,
                     sizeof(state->tftp_ack),
                     0 );

    // set expected next block
    state->next_block = 1;

    // write loop
    while(1){

        THREAD_YIELD( pt );

        // set a timeout
        state->timeout = tmr_u32_get_system_time() + TFTP_TIMEOUT_MS;

        // wait for data
        THREAD_WAIT_WHILE( pt, ( sock_i8_recvfrom( state->sock ) < 0 ) &&
                               ( tmr_i8_compare_time( state->timeout ) > 0 ) );

        // check for data
        if( sock_i16_get_bytes_read( state->sock ) < MINIMUM_DATA_PKT_SIZE ){

            send_error( TFTP_ERR_TIMED_OUT, state->sock );

            // timeout occurred, clean up
            goto clean_up;
        }

        // wait on FS
        THREAD_WAIT_WHILE( pt, fs_b_busy() );

        // map to packet
        tftp_data_t *tftp_data = sock_vp_get_data( state->sock );
        int16_t data_length = sock_i16_get_bytes_read( state->sock ) - MINIMUM_DATA_PKT_SIZE;

        // check that data was received
        if( ( HTONS( tftp_data->opcode ) == TFTP_DATA ) &&
            ( sock_i16_get_bytes_read( state->sock ) >= MINIMUM_DATA_PKT_SIZE ) &&
            ( data_length <= 512 ) ){

            // check if next block was received
            if( HTONS(tftp_data->block) == state->next_block ){

                // write data to file system, and check how much data was written
                if( fs_i16_write( state->file,
                                  tftp_data->data,
                                  data_length ) < data_length ){

                    // the file system didn't write the requested amount of data.
                    // most likely the file system is full, so send an error.
                    send_error( TFTP_ERR_DISK_FULL, state->sock );

                    goto clean_up;
                }

                // build an ack packet and send it
                build_ack( &state->tftp_ack, state->next_block );
                sock_i16_sendto( state->sock,
                                 &state->tftp_ack,
                                 sizeof(state->tftp_ack),
                                 0 );

                // check if first block and data length is 0
                if( ( state->next_block == 1 ) && ( data_length == 0 ) ){

                    // delete the file
                    fs_v_delete( state->file );
                }

                state->next_block++;

                // if last block
                if( data_length < 512 ){

                    // clean up, we're done
                    goto clean_up;
                }
            }
            else{

                // resend the last ack
                build_ack( &state->tftp_ack, state->next_block - 1 );
                sock_i16_sendto( state->sock,
                                 &state->tftp_ack,
                                 sizeof(state->tftp_ack),
                                 0 );
            }
        }
        else{
            // received something other than a data packet
            send_error( TFTP_ERR_ILLEGAL_OP, state->sock );

            goto clean_up;
        }
    }

clean_up:
    fs_f_close( state->file );

clean_up_sock:
    sock_v_release( state->sock );
    // this will also clear the socket's data handle, if it had one

PT_END( pt );
}


// Send an error packet
//
// NOTES:
// Since UDP transmission is unreliable anyway, this function will only attempt
// to send an error message, and may fail if buffer memory or other resources
// are not available.  This is fine since any client will already have to
// handle the possibility that a packet may not be received even if it was
// sent.  Thus, error reporting is done as a best-effort convenience, not a
// reliable service.
static void send_error( uint16_t err_code, socket_t sock ){

    tftp_err_t tftp_err;

    tftp_err.opcode = HTONS(TFTP_ERROR);
    tftp_err.err_code = HTONS(err_code);

    switch( err_code ){

        case TFTP_ERR_UNDEFINED:

            strlcpy_P( tftp_err.err_string,
                       PSTR("Undefined error"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_FILE_NOT_FOUND:

            strlcpy_P( tftp_err.err_string,
                       PSTR("File not found"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_ACCESS_VIOLATION:

            strlcpy_P( tftp_err.err_string,
                       PSTR("Access violation"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_DISK_FULL:

            strlcpy_P( tftp_err.err_string,
                       PSTR("Disk full"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_ILLEGAL_OP:

            strlcpy_P( tftp_err.err_string,
                       PSTR("Illegal operation"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_UNKNOWN_XFER_ID:

            strlcpy_P( tftp_err.err_string,
                       PSTR("Unknown transfer ID"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_FILE_ALREADY_EXISTS:

            strlcpy_P( tftp_err.err_string,
                       PSTR("File already exists"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_NO_SUCH_USER:

            strlcpy_P( tftp_err.err_string,
                       PSTR("No such user"),
                       sizeof(tftp_err.err_string) );

            break;

        case TFTP_ERR_TIMED_OUT:

            strlcpy_P( tftp_err.err_string,
                       PSTR("Timed out"),
                       sizeof(tftp_err.err_string) );

            break;

        default:

            ASSERT( FALSE );
    }

    // error codes greater than 7 are not defined in RFC1350, so send them
    // as undefined
    if( err_code > 7 ){

        tftp_err.err_code = HTONS(TFTP_ERR_UNDEFINED);
    }

    // send packet
    sock_i16_sendto( sock,
                     &tftp_err,
                     sizeof(tftp_err),
                     0 );
}

// build an ack packet
static void build_ack( tftp_ack_t *ack, uint16_t block_number ){

	ack->opcode = HTONS( TFTP_ACK );
	ack->block = HTONS( block_number );
}


#endif
