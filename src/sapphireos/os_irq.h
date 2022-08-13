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


#ifndef _OS_IRQ_H
#define _OS_IRQ_H

#include <inttypes.h>

#define ENABLE_IRQ_TIMING

// public API
bool osirq_b_is_irq( void );
uint32_t osirq_u32_get_irq_addr( void );
uint64_t osirq_u64_get_irq_time( void );
uint32_t osirq_u32_get_last_irq_addr( void );

// DO NOT CALL THESE
void __osirq_v_enter( void (*irq_func)(void) );
void __osirq_v_exit( void );


#define OS_IRQ_BEGIN(vector) __osirq_v_enter(vector)
#define OS_IRQ_END(a) __osirq_v_exit()


#endif

