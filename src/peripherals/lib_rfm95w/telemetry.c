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

#include "telemetry.h"
#include "rf_mac.h"

#ifdef ESP32

static bool telemetry_enable;
static bool telemetry_station_enable;

KV_SECTION_META kv_meta_t telemetry_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &telemetry_enable,           0,   "telemetry_enable" },
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &telemetry_station_enable,   0,   "telemetry_station_enable" },
};


static list_t remote_stations_list;

static int16_t base_rssi;
static int16_t base_snr;

// PT_THREAD( telemetry_rx_thread( pt_t *pt, void *state ) );
// PT_THREAD( telemetry_tx_thread( pt_t *pt, void *state ) );
PT_THREAD( telemetry_remote_station_rx_thread( pt_t *pt, void *state ) );
PT_THREAD( telemetry_remote_station_tx_thread( pt_t *pt, void *state ) );

PT_THREAD( telemetry_beacon_thread( pt_t *pt, void *state ) );
PT_THREAD( telemetry_base_station_rx_thread( pt_t *pt, void *state ) );
PT_THREAD( telemetry_base_station_tx_thread( pt_t *pt, void *state ) );

void telemetry_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    list_v_init( &remote_stations_list );

    if( !telemetry_enable ){

        return;
    }

    if( rf_mac_i8_init() < 0 ){

        return;
    }
    
    if( telemetry_station_enable ){

        thread_t_create( telemetry_base_station_tx_thread,
                     PSTR("telemetry_base_station_tx"),
                     0,
                     0 );

        thread_t_create( telemetry_base_station_rx_thread,
                     PSTR("telemetry_base_station_rx"),
                     0,
                     0 );

       thread_t_create( telemetry_beacon_thread,
                 PSTR("telemetry_beacon"),
                 0,
                 0 );
    }  
    else{

        thread_t_create( telemetry_remote_station_rx_thread,
             PSTR("telemetry_remote_station_rx"),
             0,
             0 );

        thread_t_create( telemetry_remote_station_tx_thread,
             PSTR("telemetry_remote_station_tx"),
             0,
             0 );

        // thread_t_create( telemetry_rx_thread,
        //              PSTR("telemetry_rx"),
        //              0,
        //              0 );

        // thread_t_create( telemetry_tx_thread,
        //              PSTR("telemetry_tx"),
        //              0,
        //              0 );
    }
}


// typedef struct __attribute__((packed)){
//     uint32_t sys_time;
//     uint64_t src_addr;
//     int16_t rssi;
//     int16_t snr;
//     telemetry_msg_0_t msg;
// } telemtry_log_0_t;


// static file_t log_f;

// PT_THREAD( telemetry_rx_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

//     log_f = fs_f_open_P( PSTR("telemetry_log"), FS_MODE_CREATE_IF_NOT_FOUND | FS_MODE_WRITE_APPEND );

//     if( log_f < 0 ){

//         THREAD_EXIT( pt );
//     }

//     while( 1 ){

//         THREAD_WAIT_WHILE( pt, !rf_mac_b_rx_available() );

//         rf_mac_rx_pkt_t pkt;
//         uint8_t buf[RFM95W_FIFO_LEN];

//         rf_mac_i8_get_rx( &pkt, buf, sizeof(buf) );

//         rf_mac_header_0_t *header =(rf_mac_header_0_t *)buf;
//         uint32_t *data = (uint32_t *)&buf[sizeof(rf_mac_header_0_t)];

//         telemtry_log_0_t log_data;

//         log_data.sys_time = tmr_u32_get_system_time_ms();
//         log_data.src_addr = header->src_addr;
//         log_data.rssi = pkt.rssi;
//         log_data.snr = pkt.snr;
//         log_data.msg = *(telemetry_msg_0_t *)data;

//         fs_i16_write( log_f, &log_data, sizeof(log_data) );

//         // log_v_debug_P( PSTR("received %u %d bytes rssi: %d snr: %d"), *data, pkt.len, pkt.rssi, pkt.snr );
//     }

// PT_END( pt );
// }

// PT_THREAD( telemetry_tx_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
    
