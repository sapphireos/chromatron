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

#ifndef _VM_CRON_H_
#define _VM_CRON_H_

#include "vm_core.h"

typedef struct{
	cron_t cron;
	uint8_t vm_id;
} cron_job_t;

void vm_cron_v_init( void );
void vm_cron_v_load_job( uint8_t vm_id, cron_t *job );
void vm_cron_v_start_jobs( uint8_t vm_id );
void vm_cron_v_unload( uint8_t vm_id );

// function must be defined by vm.c
int8_t vm_cron_i8_run_func( uint8_t i, uint16_t func_addr );

#endif
