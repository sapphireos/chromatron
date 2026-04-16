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

Protocol notes:

Designed for high speed, low latency array streaming into the FX VM.

Server is sending u16 data, 256 items per data set.

This side implements the client.

The server is discovered via devicedb query for "superconductor".

FX4 programs can request superconductor linkage:

fixed16 my_array[16];
superconductor_request("audio_data", my_array);

Links "audio_data" stream into FX4 variable array my_array.
Data type is converted automatically on the client side.

The VM runtime will preload superconductor data into the
target array, automatically matching the array length.

Optional decimation feature: resize incoming array to smaller array.
Offer average decimation (integer reduction to nearest array fit)
Bonus feature would be LTTB decimation to match target array.


*/



#include "sapphire.h"

#include "superconductor.h"
#include "gfx_lib.h"

// static catbus_string_t banks[SC_MAX_BANKS];

// int8_t _sc_kv_handler(
//     kv_op_t8 op,
//     catbus_hash_t32 hash,
//     void *data,
//     uint16_t len )
// {
//     if( op == KV_OP_GET ){
        
//     }
//     else if( op == KV_OP_SET ){

//     	char *s = (char *)data;	

//         // set DB name for KV lookups
//         kvdb_v_set_name( s );
//         // this really needs to be done in the KVDB add function.
//     }
//     else{

//         ASSERT( FALSE );
//     }

//     return 0;
// }

#define MAX_BANDS 256
static uint16_t audio_data[MAX_BANDS];

static uint16_t dec_offset;
static uint16_t dec_count;

KV_SECTION_META kv_meta_t superconductor_info_kv[] = {
    { CATBUS_TYPE_UINT16,     MAX_BANDS - 1, KV_FLAGS_READ_ONLY,  audio_data,     0,  "superconductor_data" },

    { CATBUS_TYPE_UINT16,     0,             0,                   &dec_offset,    0,  "superconductor_dec_offset"},
    { CATBUS_TYPE_UINT16,     0,             0,                   &dec_count,     0,  "superconductor_dec_count"},

//     { CATBUS_TYPE_STRING32, 	0, 0,  				  &banks[0],    _sc_kv_handler,  "sc_bank0" },
//     { CATBUS_TYPE_STRING32, 	0, 0,  				  &banks[1],    _sc_kv_handler,  "sc_bank1" },
//     { CATBUS_TYPE_STRING32, 	0, 0,  				  &banks[2],    _sc_kv_handler,  "sc_bank2" },
//     { CATBUS_TYPE_STRING32, 	0, 0,  				  &banks[3],    _sc_kv_handler,  "sc_bank3" },
};


static socket_t sock;
// static uint32_t timer;


// static void send_init_msg( void ){

// 	sc_msg_init_t msg;
// 	memset( &msg, 0, sizeof(msg) );

// 	msg.hdr.magic    = SC_MAGIC;
// 	msg.hdr.msg_type = SC_MSG_TYPE_INIT;

// 	for( uint8_t i = 0; i < SC_MAX_BANKS; i++ ){

// 		msg.banks[i] = banks[i];
// 	}

// 	sock_addr_t raddr = services_a_get( __KV__SuperConductor, 0 );

// 	sock_i16_sendto( sock, &msg, sizeof(msg), &raddr );
// }

// static bool sc_enabled( void ){

//     for( uint8_t i = 0; i < SC_MAX_BANKS; i++ ){

//         if( banks[i].str[0] != 0 ){

//             return TRUE;
//         }
//     }

//     return FALSE;
// }


void decimate( const uint16_t input_array[MAX_BANDS], uint16_t output_array[MAX_BANDS], uint16_t offset, uint16_t count ){

    if( count == 0 ){

        count = gfx_u16_get_pix_count();
    }

    uint16_t factor = MAX_BANDS / count;

    if( factor == 0 ){

        // override to factor of 1
        factor = 1;
    }

    if( offset >= MAX_BANDS ){

        return;
    }

    // check bands
    if( ( offset + count ) >= MAX_BANDS ){

        count = MAX_BANDS - offset - 1;
    }

    for( uint16_t i = 0; i < count; i += factor ){

        uint32_t accum = 0;

        for( uint16_t j = 0; j < factor; j++ ){

            accum += input_array[offset + i + j];
        }

        accum /= factor;

        output_array[i] = accum;
    }
}


