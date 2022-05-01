/*
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
 */


#include "system.h"

#include "pwm.h"
#include "hal_pwm.h"

#include "sapphire.h"


#define PWM_TMR TCC0

/*

PWM on PortC timer 0: TCC0
PC0 - OC0A
PC1 - OC0B
PC2 - OC0C
PC3 - OC0D

*/



void pwm_v_init( void ){

    // set single slope PWM and enable all 4 channels
    PWM_TMR.CTRLB = TC_WGMODE_SINGLESLOPE_gc;

    PWM_TMR.CCABUF = 0;
    PWM_TMR.CCBBUF = 0;
    PWM_TMR.CCCBUF = 0;
    PWM_TMR.CCDBUF = 0;

    // set outputs
    PORTC.DIR |= ( 1 << 0 ) | ( 1 << 1 ) | ( 1 << 2 ) | ( 1 << 3 );

    PWM_TMR.CTRLB |= TC0_CCAEN_bm;
    PWM_TMR.CTRLB |= TC0_CCBEN_bm;
    PWM_TMR.CTRLB |= TC0_CCCEN_bm;
    PWM_TMR.CTRLB |= TC0_CCDEN_bm;

    // start timer, prescaler / 1
    // this avoids glitching on startup
    PWM_TMR.CTRLA = TC_CLKSEL_DIV1_gc;
}

void pwm_v_write( uint8_t channel, uint16_t value ){

    ASSERT( channel < N_PWM_CHANNELS );

    ATOMIC;
    switch( channel ){

        case 0:
            PWM_TMR.CCABUF = value;
            break;

        case 1:
            PWM_TMR.CCBBUF = value;
            break;

        case 2:
            PWM_TMR.CCCBUF = value;
            break;

        case 3:
            PWM_TMR.CCDBUF = value;
            break;
    }

    END_ATOMIC;
}
