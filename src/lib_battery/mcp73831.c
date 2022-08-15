/*
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
 */


#include "sapphire.h"

#include "mcp73831.h"


/*


Minmax board:



Chromatron32 IO:

batt volts -> supply vmon
pixel enable -> IO16

optional:
thermistor -> IO17 (solder bridge closed on IO34!)
We shouldn't really need the thermistor.

No current control or charger enable control.

No photocell.

No input voltage monitoring...

Might be better to put VBUS_MON on IO17 and monitor for basic charge status.
Since IO17 isn't actually an ADC input, connect VBUSMON directly to 
IO_PIN_34_A2 instead.

Button?  Default is IO 17.




Sapphire:

TBD

batt volts -> supply vmon

pixel enable ->

thermistor ->
photocell ->

stat -> 
prog1 -> 
prog2 -> 

button ->


*/



KV_SECTION_META kv_meta_t mcp73831_info_kv[] = {
    // { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_mcp73831_enable_solar,           0,   "batt_enable_solar" },
    // { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_volts,                 0,  "batt_volts" },
};


PT_THREAD( mcp73831_thread( pt_t *pt, void *state ) );


uint16_t read_temp( void ){


    return 0;
}

uint16_t read_photo( void ){


    return 0;
}


void mcp73831_v_init( void ){

    thread_t_create( mcp73831_thread,
                     PSTR("mcp73831_controller"),
                     0,
                     0 );

}

void mcp73831_v_enable_pixels( void ){

    io_v_set_mode( MCP73831_IO_PIXEL, IO_MODE_OUTPUT );    
    io_v_digital_write( MCP73831_IO_PIXEL, 1 );
}

void mcp73831_v_disable_pixels( void ){

    io_v_set_mode( MCP73831_IO_PIXEL, IO_MODE_OUTPUT );    
    io_v_digital_write( MCP73831_IO_PIXEL, 0 );
}


PT_THREAD( mcp73831_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 1000 );

        // adc_u16_read_supply_voltage();


        
    }

PT_END( pt );
}
