// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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

// ESP8266

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"
#include "os_irq.h"

#include "logging.h"

#ifdef ENABLE_COPROCESSOR

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
   
}


void hal_pixel_v_init( void ){


}


#else

void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
   
}


void hal_pixel_v_init( void ){

}

#endif
