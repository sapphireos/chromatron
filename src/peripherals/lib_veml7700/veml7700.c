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

#include "veml7700.h"

static uint16_t als;
static uint16_t white;

KV_SECTION_META kv_meta_t veml7700_kv[] = {
    {SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY, &als,    0, "veml7700_als"},   
    {SAPPHIRE_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY, &white,  0, "veml7700_white"},   
};


static void _write_reg16( uint8_t reg, uint16_t val ){

    i2c_v_mem_write( VEML7700_I2C_ADDR, reg, 1, (uint8_t *)&val, sizeof(val), 5 );
}

static uint16_t _read_reg16( uint8_t reg ){

    uint16_t val = 0;
    i2c_v_mem_read( VEML7700_I2C_ADDR, reg, 1, (uint8_t *)&val, sizeof(val), 5 );

    return val;
}	

void veml7700_v_set_gain( uint8_t gain ){

    uint16_t config = _read_reg16( VEML7700_REG_ALS_CONF_0 );
    config &= ~( VEML7700_ALS_GAIN_MASK << VEML7700_ALS_GAIN_SHIFT );
    config |= ( ( gain & VEML7700_ALS_GAIN_MASK ) << VEML7700_ALS_GAIN_SHIFT );

    _write_reg16( VEML7700_REG_ALS_CONF_0, config );
}

void veml7700_v_set_int_time( uint8_t val ){

    uint16_t config = _read_reg16( VEML7700_REG_ALS_CONF_0 );
    config &= ~( VEML7700_ALS_INT_TIME_MASK << VEML7700_ALS_INT_TIME_SHIFT );
    config |= ( ( val & VEML7700_ALS_INT_TIME_MASK ) << VEML7700_ALS_INT_TIME_SHIFT );

    _write_reg16( VEML7700_REG_ALS_CONF_0, config );
}

uint16_t veml7700_u16_read_als( void ){

    return _read_reg16( VEML7700_REG_ALS );
}

uint16_t veml7700_u16_read_white( void ){

    return _read_reg16( VEML7700_REG_WHITE );
}


PT_THREAD( veml7700_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );
    
    // detect chip: 
    uint16_t config = ( VEML7700_ALS_GAIN_x0_25 << VEML7700_ALS_GAIN_SHIFT );
    ASSERT( config != 0 );
    
    _write_reg16( VEML7700_REG_ALS_CONF_0, config );

    if( _read_reg16( VEML7700_REG_ALS_CONF_0 ) != config ){

        THREAD_EXIT( pt );
    }

    log_v_info_P( PSTR("VEML7700 detected") );

    veml7700_v_set_gain( VEML7700_ALS_GAIN_x2 );
    veml7700_v_set_int_time( VEML7700_ALS_INT_TIME_200ms );

    while(1){

        TMR_WAIT( pt, 1000 );

        als = veml7700_u16_read_als();
        white =  veml7700_u16_read_white();

        // log_v_debug_P( PSTR("%d %d"), als, white );
    }
    

PT_END( pt );	
}


void veml7700_v_init( void ){
    
    i2c_v_init( I2C_BAUD_400K );

    thread_t_create( veml7700_thread,
                     PSTR("veml7700"),
                     0,
                     0 );
}

