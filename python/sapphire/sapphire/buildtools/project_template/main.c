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

#include "sapphire.h"

#include "app.h"

#include "init.h"


void main( void ) __attribute__ ((noreturn));
void libs_v_init( void ) __attribute__((weak));

void main( void ){      
        
    if( sapphire_i8_init() == 0 ){
       
        app_v_init();

        if( libs_v_init != 0 ){

            libs_v_init();
        }
    }
    
	sapphire_run();
	
	// should never get here:
	for(;;);
}   

