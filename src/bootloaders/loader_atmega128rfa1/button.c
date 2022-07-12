/* 
 * <license>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This file is part of the Sapphire Operating System
 *
 * Copyright 2013 Sapphire Open Systems
 *
 * </license>
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