//     static uint16_t sample;
//     sample = 0;

//     while( 1 ){

//         TMR_WAIT( pt, 60000 * 5 + rnd_u16_get_int() );       

//         telemetry_msg_0_t msg;
//         msg.sample = sample;
//         sample++;

//         catbus_i8_get( __KV__batt_volts,            CATBUS_TYPE_UINT16, &msg.batt_volts );
//         catbus_i8_get( __KV__batt_charge_current,   CATBUS_TYPE_UINT16, &msg.charge_current );
//         catbus_i8_get( __KV__veml7700_filtered_als, CATBUS_TYPE_UINT32, &msg.als );
//         catbus_i8_get( __KV__batt_temp,             CATBUS_TYPE_INT8,   &msg.batt_temp );
//         catbus_i8_get( __KV__batt_case_temp,        CATBUS_TYPE_INT8,   &msg.case_temp );
//         catbus_i8_get( __KV__batt_ambient_temp,     CATBUS_TYPE_INT8,   &msg.ambient_temp );
//         catbus_i8_get( __KV__batt_fault,            CATBUS_TYPE_INT8,   &msg.batt_fault );

//         telemtry_log_0_t log_data;

//         log_data.sys_time = tmr_u32_get_system_time_ms();
//         log_data.src_addr = 0; // local node
//         log_data.rssi = 0;
//         log_data.snr = 0;
//         log_data.msg = msg;

//         fs_i16_write( log_f, &log_data, sizeof(log_data) );


//         rf_mac_i8_send( 0, (uint8_t *)&msg, sizeof(msg) );

//     }

// PT_END( pt );
// }


static uint32_t beacons_received;

KV_SECTION_OPT kv_meta_t telemetry_remote_opt[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &beacons_received,           0,   "telemetry_beacons_received" },
    { CATBUS_TYPE_INT16,  0, KV_FLAGS_READ_ONLY,  &base_rssi,                  0,   "telemetry_base_rssi" },
    { CATBUS_TYPE_INT16,  0, KV_FLAGS_READ_ONLY,  &base_snr,                   0,   "telemetry_base_snr" },
};


PT_THREAD( telemetry_remote_station_rx_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    kv_v_add_db_info( telemetry_remote_opt, sizeof(telemetry_remote_opt) );

    while( 1 ){

        THREAD_WAIT_WHILE( pt, !rf_mac_b_rx_available() );

        rf_mac_rx_pkt_t pkt;
        uint8_t buf[RFM95W_FIFO_LEN];

        if( rf_mac_i8_get_rx( &pkt, buf, sizeof(buf) ) < 0 ){

            continue;
        }

        // rf_mac_header_0_t *header =(rf_mac_header_0_t *)buf;
        uint8_t *flags = (uint8_t *)&buf[sizeof(rf_mac_header_0_t)];

        // check for beacon
        if( *flags & TELEMETRY_FLAGS_BEACON ){

            beacons_received++;

            base_rssi = pkt.rssi;
            base_snr = pkt.snr;
        }
    }

PT_END( pt );
}

PT_THREAD( telemetry_remote_station_tx_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint32_t sample;
    sample = 0;

    while( 1 ){

        TMR_WAIT( pt, 16000 );

        // don't transmit if there are no beacons
        if( beacons_received == 0 ){

            continue;
        }

        telemetry_msg_remote_data_0_t msg = {0};
        msg.flags = TELEMETRY_FLAGS_REMOTE;
        msg.sample = sample;
        sample++;

        catbus_i8_get( __KV__batt_volts,            CATBUS_TYPE_UINT16, &msg.batt_volts );
        catbus_i8_get( __KV__batt_charge_current,   CATBUS_TYPE_UINT16, &msg.charge_current );
        catbus_i8_get( __KV__veml7700_filtered_als, CATBUS_TYPE_UINT32, &msg.als );
        catbus_i8_get( __KV__batt_temp,             CATBUS_TYPE_INT8,   &msg.batt_temp );
        catbus_i8_get( __KV__batt_case_temp,        CATBUS_TYPE_INT8,   &msg.case_temp );
        catbus_i8_get( __KV__batt_ambient_temp,     CATBUS_TYPE_INT8,   &msg.ambient_temp );
        catbus_i8_get( __KV__batt_fault,            CATBUS_TYPE_INT8,   &msg.batt_fault );

        msg.base_rssi = base_rssi;
        msg.base_snr = base_snr;

        rf_mac_i8_send( 0, (uint8_t *)&msg, sizeof(msg) );
    }

PT_END( pt );
}



