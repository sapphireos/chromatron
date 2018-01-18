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

#include "sapphire.h"

#include "vm_core.h"

#include "hash.h"
#include "automaton.h"
#include "catbus.h"
#include "kvdb.h"

static int8_t automaton_status;
static bool automaton_enable;
static int32_t trigger = 0;

static mem_handle_t trigger_index_handle = -1;
static file_t f = -1;

KV_SECTION_META kv_meta_t automaton_info_kv[] = {
    { SAPPHIRE_TYPE_INT8,      0, 0,                   &automaton_status,    0,   "automaton_status" },
    { SAPPHIRE_TYPE_BOOL,      0, 0,                   &automaton_enable,    0,   "automaton_enable" },
};


PT_THREAD( automaton_thread( pt_t *pt, void *state ) );
PT_THREAD( automaton_clock_thread( pt_t *pt, void *state ) );

void auto_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( automaton_thread,
                 PSTR("automaton"),
                 0,
                 0 );

    thread_t_create( automaton_clock_thread,
                 PSTR("automaton_clock"),
                 0,
                 0 );
}

// stores trigger index in the code section
int8_t _auto_i8_load_trigger_index( void ){

    if( trigger_index_handle > 0 ){

        mem2_v_free( trigger_index_handle );
        trigger_index_handle = -1;
    }

    if( f < 0 ){

        return -1;
    }

    fs_v_seek( f, 0 );

    int8_t status = -1;

    automaton_file_t header;

    if( fs_i16_read( f, (uint8_t *)&header, sizeof(header) ) < 0 ){

        goto end;
    }

    // verify magic number
    if( header.magic != AUTOMATON_FILE_MAGIC ){

        status = -1;

        goto end;
    }

    // skip to trigger index
    uint32_t pos = sizeof(header) + 
                   ( header.var_len * sizeof(automaton_var_t) ) +
                   ( header.send_len * sizeof(automaton_link_t) ) +
                   ( header.recv_len * sizeof(automaton_link_t) );

    fs_v_seek( f, pos );

    uint32_t index_size = header.trigger_index_len * sizeof(automaton_trigger_index_t);

    // allocate memory
    trigger_index_handle = mem2_h_alloc( index_size );

    if( trigger_index_handle < 0 ){

        status = -2;
        goto end;
    }

    uint8_t *ptr = mem2_vp_get_ptr( trigger_index_handle );

    if( fs_i16_read( f, ptr, index_size ) < 0 ){

        status = -3;
        goto end;
    }
    
    status = 0;

end:
    if( status < 0 ){

        log_v_error_P( PSTR("error: %d"), status );
    }

    return status;
}


int8_t _auto_i8_load_file( void ){

    // delete existing database entries
    kvdb_v_delete_tag( AUTOMATON_KV_TAG );


    f = fs_f_open_P( PSTR("automaton.auto"), FS_MODE_READ_ONLY );

    if( f < 0 ){

        return -1;
    }

    int8_t status = -1;

    automaton_file_t header;

    if( fs_i16_read( f, (uint8_t *)&header, sizeof(header) ) < 0 ){

        goto end;
    }

    // verify magic number
    if( header.magic != AUTOMATON_FILE_MAGIC ){

        status = -2;
        log_v_error_P( PSTR("bad magic") );

        goto end;
    }   

    // verify version
    if( header.version != AUTOMATON_VERSION ){

        status = -3;
        log_v_error_P( PSTR("wrong version") );

        goto end;
    }

    if( header.var_len > AUTOMATON_MAX_VARS ){

        status = -4;
        log_v_error_P( PSTR("too many vars") );

        goto end;
    }

    // read KV vars
    for( uint8_t i = 0; i < header.var_len; i++ ){

        automaton_var_t kv;

        // read data
        if( fs_i16_read( f, (uint8_t *)&kv, sizeof(kv) ) < 0 ){

            status = -5;
            goto end;
        }

        kvdb_i8_add( kv.hash, 0, AUTOMATON_KV_TAG, kv.name );
    }    

    // purge all locally created links
    // NOTE:
    // right now, we are not flagging links from the automaton,
    // so this will also destroy any app-created links!!!
    catbus_v_purge_links();

    // process sends
    for( uint8_t i = 0; i < header.send_len; i++ ){

        automaton_link_t link;

        // read data
        if( fs_i16_read( f, (uint8_t *)&link, sizeof(link) ) < 0 ){

            status = -6;
            goto end;
        }

        catbus_l_send( link.source_hash, link.dest_hash, &link.query );
    }

    // process receives
    for( uint8_t i = 0; i < header.recv_len; i++ ){

        automaton_link_t link;

        // read data
        if( fs_i16_read( f, (uint8_t *)&link, sizeof(link) ) < 0 ){

            status = -7;
            goto end;
        }

        catbus_l_recv( link.dest_hash, link.source_hash, &link.query );
    }

    // load OK

    status = 0;

    if( _auto_i8_load_trigger_index() < 0 ){

        status = -8;
        goto end;
    }

end:
    if( status < 0 ){

        log_v_error_P( PSTR("error: %d"), status );
    }

    return status;
}

