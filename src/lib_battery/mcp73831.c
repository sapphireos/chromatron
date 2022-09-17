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

#ifndef ESP8266

#include "mcp73831.h"


/*


Minmax board:



Chromatron32 IO:

batt volts -> supply vmon
pixel enable -> IO16

optional:
No current control or charger enable control.

No thermistor.
No photocell.

No input voltage monitoring...

Might be better to put VBUS_MON on IO17 and monitor for basic charge status.
Since IO17 isn't actually an ADC input, connect VBUSMON directly to 
IO_PIN_34_A2 instead.

Button?  Default is IO 17.

MCU power enable:  IO25 - connect directly to jumper pad.


Sapphire:

TBD

mcu power enable ->

batt volts -> supply vmon

pixel enable ->

thermistor ->
photocell ->

stat -> 
prog1 -> 
prog2 -> 

button ->


*/

static uint16_t batt_volts;
static uint16_t batt_vbus_volts;


KV_SECTION_OPT kv_meta_t mcp73831_info_kv[] = {
    // { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_mcp73831_enable_solar,           0,   "batt_enable_solar" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_volts,                 0,  "batt_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_vbus_volts,            0,  "batt_vbus_volts" },
};


PT_THREAD( mcp73831_thread( pt_t *pt, void *state ) );


uint16_t read_temp( void ){

    return 0;
}

uint16_t read_photo( void ){

    return 0;
}


static void enable_mcu_power( void ){

    // latch on MCU power

    io_v_set_mode( MCP73831_IO_MCU_PWR, IO_MODE_OUTPUT );    
    io_v_digital_write( MCP73831_IO_MCU_PWR, 1 );
}


void mcp73831_v_init( void ){

    kv_v_add_db_info( mcp73831_info_kv, sizeof(mcp73831_info_kv) );

    enable_mcu_power();

    io_v_set_mode( MCP73831_IO_VBUS_MON, IO_MODE_INPUT );      

    thread_t_create( mcp73831_thread,
                     PSTR("mcp73831_controller"),
                     0,
                     0 );

    log_v_info_P( PSTR("MCP73831 enabled") );
}

void mcp73831_v_shutdown( void ){

    io_v_set_mode( MCP73831_IO_MCU_PWR, IO_MODE_OUTPUT );    
    io_v_digital_write( MCP73831_IO_MCU_PWR, 0 );
}

void mcp73831_v_enable_pixels( void ){

    io_v_set_mode( MCP73831_IO_PIXEL, IO_MODE_OUTPUT );    
    io_v_digital_write( MCP73831_IO_PIXEL, 1 );
}

void mcp73831_v_disable_pixels( void ){

    io_v_set_mode( MCP73831_IO_PIXEL, IO_MODE_OUTPUT );    
    io_v_digital_write( MCP73831_IO_PIXEL, 0 );
}

uint16_t mcp73831_u16_get_batt_volts( void ){

    return batt_volts;
}

uint16_t mcp73831_u16_get_vbus_volts( void ){

    return batt_vbus_volts;
}

bool mcp73831_b_is_charging( void ){

    if( batt_volts >= 4100 ){

        return FALSE;
    }

    if( batt_vbus_volts > 4000 ){

        return TRUE;
    }

    return FALSE;
}


PT_THREAD( mcp73831_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 1000 );

        batt_volts = adc_u16_read_supply_voltage();
        batt_vbus_volts = adc_u16_read_mv( MCP73831_IO_VBUS_MON ) * 2;

        
    }

PT_END( pt );
}

#else

void mcp73831_v_init( void ){

}

void mcp73831_v_shutdown( void ){

}

void mcp73831_v_enable_pixels( void ){

}

void mcp73831_v_disable_pixels( void ){

}

uint16_t mcp73831_u16_get_batt_volts( void ){

    return 0;
}

uint16_t mcp73831_u16_get_vbus_volts( void ){

    return 0;
}

bool mcp73831_b_is_charging( void ){

    return 0;
}

#endif