static uint32_t telemetry_data_vfile_handler( vfile_op_t8 op, uint32_t pos, void *ptr, uint32_t len ){

    // the pos and len values are already bounds checked by the FS driver
    switch( op ){

        case FS_VFILE_OP_READ:
            len = list_u16_flatten( &remote_stations_list, pos, ptr, len );
            break;

        case FS_VFILE_OP_SIZE:
            len = list_u16_size( &remote_stations_list );
            break;

        default:
            len = 0;

            break;
    }

    return len;
}


static void transmit_beacon( void ){

    telemetry_msg_beacon_t msg = {
        TELEMETRY_FLAGS_BEACON,
        0
    };

    rf_mac_i8_send( 0, (uint8_t *)&msg, sizeof(msg) );
}


PT_THREAD( telemetry_beacon_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while( 1 ){

        TMR_WAIT( pt, 16000 );
        
        transmit_beacon();
    }

PT_END( pt );
}


void load_telemetry_config( void ){

    file_t f = fs_f_open_P( PSTR("telemetry_config"), FS_MODE_READ_ONLY );

    if( f < 0 ){

        // reset_config();

        goto done;
    }



done:
    
    if( f > 0 ){

        f = fs_f_close( f );
    }
}

PT_THREAD( telemetry_base_station_tx_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // kv_v_add_db_info( telemetry_basestation_opt, sizeof(telemetry_basestation_opt) );

    while( 1 ){

        TMR_WAIT( pt, 1000 );


    }

PT_END( pt );
}


telemetry_data_entry_t* search_remotes( uint64_t src_addr ){

    list_node_t ln = remote_stations_list.head;
    list_node_t next_ln;

    while( ln > 0 ){

        next_ln = list_ln_next( ln );

        telemetry_data_entry_t *entry = list_vp_get_data( ln );

        if( entry->src_addr == src_addr ){

            return entry;
        }

        ln = next_ln;
    }   

    return 0;
}

PT_THREAD( telemetry_base_station_rx_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    fs_f_create_virtual( PSTR("telemetry_data"), telemetry_data_vfile_handler );

    while( 1 ){

        THREAD_WAIT_WHILE( pt, !rf_mac_b_rx_available() );

        rf_mac_rx_pkt_t pkt;
        uint8_t buf[RFM95W_FIFO_LEN];

        if( rf_mac_i8_get_rx( &pkt, buf, sizeof(buf) ) < 0 ){

            continue;
        }

        // rf_mac_header_0_t *header =(rf_mac_header_0_t *)buf;
        uint8_t *flags = (uint8_t *)&buf[sizeof(rf_mac_header_0_t)];

        // check for remote data
        if( *flags & TELEMETRY_FLAGS_REMOTE ){

            telemetry_msg_remote_data_0_t *msg = (telemetry_msg_remote_data_0_t *)flags;

            log_v_debug_P( PSTR("rx remote: 0x%lx"), pkt.src_addr );

            telemetry_data_entry_t *entry = search_remotes( pkt.src_addr );

            if( entry == 0 ){

                list_node_t ln = list_ln_create_node( 0, sizeof(telemetry_data_entry_t) );

                if( ln < 0 ){

                    THREAD_EXIT( pt );
                }

                list_v_insert_tail( &remote_stations_list, ln );

                entry = list_vp_get_data( ln );
            }

            entry->src_addr = pkt.src_addr;
            entry->rssi = pkt.rssi;
            entry->snr = pkt.snr;
            entry->msg = *msg;
        }
    }

PT_END( pt );
}





#else

void telemetry_v_init( void ){


}


#endif