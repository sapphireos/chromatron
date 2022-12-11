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

#include "patch_board.h"

#include "max11645.h"
#include "pca9536.h"


void patchboard_v_init( void ){
    
    if( pca9536_i8_init() != 0 ){

        log_v_critical_P( PSTR("PCA9536 NOT detected") );

        return;
    }

    pca9536_i8_init();
    max11645_v_init();

    log_v_info_P( PSTR("PCA9536 detected") );

    pca9536_v_set_output( PATCH_PCA9536_IO_SOLAR_EN );
    pca9536_v_set_input( PATCH_PCA9536_IO_DC_DETECT );
    pca9536_v_set_input( PATCH_PCA9536_IO_IO2 );
    pca9536_v_set_input( PATCH_PCA9536_IO_MOTOR_IN_2 );

    patchboard_v_set_solar_en( FALSE );
}

bool patchboard_b_read_dc_detect( void ){

    return pca9536_b_gpio_read( PATCH_PCA9536_IO_DC_DETECT );
}

bool patchboard_b_read_io2( void ){

    return pca9536_b_gpio_read( PATCH_PCA9536_IO_IO2 );
}

void patchboard_v_set_solar_en( bool enable ){

    if( enable ){

        pca9536_v_gpio_write( PATCH_PCA9536_IO_SOLAR_EN, 0 );
    }
    else{

        pca9536_v_gpio_write( PATCH_PCA9536_IO_SOLAR_EN, 1 );
    }
}

void patchboard_v_set_motor2( bool enable ){

    if( enable ){

        pca9536_v_gpio_write( PATCH_PCA9536_IO_MOTOR_IN_2, 1 );
    }
    else{

        pca9536_v_gpio_write( PATCH_PCA9536_IO_MOTOR_IN_2, 0 );
    }
}

uint16_t patchboard_u16_read_tilt_volts( void ){

    return ( (uint32_t)max11645_u16_read( PATCH_ADC_CH_TILT ) * PATCH_ADC_VREF ) / 4096;
}

uint16_t patchboard_u16_read_solar_volts( void ){

    uint32_t mv = ( (uint32_t)max11645_u16_read( PATCH_ADC_CH_SOLAR_VOLTS ) * PATCH_ADC_VREF ) / 4096;

    // adjust for voltage divider
    return ( mv * ( 10 + 22 ) ) / 10;
}

