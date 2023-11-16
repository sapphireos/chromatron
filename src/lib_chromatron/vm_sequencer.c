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

#include "vm.h"

#include "vm_sequencer.h"

static uint8_t seq_time_mode;
static uint8_t seq_select_mode;

static uint8_t seq_current_step;

static bool seq_running;

static uint16_t seq_interval_time;
static uint16_t seq_random_time_min;
static uint16_t seq_random_time_max;

static uint16_t seq_time_remaining;

static bool seq_trigger;


#define N_SLOTS 8


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

	{ CATBUS_TYPE_BOOL,     0, 0, 					&seq_trigger,     	   0,                  "seq_trigger" },

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


static int8_t get_program_for_slot( uint8_t slot, char progname[FFS_FILENAME_LEN] ){

	ASSERT( progname != 0 );

	memset( progname, 0, FFS_FILENAME_LEN );

	if( seq_current_step == 0 ){

		kv_i8_get( __KV__seq_slot_0, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 1 ){

		kv_i8_get( __KV__seq_slot_1, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 2 ){

		kv_i8_get( __KV__seq_slot_2, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 3 ){

		kv_i8_get( __KV__seq_slot_3, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 4 ){

		kv_i8_get( __KV__seq_slot_4, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 5 ){

		kv_i8_get( __KV__seq_slot_5, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 6 ){

		kv_i8_get( __KV__seq_slot_6, progname, FFS_FILENAME_LEN );
	}
	else if( seq_current_step == 7 ){

		kv_i8_get( __KV__seq_slot_7, progname, FFS_FILENAME_LEN );
	}
	else{

		log_v_error_P( PSTR("Invalid sequencer step!") );

		return -1;
	}

	return 0;
}

static int8_t _run_step( void ){

	if( seq_select_mode == VM_SEQ_SELECT_MODE_NEXT ){

		seq_current_step++;
	}
	else if( seq_select_mode == VM_SEQ_SELECT_MODE_RANDOM ){

		seq_current_step = rnd_u16_range( N_SLOTS );
	}
	else{

		log_v_error_P( PSTR("Invalid select mode!") );

		return -1;
	}

	seq_current_step %= N_SLOTS;

	char progname[FFS_FILENAME_LEN];
	memset( progname, 0, sizeof(progname) );

	if( seq_current_step == 0 ){

		kv_i8_get( __KV__seq_slot_0, progname, sizeof(progname) );
	}
	else if( seq_current_step == 1 ){

		kv_i8_get( __KV__seq_slot_1, progname, sizeof(progname) );
	}
	else if( seq_current_step == 2 ){

		kv_i8_get( __KV__seq_slot_2, progname, sizeof(progname) );
	}
	else if( seq_current_step == 3 ){

		kv_i8_get( __KV__seq_slot_3, progname, sizeof(progname) );
	}
	else if( seq_current_step == 4 ){

		kv_i8_get( __KV__seq_slot_4, progname, sizeof(progname) );
	}
	else if( seq_current_step == 5 ){

		kv_i8_get( __KV__seq_slot_5, progname, sizeof(progname) );
	}
	else if( seq_current_step == 6 ){

		kv_i8_get( __KV__seq_slot_6, progname, sizeof(progname) );
	}
	else if( seq_current_step == 7 ){

		kv_i8_get( __KV__seq_slot_7, progname, sizeof(progname) );
	}
	else{

		log_v_error_P( PSTR("Invalid sequencer step!") );

		return -2;
	}

	// if progname is empty:
	if( progname[0] == 0 ){

		return -3;
	}

	// check that program exists on filesystem:
	if( !fs_b_exists_id( fs_i8_get_file_id( progname ) ) ){

		return -4;
	}

	// in theory the program will run

	vm_v_run_prog( progname, 0 ); // run new program on slot 0

	return 0;
}


static void run_step(void){

	// only try so many times to get a valid program
	// this is how we deal with not filling up all of the slots,
	// if it selects an empty slot it just runs the stepping algorithm
	// again until it finds one (or gives up)
	uint8_t tries = 32;

	while(tries > 0){

		if(_run_step() == 0){

			return;
		}

		tries--;
	}
}


PT_THREAD( vm_sequencer_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
	
	while(1){

		TMR_WAIT( pt, 100 );

		seq_running = FALSE;

		THREAD_WAIT_WHILE( pt, seq_time_mode == VM_SEQ_TIME_MODE_STOPPED );

		seq_running = TRUE;

		if( seq_time_mode == VM_SEQ_TIME_MODE_INTERVAL ){

			seq_time_remaining = seq_interval_time;
	    }
	    else if( seq_time_mode == VM_SEQ_TIME_MODE_RANDOM ){

	    	uint16_t temp = seq_random_time_max - seq_random_time_min;

	    	seq_time_remaining = seq_random_time_min + rnd_u16_range( temp );
	    }	
	   	else if( seq_time_mode == VM_SEQ_TIME_MODE_MANUAL ){

	    	THREAD_WAIT_WHILE( pt, 
	    		( seq_time_mode == VM_SEQ_TIME_MODE_MANUAL ) &&
	    		( seq_trigger == FALSE ) );

			if( seq_time_mode != VM_SEQ_TIME_MODE_MANUAL ){

				seq_trigger = FALSE;

				continue;
			}	

			if( seq_trigger ){

				seq_trigger = FALSE;

				run_step();
			}

			continue;
	    }
	    else{

			log_v_error_P( PSTR("Invalid time mode!") );	    	
	    }

	    thread_v_set_alarm( tmr_u32_get_system_time_ms() + 1000 );

	    while( ( ( seq_time_mode == VM_SEQ_TIME_MODE_INTERVAL ) ||
	    	     ( seq_time_mode == VM_SEQ_TIME_MODE_RANDOM ) ) &&
			   ( seq_time_remaining > 0 ) ){

			thread_v_set_alarm( thread_u32_get_alarm() + 1000 );
	        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );

	        seq_time_remaining--;

	        if( seq_time_remaining == 0 ){

	        	run_step();
	        }
		}
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


