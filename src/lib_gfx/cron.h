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

#ifndef _CRON_H_
#define _CRON_H_

typedef struct{
	int8_t minutes;
	int8_t hours;
	int8_t day_of_month;
	int8_t month;
	int8_t day_of_week;

	uint16_t func_addr;
	uint8_t vm_id;
} cron_job_t;

void cron_v_init( void );

int8_t cron_i8_parse( char* s, cron_job_t *job );
void cron_v_add_job( char *s, uint16_t func_addr, uint8_t vm_id );
void cron_v_unload( uint8_t vm_id );


#endif
