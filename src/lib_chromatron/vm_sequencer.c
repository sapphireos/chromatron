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

#include "vm_sequencer.h"

static uint8_t seq_time_mode;
static uint8_t seq_select_mode;

static uint8_t seq_current_step;

static bool seq_running;

static uint16_t seq_interval_time;
static uint16_t seq_random_time_min;
static uint16_t seq_random_time_max;

static uint16_t seq_time_remaining;

KV_SECTION_META kv_meta_t vm_seq_info_kv[] = {

	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_0" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_1" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_2" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_3" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_4" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_5" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_6" },
	{ CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "seq_slot_7" },

	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_PERSIST,  	&seq_time_mode,        0,                  "seq_time_mode" },
	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_PERSIST,  	&seq_select_mode,      0,                  "seq_select_mode" },

	{ CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY, 	&seq_current_step,     0,                  "seq_current_step" },
	{ CATBUS_TYPE_BOOL,     0, KV_FLAGS_READ_ONLY, 	&seq_running,     	   0,                  "seq_running" },

	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST, 	&seq_interval_time,    0,                  "seq_interval_time" },
	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST, 	&seq_random_time_min,  0,                  "seq_random_time_min" },
	{ CATBUS_TYPE_UINT16,   0, KV_FLAGS_PERSIST, 	&seq_random_time_max,  0,                  "seq_random_time_max" },

	{ CATBUS_TYPE_UINT16,   0, 0, 					&seq_time_remaining,   0,                  "seq_time_remaining" },

    // { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[0],          0,                  "vm_reset" },
    // { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[0],            0,                  "vm_run" },
    // { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog" },
    // { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[0],         0,                  "vm_status" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[0],       0,                  "vm_run_time" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[0],     0,                  "vm_peak_cycles" },
    // { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  &vm_timing_status,     0,                  "vm_timing" },

    // #if VM_MAX_VMS >= 2
    // { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[1],          0,                  "vm_reset_1" },
    // { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[1],            0,                  "vm_run_1" },
    // { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_1" },
    // { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[1],         0,                  "vm_status_1" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[1],       0,                  "vm_run_time_1" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[1],     0,                  "vm_peak_cycles_1" },
    // #endif

    // #if VM_MAX_VMS >= 3
    // { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[2],          0,                  "vm_reset_2" },
    // { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[2],            0,                  "vm_run_2" },
    // { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_2" },
    // { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[2],         0,                  "vm_status_2" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[2],       0,                  "vm_run_time_2" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[2],     0,                  "vm_peak_cycles_2" },
    // #endif

    // #if VM_MAX_VMS >= 4
    // { CATBUS_TYPE_BOOL,     0, 0,                   &vm_reset[3],          0,                  "vm_reset_3" },
    // { CATBUS_TYPE_BOOL,     0, KV_FLAGS_PERSIST,    &vm_run[3],            0,                  "vm_run_3" },
    // { CATBUS_TYPE_STRING32, 0, KV_FLAGS_PERSIST,    0,                     0,                  "vm_prog_3" },
    // { CATBUS_TYPE_INT8,     0, KV_FLAGS_READ_ONLY,  &vm_status[3],         0,                  "vm_status_3" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_run_time[3],       0,                  "vm_run_time_3" },
    // { CATBUS_TYPE_UINT16,   0, KV_FLAGS_READ_ONLY,  &vm_max_cycles[3],     0,                  "vm_peak_cycles_3" },
    // #endif

    // { CATBUS_TYPE_UINT8,    0, KV_FLAGS_READ_ONLY,  0,                     vm_i8_kv_handler,   "vm_isa" },
};

static void run_step( void ){


}


PT_THREAD( vm_sequencer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	THREAD_WAIT_WHILE( pt, seq_time_mode != VM_SEQ_TIME_MODE_STOPPED );

    if( seq_time_mode == VM_SEQ_TIME_MODE_INTERVAL ){

    	seq_time_remaining = seq_interval_time;
    }

	
	while(1){

        thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

        if( seq_time_remaining == 0 ){

        	THREAD_RESTART( pt );
        }

        seq_time_remaining--;

        if( seq_time_remaining == 0 ){

        	// time is up
        	run_step();
        }

        // while( seq_time_mode != VM_SEQ_TIME_MODE_STOPPED ){

        	
		// }
	}
	
	
PT_END( pt );
}

void vm_seq_v_init( void ){

	if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return;
    }

    thread_t_create( vm_sequencer_thread,
                 PSTR("vm_sequencer"),
                 0,
                 0 );

}