PT_THREAD( superconductor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // create socket
    sock = sock_s_create( SOS_SOCK_DGRAM );

    if( sock < 0 ){

        log_v_critical_P( PSTR("socket fail") );

        THREAD_EXIT( pt );      
    }

    sock_v_bind( sock, SC_PORT );
    // sock_v_set_timeout( sock, 1 );

    while( 1 ){

        THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

        if( sock_i16_get_bytes_read( sock ) <= 0 ){

            continue;
        }

        const sc_msg_hdr_t *header = sock_vp_get_data( sock );

        if( header->magic != SC_MAGIC ){

            continue;
        }
            
        // log_v_debug_P( PSTR("received superconductor") );

        uint16_t *msg_data = (uint16_t *)( header + 1 );

        decimate( msg_data, audio_data, dec_offset, dec_count );

        // memcpy(audio_data, msg_data, sizeof(audio_data));
    }
    



//     THREAD_WAIT_WHILE( pt, !sc_enabled() );

// 	// log_v_info_P( PSTR("SuperConductor server is waiting") );
  
// 	// services_v_listen( __KV__SuperConductor, 0 );

// 	// THREAD_WAIT_WHILE( pt, !services_b_is_available( __KV__SuperConductor, 0 ) );

// 	// service is available

// 	// create socket
//     sock = sock_s_create( SOS_SOCK_DGRAM );

//     if( sock < 0 ){

// 		log_v_critical_P( PSTR("socket fail") );

// 		THREAD_EXIT( pt );    	
//     }

//     sock_v_set_timeout( sock, 1 );

//     log_v_info_P( PSTR("SuperConductor is connected") );

//     send_init_msg();
//     timer = tmr_u32_get_system_time_ms();

// 	while( TRUE ){

// 		THREAD_WAIT_WHILE( pt, sock_i8_recvfrom( sock ) < 0 );

// 		// check if service is gone
// 		if( !services_b_is_available( __KV__SuperConductor, 0 ) ){

// 			goto cleanup;
// 		}

//         if( tmr_u32_elapsed_time_ms( timer ) > SC_SYNC_INTERVAL ){

//         	send_init_msg();
//         	timer = tmr_u32_get_system_time_ms();
//         }

//         if( sock_i16_get_bytes_read( sock ) <= 0 ){

//             continue;
//         }

//         sc_msg_hdr_t *header = sock_vp_get_data( sock );

//         if( header->magic != SC_MAGIC ){

//             continue;
//         }
		
// 		if( header->msg_type == SC_MSG_TYPE_BANK ){

// 			sc_msg_bank_t *msg = (sc_msg_bank_t *)header;

// 			// look up matching bank
// 			bool found = FALSE;
// 			for( uint8_t i = 0; i < SC_MAX_BANKS; i++ ){

// 				if( strncmp( msg->bank.str, banks[i].str, CATBUS_STRING_LEN ) == 0 ){

// 					found = TRUE;
// 					break;
// 				}
// 			}		

// 			if( !found ){

// 				continue;
// 			}

//             uint16_t data_len = sock_i16_get_bytes_read( sock ) - ( sizeof(sc_msg_bank_t) - 1 );

//             uint32_t hash = hash_u32_string( msg->bank.str );

//             // add. and set if already added:
//             int8_t status = kvdb_i8_add( 
//                                 hash, 
//                                 msg->data.meta.type, 
//                                 (uint16_t)msg->data.meta.count + 1, 
//                                 &msg->data.data, 
//                                 data_len
//                             );

//             if( status != KVDB_STATUS_OK ){

//                 log_v_error_P( PSTR("kvdb failed to set: %d %s 0x%0x len: %d"), status, msg->bank.str, hash, data_len );
//             }
// 		}        
// 	}


// cleanup:
	
// 	sock_v_release( sock );

// 	sock = -1;

// 	THREAD_RESTART( pt );

PT_END( pt );
}

void sc_v_start( void ){

	thread_t_create( superconductor_thread,
                PSTR("superconductor_rx"),
                0,
                0 );
}

void sc_v_stop( void ){

	
}

void sc_v_init( void ){

    sc_v_start();	
}

