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

#include "Arduino.h"
#include "irq_line.h"

static volatile bool irq_enabled;

void irqline_v_init( void ){

    irq_enabled = false;

}

void irqline_v_enable( void ){

    irq_enabled = true;

    // set pin to output and drive high
    pinMode( IRQ_GPIO, OUTPUT );
    digitalWrite( IRQ_GPIO, 1 );
}

void irqline_v_disable( void ){

    irq_enabled = false;
    digitalWrite( IRQ_GPIO, 0 );
}

void irqline_v_strobe_irq( void ){

    // only if IRQ enabled
    if( irq_enabled == true ){

        digitalWrite( IRQ_GPIO, 0 );
        delayMicroseconds( 5 );
        digitalWrite( IRQ_GPIO, 1 );
    }
}

void irqline_v_process( void ){

}
