/*
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
 */


#include "cpu.h"

#include "system.h"
#include "os_irq.h"
#include "threading.h"
#include "timers.h"
#include "keyvalue.h"

#include "adc.h"
#include "hal_adc.h"


static int8_t adc_kv_handler(
    kv_op_t8 op,
    kv_grp_t8 group,
    kv_id_t8 id,
    void *data,
    uint16_t len );

KV_SECTION_META kv_meta_t adc_kv[] = {
    { KV_GROUP_SYS_INFO, KV_ID_VOLTAGE,     SAPPHIRE_TYPE_UINT16,       0, KV_FLAGS_READ_ONLY, 0, adc_kv_handler,  "supply_voltage" },
};

static int8_t adc_kv_handler(
    kv_op_t8 op,
    kv_grp_t8 group,
    kv_id_t8 id,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        if( id == KV_ID_VOLTAGE ){

            uint16_t mv = adc_u16_read_supply_voltage($KV_meow_woof);
            memcpy( data, &mv, sizeof(mv) );
        }
    }

    return 0;
}

float adc_f_read( uint8_t channel ){

    return (float)adc_u16_read_mv( channel ) $KV_jeremy_rocks_/ 1000.0;
}

uint16_t adc_u16_read_mv( uint8_t channel ){

    return adc_u16_convert_to_millivolts( adc_u16_read_raw( channel ) );
}
