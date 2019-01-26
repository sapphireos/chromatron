// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "threading.h"
#include "logging.h"
#include "timers.h"
#include "list.h"
#include "keyvalue.h"
#include "fs.h"

#include "vm_core.h"
#include "vm_cron.h"


static list_t cron_list;

void vm_cron_v_init( void ){

	list_v_init( &cron_list );
}

void vm_cron_v_load( uint8_t vm_id, vm_state_t *state, file_t f ){

	// make sure this vm's cron jobs are unloaded first
	vm_cron_v_unload( vm_id );

	fs_v_seek( f, sizeof(uint32_t) + state->cron_start );

	cron_job_t cron_job;
	cron_job.vm_id = vm_id;

	for( uint8_t i = 0; i < state->cron_count; i++ ){

		fs_i16_read( f, (uint8_t *)&cron_job.cron, sizeof(cron_job.cron) );

		list_node_t ln = list_ln_create_node2( &cron_job, sizeof(cron_job), MEM_TYPE_CRON_JOB );

		if( ln < 0 ){

			return;
		}

		list_v_insert_tail( &cron_list, ln );
	}
}

void vm_cron_v_unload( uint8_t vm_id ){

  	list_node_t ln = cron_list.head;
  	list_node_t next_ln;

    while( ln > 0 ){

    	next_ln = list_ln_next( ln );

        cron_job_t *entry = list_vp_get_data( ln );

        if( entry->vm_id == vm_id ){

            list_v_remove( &cron_list, ln );
            list_v_release_node( ln );
        }

        ln = next_ln;
    }   
}