// static uint64_t elapsed;
// elapsed = tmr_u64_get_system_time_us();
// elapsed = tmr_u64_elapsed_time_us( elapsed );


int8_t _auto_i8_process_rule( uint16_t index ){

    if( f < 0 ){

        return -1;
    }

    fs_v_seek( f, 0 );

    int8_t status = -1;

    automaton_file_t header;

    if( fs_i16_read( f, (uint8_t *)&header, sizeof(header) ) < 0 ){

        goto end;
    }

    // verify magic number
    if( header.magic != AUTOMATON_FILE_MAGIC ){

        status = -1;

        goto end;
    }

    int32_t registers[AUTOMATON_REG_COUNT];
    uint8_t code[AUTOMATON_CODE_LEN];
    memset( code, 0xff, sizeof(code) );

    // skip to condition index
    uint32_t pos = sizeof(header) + 
                   ( header.var_len * sizeof(automaton_var_t) ) +
                   ( header.send_len * sizeof(automaton_link_t) ) +
                   ( header.recv_len * sizeof(automaton_link_t) ) +
                   ( header.trigger_index_len * sizeof(automaton_trigger_index_t) ) +
                   index;

    fs_v_seek( f, pos );

    // read magic to verify we got to the right place
    automaton_rule_t rule;
    fs_i16_read( f, (uint8_t *)&rule, sizeof(rule) );

    if( rule.magic != AUTOMATON_RULE_MAGIC ){

        status = -2;
        goto end;
    }

    
    // CONDITION

    // load registers
    memset( registers, 0, sizeof(registers) );

    if( rule.condition_data_len > cnt_of_array(registers) ){

        status = -3;
        goto end;
    }

    if( fs_i16_read( f, (uint8_t *)registers, ( rule.condition_data_len * sizeof(int32_t) ) ) < 0 ){

        status = -4;
        goto end;
    }

    // load KV
    for( uint8_t i = 0; i < rule.condition_kv_len; i++ ){

        automaton_kv_load_t kv_load;
        if( fs_i16_read( f, (uint8_t *)&kv_load, sizeof(kv_load) ) < 0 ){

            status = -5;
            goto end;
        }

        if( kv_load.addr >= cnt_of_array(registers) ){

            status = -6;
            goto end;      
        }

        if( catbus_i8_get( kv_load.hash, &registers[kv_load.addr] ) < 0 ){

            // status = -7;
            // goto end;
            registers[kv_load.addr] = 0;
        }
    }
    
    // load code
    if( rule.condition_code_len > sizeof(code) ){

        status = -8;
        goto end;
    }

    if( fs_i16_read( f, code, rule.condition_code_len ) < 0 ){

        status = -9;
        goto end;
    }

    int32_t result = 0;
    int8_t vm_status = vm_i8_eval( code, registers, &result );

    // log_v_debug_P( PSTR("Condition status: %d result: %ld"), vm_status, result );

    if( result == FALSE ){

        status = 1;
        goto end;
    }

    // ACTION

    // load registers
    memset( registers, 0, sizeof(registers) );

    if( rule.action_data_len > cnt_of_array(registers) ){

        status = -20;
        goto end;
    }

    if( fs_i16_read( f, (uint8_t *)registers, ( rule.action_data_len * sizeof(int32_t) ) ) < 0 ){

        status = -21;
        goto end;
    }

    // load KV
    // remember our file position, we'll come back to this
    int32_t kv_load_pos = fs_i32_tell( f );

    for( uint8_t i = 0; i < rule.action_kv_len; i++ ){

        automaton_kv_load_t kv_load;
        if( fs_i16_read( f, (uint8_t *)&kv_load, sizeof(kv_load) ) < 0 ){

            status = -22;
            goto end;
        }

        if( kv_load.addr >= cnt_of_array(registers) ){

            status = -23;
            goto end;      
        }

        if( catbus_i8_get( kv_load.hash, &registers[kv_load.addr] ) < 0 ){

            // status = -24;
            registers[kv_load.addr] = 0;
            // goto end;
        }
    }

    // load code
    if( rule.action_code_len > sizeof(code) ){

        status = -25;
        goto end;
    }

    if( fs_i16_read( f, code, rule.action_code_len ) < 0 ){

        status = -26;
        goto end;
    }

    result = 0;
    vm_status = vm_i8_eval( code, registers, &result );

    // write KV to database
    fs_v_seek( f, kv_load_pos );

    for( uint8_t i = 0; i < rule.action_kv_len; i++ ){

        automaton_kv_load_t kv_load;
        if( fs_i16_read( f, (uint8_t *)&kv_load, sizeof(kv_load) ) < 0 ){

            status = -27;
            goto end;
        }

        if( kv_load.addr >= cnt_of_array(registers) ){

            status = -28;
            goto end;      
        }

        // check if item changed
        int32_t data = 0;

        if( catbus_i8_get( kv_load.hash, &data ) < 0 ){        

            // status = -29;
            // goto end;
        }
        else{
            if( registers[kv_load.addr] != data ){
                
                if( catbus_i8_set( kv_load.hash, registers[kv_load.addr] ) < 0 ){

                    // status = -30;
                    // goto end;
                }
            }
        }
    }

    // log_v_debug_P( PSTR("Action status: %d result: %ld"), vm_status, result );


    status = 0;

end:
    if( status < 0 ){

        log_v_error_P( PSTR("error: %d"), status );
    }

    return status;
}

