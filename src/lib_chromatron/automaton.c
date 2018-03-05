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

#ifdef ENABLE_AUTOMATON

#include "vm_core.h"

#include "hash.h"
#include "automaton.h"
#include "catbus.h"
#include "kvdb.h"

static int8_t automaton_status;
static bool automaton_enable;
static uint8_t automaton_seconds;
static bool triggered;

static mem_handle_t trigger_index_handle = -1;
static file_t file_handle = -1;

KV_SECTION_META kv_meta_t automaton_info_kv[] = {
    { SAPPHIRE_TYPE_INT8,      0, 0,                   &automaton_status,    0,   "automaton_status" },
    { SAPPHIRE_TYPE_BOOL,      0, KV_FLAGS_PERSIST,    &automaton_enable,    0,   "automaton_enable" },
    { SAPPHIRE_TYPE_UINT8,     0, 0,                   &automaton_seconds,   0,   "seconds" },
    { SAPPHIRE_TYPE_STRING32,  0, KV_FLAGS_PERSIST,    0,                    0,   "automaton_prog" },
};


PT_THREAD( automaton_runner_thread( pt_t *pt, int32_t *state ) );
PT_THREAD( automaton_thread( pt_t *pt, void *state ) );

void auto_v_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    automaton_status = AUTOMATON_STATUS_STOPPED;

    thread_t_create( automaton_thread,
                 PSTR("automaton"),
                 0,
                 0 );
}

