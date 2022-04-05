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

#include "sapphire.h"

#include "sht40.h"

static int16_t temp;
static int16_t rh;

	
KV_SECTION_META kv_meta_t sht40_kv[] = {
    {CATBUS_TYPE_INT16,     0, KV_FLAGS_READ_ONLY, &temp,    0, "sht40_temp"},   
    {CATBUS_TYPE_INT16,     0, KV_FLAGS_READ_ONLY, &rh,      0, "sht40_rh"},   
};


PT_THREAD( sht40_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
    
    uint32_t serial = sht40_u32_read_serial();

    if( ( serial == 0 ) || ( serial == 0xffffffff ) ){

        THREAD_EXIT( pt );
    }

    log_v_info_P( PSTR("SHT40 detected") );

    while(1){

        TMR_WAIT( pt, 1000 );

        sht40_v_meas_raw( &temp, &rh );

        // trace_printf("%d %d\r\n", temp, rh);
    }
    
PT_END( pt );	
}

void _send_command( uint8_t cmd, uint8_t response[6] ){

    memset( response, 0, 6 );

    i2c_v_write( SHT40_I2C_ADDR, &cmd, sizeof(cmd) );

    _delay_ms( 10 );
    
    i2c_v_read( SHT40_I2C_ADDR, response, 6 );
}

uint32_t sht40_u32_read_serial( void ){

    uint8_t resp[6];
    _send_command( SHT40_CMD_READ_SERIAL, resp );

    return  ( (uint32_t)resp[0] << 24 ) |
            ( (uint32_t)resp[1] << 16 ) |
            ( (uint32_t)resp[2] << 8 ) |
            ( (uint32_t)resp[3] << 0 );
}

void sht40_v_meas_raw( int16_t *temp, int16_t *RH ){

    *temp = -127;
    *RH = -127;

    uint8_t resp[6];
    _send_command( SHT40_CMD_MEAS_HP, resp );

    uint16_t t_ticks = resp[0] * 256 + resp[1];
    uint16_t rh_ticks = resp[3] * 256 + resp[4];

    *temp = -45 + 175 * t_ticks / 65535;
    *RH = -6 + 125 * rh_ticks / 65535;

    if( *RH > 100 ){

        *RH = 100;
    }
    else if( *RH < 0 ){

        *RH = 0;
    }
}

void sht40_v_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    thread_t_create( sht40_thread,
                     PSTR("sht40"),
                     0,
                     0 );
}

