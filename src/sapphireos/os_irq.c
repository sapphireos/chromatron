// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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


#include "cpu.h"
#include "system.h"
#include "keyvalue.h"
#include "timers.h"

#include "os_irq.h"

static uint16_t current_irq;
static uint16_t last_irq;

#ifdef ENABLE_IRQ_TIMING

static uint32_t irq_start_time;
static uint16_t irq_longest_time;
static uint16_t irq_longest_addr;
static uint16_t irq_longest_time2;
static uint16_t irq_longest_addr2;
static uint64_t irq_time;

KV_SECTION_META kv_meta_t os_irq_info_kv[] = {
    { CATBUS_TYPE_UINT64,  0, KV_FLAGS_READ_ONLY,  &irq_time,          0,  "irq_time" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &irq_longest_time,          0,  "irq_longest_time" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &irq_longest_addr,          0,  "irq_longest_addr" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &irq_longest_time2,         0,  "irq_longest_time2" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &irq_longest_addr2,         0,  "irq_longest_addr2" },
};

#endif

// are we currently running an interrupt?
bool osirq_b_is_irq( void ){

    bool is_irq;

    ATOMIC;

    is_irq = ( current_irq != 0 );

    END_ATOMIC;

    return  is_irq;
}

uint32_t osirq_u32_get_irq_addr( void ){

    uint32_t temp;

    ATOMIC;

    temp = current_irq;

    END_ATOMIC;

    // multiply by 2 to get byte address
    return temp << 1;
}

uint64_t osirq_u64_get_irq_time( void ){

    ATOMIC;

    uint64_t temp = irq_time;

    END_ATOMIC;

    return temp;
}

uint32_t osirq_u32_get_last_irq_addr( void ){

    uint32_t temp;

    ATOMIC;

    temp = last_irq;

    END_ATOMIC;

    // multiply by 2 to get byte address
    return temp << 1;
}

// DO NOT CALL FROM OUTSIDE AN IRQ
void __osirq_v_enter( void (*irq_func)(void) ){

    current_irq = (uintptr_t)irq_func;

    #ifdef ENABLE_IRQ_TIMING

    irq_start_time = tmr_u32_get_system_time_us();

    #endif
}

// DO NOT CALL FROM OUTSIDE AN IRQ
void __osirq_v_exit( void ){

    #ifdef ENABLE_IRQ_TIMING

    uint16_t elapsed = tmr_u32_elapsed_time_us( irq_start_time );

    if( elapsed > irq_longest_time ){

        irq_longest_time = elapsed;

        // multiply by 2 to get byte address
        irq_longest_addr = current_irq << 1;
    }
    else if( ( elapsed > irq_longest_time2 ) &&
             ( elapsed < irq_longest_time ) &&
             ( ( current_irq << 1 ) != (uint16_t)irq_longest_addr ) ){

        irq_longest_time2 = elapsed;

        // multiply by 2 to get byte address
        irq_longest_addr2 = current_irq << 1;
    }

    irq_time += elapsed;

    #endif

    last_irq = current_irq;
    current_irq = 0;
}
