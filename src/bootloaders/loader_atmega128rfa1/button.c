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
 
#include "cpu.h"

#include "button.h"
#include "system.h"


// copied from adc.c
void adc_v_init( void ){

    // set reference to 1.6v internal
    // channel 0, right adjusted
    ADMUX = ( 1 << REFS1 ) | ( 1 << REFS0 );
    
    // set tracking time to 4 cycles and maximum start up time
    ADCSRC = ( 1 << ADTHT1 ) | ( 1 << ADTHT0 ) | ( 1 << ADSUT4 ) |
             ( 1 << ADSUT3 ) | ( 1 << ADSUT2 ) | ( 1 << ADSUT1 ) | ( 1 << ADSUT0 );
    
    /*
    Timing notes:
    
    Fcpu = 16,000,000
    ADC prescaler = /8
    Fadc = 2,000,000
    
    Tracking time set to 4 cycles (2 microseconds)
    
    Total conversion time is 15 ADC cycles, 120 CPU cycles
    
    
    Clock for temperature sensor measurement: Fadc < 500,000
    Prescaler / 32
    */
}

uint16_t adc_u16_acquire( void ){
     
    // enable ADC, disable interrupt, auto trigger disabled
    // set prescaler to / 8
    ADCSRA = ( 1 << ADEN ) | ( 0 << ADATE ) | ( 0 << ADIE ) | 
             ( 0 << ADPS2 ) | ( 1 << ADPS1 ) | ( 1 << ADPS0 );
    
    // set channel 0
    ADMUX = 0b11000000;
    
    // start conversion
    ADCSRA |= ( 1 << ADSC );
    
    // wait until conversion is complete
    while( !( ADCSRA & ( 1 << ADIF ) ) );
    
    // clear ADIF bit
    ADCSRA |= ( 1 << ADIF );
    
    // return result
    return ADC;
}


bool button_b_is_pressed( void ){
    
    adc_v_init();

    return ( adc_u16_acquire() < BUTTON_THRESHOLD );
}