// stores trigger index in the code section
int8_t _auto_i8_load_trigger_index( void ){

    if( trigger_index_handle > 0 ){

        mem2_v_free( trigger_index_handle );
        trigger_index_handle = -1;
    }

    if( file_handle < 0 ){

        return -1;
    }

    fs_v_seek( file_handle, 0 );

    int8_t status = -1;

    automaton_file_t header;

    if( fs_i16_read( file_handle, (uint8_t *)&header, sizeof(header) ) < 0 ){

        goto end;
    }

    // verify magic number
    if( header.magic != AUTOMATON_FILE_MAGIC ){

        status = -1;

        goto end;
    }

    // skip to trigger index
    uint32_t pos = sizeof(header) + 
                   ( header.db_vars_len * sizeof(automaton_db_var_t) ) +
                   ( header.send_len * sizeof(automaton_link_t) ) +
                   ( header.recv_len * sizeof(automaton_link_t) );

    fs_v_seek( file_handle, pos );

    uint32_t index_size = header.trigger_index_len * sizeof(automaton_trigger_index_t);

    // allocate memory
    trigger_index_handle = mem2_h_alloc( index_size );

    if( trigger_index_handle < 0 ){

        status = -2;
        goto end;
    }

    uint8_t *ptr = mem2_vp_get_ptr( trigger_index_handle );

    if( fs_i16_read( file_handle, ptr, index_size ) < 0 ){

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

    char program_fname[KV_NAME_LEN];

    if( kv_i8_get( __KV__automaton_prog, program_fname, sizeof(program_fname) ) < 0 ){

        return -1;
    }

    file_handle = fs_f_open( program_fname, FS_MODE_READ_ONLY );

    if( file_handle < 0 ){

        return AUTOMATON_STATUS_FILE_NOT_FOUND;
    }

    int8_t status = AUTOMATON_STATUS_LOAD_ERROR;

    automaton_file_t header;

    if( fs_i16_read( file_handle, (uint8_t *)&header, sizeof(header) ) < 0 ){

        goto end;
    }

    // verify magic number
    if( header.magic != AUTOMATON_FILE_MAGIC ){

        log_v_error_P( PSTR("bad magic") );

        goto end;
    }   

    // verify version
    if( header.version != AUTOMATON_VERSION ){

        status = AUTOMATON_STATUS_BAD_VERSION;
        log_v_error_P( PSTR("wrong version") );

        goto end;
    }

    if( header.local_vars_len > AUTOMATON_MAX_LOCAL_VARS ){

        status = AUTOMATON_STATUS_DATA_OVERFLOW;
        log_v_error_P( PSTR("too many local vars") );

        goto end;
    }

    if( header.db_vars_len > AUTOMATON_MAX_DB_VARS ){

        status = AUTOMATON_STATUS_DATA_OVERFLOW;
        log_v_error_P( PSTR("too many db vars") );

        goto end;
    }

    // read DB vars
    for( uint8_t i = 0; i < header.db_vars_len; i++ ){

        automaton_db_var_t kv;

        // read data
        if( fs_i16_read( file_handle, (uint8_t *)&kv, sizeof(kv) ) < 0 ){

            goto end;
        }

        if( kvdb_i8_add( kv.hash, CATBUS_TYPE_INT32, 1, 0, 0 ) < 0 ){

            status = AUTOMATON_STATUS_DB_ADD_FAIL;
            goto end;
        }
        kvdb_v_set_tag( kv.hash, AUTOMATON_KV_TAG );
        kvdb_v_set_name( kv.name );
    }    

    // purge all locally created links
    // NOTE:
    // right now, we are not flagging links from the automaton,
    // so this will also destroy any app-created links!!!
    catbus_v_purge_links( __KV__automaton );

    // process sends
    for( uint8_t i = 0; i < header.send_len; i++ ){

        automaton_link_t link;

        // read data
        if( fs_i16_read( file_handle, (uint8_t *)&link, sizeof(link) ) < 0 ){

            goto end;
        }

        catbus_l_send( link.source_hash, link.dest_hash, &link.query, __KV__automaton );
    }

    // process receives
    for( uint8_t i = 0; i < header.recv_len; i++ ){

        automaton_link_t link;

        // read data
        if( fs_i16_read( file_handle, (uint8_t *)&link, sizeof(link) ) < 0 ){

            goto end;
        }

        catbus_l_recv( link.dest_hash, link.source_hash, &link.query, __KV__automaton );
    }

    // load OK

    status = header.local_vars_len;

    if( _auto_i8_load_trigger_index() < 0 ){

        status = AUTOMATON_STATUS_TRIGGER_INDEX_FAIL;
        goto end;
    }

end:
    if( status < 0 ){

        if( file_handle > 0 ){

            file_handle = fs_f_close( file_handle );
        }

        log_v_error_P( PSTR("error: %d"), status );
    }

    return status;
}

// static uint64_t elapsed;
// elapsed = tmr_u64_get_system_time_us();
// elapsed = tmr_u64_elapsed_time_us( elapsed );


void _auto_v_trigger( catbus_hash_t32 hash ){

    if( trigger_index_handle < 0 ){

        return;
    }

    automaton_trigger_index_t *index = mem2_vp_get_ptr( trigger_index_handle );

    uint8_t count = mem2_u16_get_size( trigger_index_handle ) / sizeof(automaton_trigger_index_t);

    for( uint8_t i = 0; i < count; i++ ){

        if( index[i].hash == hash ){

            index[i].status = 1;

            triggered = TRUE;
        }
    }
}


int8_t _auto_i8_process_rule( 
    uint16_t index, 
    automaton_file_t *header, 
    int32_t *registers, 
    uint16_t registers_len,
    int32_t *local_vars, 
    uint16_t local_vars_len ){

    int8_t status = AUTOMATON_STATUS_LOAD_ERROR;

    uint8_t code[AUTOMATON_CODE_LEN];
    memset( code, 0xff, sizeof(code) );

    // skip to condition index
    uint32_t pos = sizeof(automaton_file_t) + 
                   ( header->db_vars_len * sizeof(automaton_db_var_t) ) +
                   ( header->send_len * sizeof(automaton_link_t) ) +
                   ( header->recv_len * sizeof(automaton_link_t) ) +
                   ( header->trigger_index_len * sizeof(automaton_trigger_index_t) ) +
                   index;

    fs_v_seek( file_handle, pos );

    // read magic to verify we got to the right place
    automaton_rule_t rule;
    fs_i16_read( file_handle, (uint8_t *)&rule, sizeof(rule) );

    if( rule.magic != AUTOMATON_RULE_MAGIC ){

        goto end;
    }

    
    // CONDITION

    // init registers
    memset( registers, 0, sizeof(registers) );

    if( rule.condition_data_len > AUTOMATON_REG_COUNT ){

        status = AUTOMATON_STATUS_DATA_OVERFLOW;
        goto end;
    }

    if( fs_i16_read( file_handle, (uint8_t *)registers, ( rule.condition_data_len * sizeof(int32_t) ) ) < 0 ){

        goto end;
    }
    
    // load code
    if( rule.condition_code_len > sizeof(code) ){

        status = AUTOMATON_STATUS_CODE_OVERFLOW;
        goto end;
    }

    if( fs_i16_read( file_handle, code, rule.condition_code_len ) < 0 ){

        goto end;
    }

    // load local vars
    memcpy( &registers[1], local_vars, local_vars_len );

    int32_t result = 0;
    int8_t vm_status = vm_i8_eval( code, registers, &result );

    // log_v_debug_P( PSTR("Condition status: %d result: %ld"), vm_status, result );

    if( result == FALSE ){

        status = 1;
        goto end;
    }

    // ACTION

    // init registers
    memset( registers, 0, sizeof(registers) );

    if( rule.action_data_len > AUTOMATON_REG_COUNT ){

        status = AUTOMATON_STATUS_DATA_OVERFLOW;
        goto end;
    }

    if( fs_i16_read( file_handle, (uint8_t *)registers, ( rule.action_data_len * sizeof(int32_t) ) ) < 0 ){

        goto end;
    }

    // load local vars
    memcpy( &registers[1], local_vars, local_vars_len );

    // load code
    if( rule.action_code_len > sizeof(code) ){

        status = AUTOMATON_STATUS_CODE_OVERFLOW;
        goto end;
    }

    if( fs_i16_read( file_handle, code, rule.action_code_len ) < 0 ){

        goto end;
    }

    result = 0;
    vm_status = vm_i8_eval( code, registers, &result );

    // store local vars
    for( uint8_t i = 0; i < local_vars_len / sizeof(int32_t); i++ ){

        // check for changes
        if( local_vars[i] != registers[i + 1] ){

            // store updated value
            local_vars[i] = registers[i + 1];

            // trigger conditions for this local var
            _auto_v_trigger( i + 1 );
        }
    }
    // memcpy( local_vars, &registers[1], local_vars_len );

    // log_v_debug_P( PSTR("Action status: %d result: %ld"), vm_status, result );

    status = 0;

end:
    if( status < 0 ){

        // log_v_error_P( PSTR("error: %d"), status );
    }

    return status;
}



PT_THREAD( automaton_runner_thread( pt_t *pt, int32_t *state ) )
{
PT_BEGIN( pt );

    ASSERT( file_handle >= 0 );

    while(1){

        THREAD_WAIT_WHILE( pt, ( !triggered ) &&
                               ( fs_i32_get_size( file_handle ) >= 0 ) &&
                               ( automaton_enable ) );

        if( !triggered ){

            automaton_status = AUTOMATON_STATUS_STOPPED;

            // this means our file got deleted, or we disabled the automaton
            goto end;
        }

        // elapsed = tmr_u64_get_system_time_us();

        int8_t status = 0;

        if( file_handle < 0 ){

            goto error;
        }

        fs_v_seek( file_handle, 0 );

        automaton_file_t header;

        if( fs_i16_read( file_handle, (uint8_t *)&header, sizeof(header) ) < 0 ){

            goto error;
        }

        // verify magic number
        if( header.magic != AUTOMATON_FILE_MAGIC ){

            goto error;
        }


        // scan triggers and process rules
        automaton_trigger_index_t *index = mem2_vp_get_ptr( trigger_index_handle );

        uint8_t count = mem2_u16_get_size( trigger_index_handle ) / sizeof(automaton_trigger_index_t);

        #define MAX_PASSES 8
        uint8_t passes = MAX_PASSES;

        while( triggered && ( passes > 0 ) ){

            passes--;
            triggered = FALSE;

            for( uint8_t i = 0; i < count; i++ ){

                if( index[i].status != 0 ){

                    index[i].status = 0;

                    int32_t registers[AUTOMATON_REG_COUNT];

                    status = _auto_i8_process_rule( 
                        index[i].condition_offset, 
                        &header, 
                        registers, 
                        sizeof(registers),
                        state,
                        header.local_vars_len * sizeof(int32_t) );

                    if( status < 0 ){

                        break;
                    }
                }
            }
        }

        // if( ( MAX_PASSES - passes ) > 1 ){

        //     log_v_debug_P( PSTR("Passes: %d"), MAX_PASSES - passes );
        // }

        if( status < 0 ){

            log_v_error_P( PSTR("status: %d"), status );

            goto error;
        }

        // elapsed = tmr_u64_elapsed_time_us( elapsed );

        // log_v_debug_P( PSTR("elapsed: %lu"), (uint32_t)elapsed );        

        // triggered = FALSE;
    }

error:
    automaton_status = AUTOMATON_STATUS_ERROR;

end:
    // delete existing database entries
    kvdb_v_delete_tag( AUTOMATON_KV_TAG );

PT_END( pt );
}



PT_THREAD( automaton_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    TMR_WAIT( pt, 2000 );

    // add tag name to database hash lookup
    kvdb_v_set_name_P( PSTR("automaton") );
    
    while(1){

        THREAD_WAIT_WHILE( pt, !automaton_enable );

        int8_t status = _auto_i8_load_file();

        if( status < 0 ){

            automaton_status = status;
            goto restart;
        }

        automaton_status = AUTOMATON_STATUS_RUNNING;

        // load file returns number of local vars
        uint16_t local_var_len = status * sizeof(int32_t);

        // start runner thread
        thread_t t = thread_t_create( THREAD_CAST(automaton_runner_thread), PSTR("automaton_runner"), 0, local_var_len );
        if( t < 0 ){

            automaton_status = AUTOMATON_STATUS_ERROR;
            goto restart;
        }

        // clear thread state
        memset( thread_vp_get_data( t ), 0, local_var_len );

        while( automaton_status == AUTOMATON_STATUS_RUNNING ){

            TMR_WAIT( pt, 1000 );

            automaton_seconds++;
            _auto_v_trigger( __KV__seconds );
        }

        
restart:

        // log_v_debug_P( PSTR("Automaton restarting") );        

        // clear file handle
        if( file_handle > 0 ){

            file_handle = fs_f_close( file_handle );
        }

        if( trigger_index_handle > 0 ){

            mem2_v_free( trigger_index_handle );
            trigger_index_handle = -1;
        }

        // purge all locally created links
        // NOTE:
        // right now, we are not flagging links from the automaton,
        // so this will also destroy any app-created links!!!
        catbus_v_purge_links( __KV__automaton );

        TMR_WAIT( pt, 2000 );
    }

PT_END( pt );
}


void kv_v_notify_hash_set( uint32_t hash ){

    _auto_v_trigger( hash );    
}

#endif