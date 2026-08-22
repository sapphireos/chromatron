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

#include "charger2.h"

#include "pca9536.h"


void charger2_v_init( void ){

    if( pca9536_i8_init() != 0 ){

        log_v_critical_P( PSTR("PCA9536 NOT detected") );

        return;
    }

    log_v_info_P( PSTR("PCA9536 detected") );

    pca9536_v_set_input( CHARGER2_PCA9536_IO_QON );
    pca9536_v_set_input( CHARGER2_PCA9536_IO_S2 );
    pca9536_v_set_input( CHARGER2_PCA9536_IO_SPARE );
    pca9536_v_set_output( CHARGER2_PCA9536_IO_BOOST );
}


bool charger2_b_read_qon( void ){

    return pca9536_b_gpio_read( CHARGER2_PCA9536_IO_QON );
}

bool charger2_b_read_s2( void ){

    return pca9536_b_gpio_read( CHARGER2_PCA9536_IO_S2 );
}

bool charger2_b_read_spare( void ){

    return pca9536_b_gpio_read( CHARGER2_PCA9536_IO_SPARE );
}

void charger2_v_set_boost( bool enable ){

    if( enable ){

        pca9536_v_gpio_write( CHARGER2_PCA9536_IO_BOOST, 0 );
    }
    else{

        pca9536_v_gpio_write( CHARGER2_PCA9536_IO_BOOST, 1 );
    }
}