void _auto_v_trigger( uint32_t hash ){

    if( trigger_index_handle < 0 ){

        return;
    }

    automaton_trigger_index_t *index = mem2_vp_get_ptr( trigger_index_handle );

    uint8_t count = mem2_u16_get_size( trigger_index_handle ) / sizeof(automaton_trigger_index_t);

    for( uint8_t i = 0; i < count; i++ ){

        if( index[i].hash == hash ){

            index[i].status = 1;

            trigger = 1;
        }
    }
}

PT_THREAD( automaton_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    TMR_WAIT( pt, 2000 );
    
    while(1){

        while( _auto_i8_load_file() < 0 ){

            TMR_WAIT( pt, 2000 );
        }

        log_v_debug_P( PSTR("Automaton ready") );        

        while(1){

            THREAD_WAIT_WHILE( pt, ( trigger == 0 ) && ( fs_i32_get_size( f ) >= 0 ) );

            if( trigger == 0 ){

                // this means our file got deleted
                goto restart;

            }

            // elapsed = tmr_u64_get_system_time_us();

            int8_t status = 0;

            // scan triggers and process rules
            automaton_trigger_index_t *index = mem2_vp_get_ptr( trigger_index_handle );

            uint8_t count = mem2_u16_get_size( trigger_index_handle ) / sizeof(automaton_trigger_index_t);

            for( uint8_t i = 0; i < count; i++ ){

                if( index[i].status != 0 ){

                    index[i].status = 0;

                    status = _auto_i8_process_rule( index[i].condition_offset );

                    if( status < 0 ){

                        break;
                    }
                }
            }

            if( status < 0 ){

                log_v_error_P( PSTR("status: %d"), status );

                goto restart;
            }

            // elapsed = tmr_u64_elapsed_time_us( elapsed );

            // log_v_debug_P( PSTR("elapsed: %lu"), (uint32_t)elapsed );        


            trigger = 0;

            // prevent runaway thread
            TMR_WAIT( pt, 10 );
        }

restart:
        log_v_debug_P( PSTR("Automaton restarting") );        

        // clear file handle
        if( f > 0 ){

            fs_f_close( f );
        }

        if( trigger_index_handle > 0 ){

            mem2_v_free( trigger_index_handle );
            trigger_index_handle = -1;
        }

        f = -1;

        // purge all locally created links
        // NOTE:
        // right now, we are not flagging links from the automaton,
        // so this will also destroy any app-created links!!!
        catbus_v_purge_links();

        TMR_WAIT( pt, 1000 );
    }

PT_END( pt );
}

PT_THREAD( automaton_clock_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, 1000 );

        // for( uint8_t i = 0; i < cnt_of_array(var_type); i++ ){

        //     if( hashes[i] == 0 ){

        //         continue;
        //     }

        //     if( var_type[i] == AUTOMATON_VAR_TYPE_DOWNCOUNTER ){

        //         if( vars[i] > 0 ){

        //             vars[i]--;

        //             kv_i8_publish( hashes[i] );

        //             _auto_v_trigger( hashes[i] );
        //         }
        //     }
        // }    
    }

PT_END( pt );
}


void kv_v_notify_hash_set( uint32_t hash ){

    _auto_v_trigger( hash );    
}
