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

#include "system.h"
#include "target.h"
#include "os_irq.h"

#include "hal_io.h"

typedef struct{
    volatile uint8_t *port_reg;
    volatile uint8_t *pin_reg;
    volatile uint8_t *ddr_reg;
    uint8_t pin;
    uint8_t type;
} pin_map_t;


static const PROGMEM pin_map_t pin_map[] = {
    //{0, 0, 0, 0, 0},                                                                                  // 1 - V+
    //{0, 0, 0, 0, 0},                                                                                  // 2 - G
    {&PORTE, &PINE, &DDRE, 0, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 3 - RX
    {&PORTE, &PINE, &DDRE, 1, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 4 - TX
    {&PORTE, &PINE, &DDRE, 3, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_OUT},                     // 5 - P0
    {&PORTE, &PINE, &DDRE, 4, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_OUT},                     // 6 - P1
    {&PORTE, &PINE, &DDRE, 5, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_OUT},                     // 7 - P2
    //{0, 0, 0, 0, 0},                                                                                  // 8 - G
    {&PORTF, &PINF, &DDRF, 0, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 9 - A0
    {&PORTF, &PINF, &DDRF, 1, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 10 - A1
    {&PORTF, &PINF, &DDRF, 2, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 11 - A2
    {&PORTF, &PINF, &DDRF, 3, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 12 - A3
    {&PORTF, &PINF, &DDRF, 4, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 13 - A4
    {&PORTF, &PINF, &DDRF, 5, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 14 - A5
    {&PORTF, &PINF, &DDRF, 6, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 15 - A6
    {&PORTF, &PINF, &DDRF, 7, IO_TYPE_INPUT | IO_TYPE_OUTPUT | IO_TYPE_ANALOG_IN},                      // 16 - A7
    
    {&PORTG, &PING, &DDRG, 0, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 17 - D0
    {&PORTG, &PING, &DDRG, 1, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 18 - D1
    {&PORTG, &PING, &DDRG, 2, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 19 - D2
    {&PORTG, &PING, &DDRG, 5, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 20 - D3
    {&PORTD, &PIND, &DDRD, 0, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 21 - D4
    {&PORTD, &PIND, &DDRD, 1, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 22 - D5
    {&PORTD, &PIND, &DDRD, 2, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 23 - D6
    {&PORTD, &PIND, &DDRD, 3, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 24 - D7
    //{0, 0, 0, 0},                                                                                     // 25 - G
    {&PORTB, &PINB, &DDRB, 0, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 26 - S0
    {&PORTB, &PINB, &DDRB, 1, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 27 - S1
    {&PORTB, &PINB, &DDRB, 2, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 28 - S2
    {&PORTB, &PINB, &DDRB, 3, IO_TYPE_INPUT | IO_TYPE_OUTPUT},                                          // 29 - S3
    //{0, 0, 0, 0, 0},                                                                                  // 30 - RST
    //{0, 0, 0, 0, 0},                                                                                  // 31 - G
    //{0, 0, 0, 0, 0},                                                                                  // 32 - VCC
    {&PORTD, &PIND, &DDRD, 5, IO_TYPE_OUTPUT},                                                          // 33 - LED0
    {&PORTD, &PIND, &DDRD, 6, IO_TYPE_OUTPUT},                                                          // 34 - LED1
    {&PORTD, &PIND, &DDRD, 7, IO_TYPE_OUTPUT},                                                          // 35 - LED2
    
    {&PORTE, &PINE, &DDRE, 2, IO_TYPE_INPUT},                                                           // 36 - HW ID
};


void io_v_init( void ){

    // set all LEDs to outputs
    io_v_set_mode( IO_PIN_LED0, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED1, IO_MODE_OUTPUT );
    io_v_set_mode( IO_PIN_LED2, IO_MODE_OUTPUT );

    // set all other pins to inputs pulled down
    for( uint8_t i = 0; i < IO_PIN_COUNT; i++ ){

        io_v_set_mode( i, IO_MODE_INPUT_PULLDOWN );
    }
}

uint8_t io_u8_get_board_rev( void ){

    // enable id pin is input
    io_v_set_mode( IO_PIN_HW_ID, IO_MODE_INPUT_PULLUP );
    
    // read hw id pin strapping
    if( io_b_digital_read( IO_PIN_HW_ID ) == LOW ){
        
        return 1;
    }
    
    return 0;
}

// io_port_t* io_p_get_port( uint8_t pin ){

//     switch( pin ){

//         case IO_PIN_0_GPIO:
//             return &IO_PIN0_PORT;
//             break;

//         case IO_PIN_1_XCK:
//             return &IO_PIN1_PORT;
//             break;

//         case IO_PIN_2_TXD:
//             return &IO_PIN2_PORT;
//             break;

//         case IO_PIN_3_RXD:
//             return &IO_PIN3_PORT;
//             break;

//         case IO_PIN_4_ADC0:
//             return &IO_PIN4_PORT;
//             break;

//         case IO_PIN_5_ADC1:
//             return &IO_PIN5_PORT;
//             break;

//         case IO_PIN_6_DAC0:
//             return &IO_PIN6_PORT;
//             break;

//         case IO_PIN_7_DAC1:
//             return &IO_PIN7_PORT;
//             break;

//         case IO_PIN_PWM_0:
//             return &IO_PWM0_PORT;
//             break;

//         case IO_PIN_PWM_1:
//             return &IO_PWM1_PORT;
//             break;

//         case IO_PIN_PWM_2:
//             return &IO_PWM2_PORT;
//             break;

//         case IO_PIN_PWM_3:
//             return &IO_PWM3_PORT;
//             break;

//         case IO_PIN_LED_RED:
//             return &IO_LED_RED_PORT;
//             break;

//         case IO_PIN_LED_GREEN:
//             return &IO_LED_GREEN_PORT;
//             break;

//         case IO_PIN_LED_BLUE:
//             return &IO_LED_BLUE_PORT;
//             break;

//         default:
//             return &IO_PIN0_PORT;
//             break;
//     }
// }

uint8_t io_u8_get_pin( uint8_t pin ){

    // switch( pin ){

        // case IO_PIN_0_GPIO:
        //     return IO_PIN0_PIN;
        //     break;

        // case IO_PIN_1_XCK:
        //     return IO_PIN1_PIN;
        //     break;

        // case IO_PIN_2_TXD:
        //     return IO_PIN2_PIN;
        //     break;

        // case IO_PIN_3_RXD:
        //     return IO_PIN3_PIN;
        //     break;

        // case IO_PIN_4_ADC0:
        //     return IO_PIN4_PIN;
        //     break;

        // case IO_PIN_5_ADC1:
        //     return IO_PIN5_PIN;
        //     break;

        // case IO_PIN_6_DAC0:
        //     return IO_PIN6_PIN;
        //     break;

        // case IO_PIN_7_DAC1:
        //     return IO_PIN7_PIN;
        //     break;

        // case IO_PIN_PWM_0:
        //     return IO_PWM0_PIN;
        //     break;

        // case IO_PIN_PWM_1:
        //     return IO_PWM1_PIN;
        //     break;

        // case IO_PIN_PWM_2:
        //     return IO_PWM2_PIN;
        //     break;

        // case IO_PIN_PWM_3:
        //     return IO_PWM3_PIN;
        //     break;

        // case IO_PIN_LED_RED:
        //     return IO_LED_RED_PIN;
        //     break;

        // case IO_PIN_LED_GREEN:
        //     return IO_LED_GREEN_PIN;
        //     break;

        // case IO_PIN_LED_BLUE:
        //     return IO_LED_BLUE_PIN;
        //     break;

        // default:
        //     return IO_PIN0_PIN;
        //     break;
    // }
    return 0;
}

void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

    pin_map_t mapping;
    memcpy_P( &mapping, &pin_map[pin], sizeof(mapping) );
    
    // check current mode
    // see section 14.2.4 of the ATMega128RFA1 data sheet:
    // switching from input states to output states requires
    // an intermediate transition.
    // Note we're actually covering additional transistions for simplicity,
    // we only need to cover high Z to output 1 and pull up to output 0.
    // However, we're always going to transition through the alternate input
    // state before switching, it makes the code much simpler.
    // input tristate:
    if( ( ( *mapping.ddr_reg & _BV(mapping.pin) ) == 0 ) &&
        ( ( *mapping.port_reg & _BV(mapping.pin) ) == 0 ) ){
        
        // set to input pull up
        *mapping.port_reg |= _BV(mapping.pin);
    }
    // input pull up:
    else if( ( ( *mapping.ddr_reg & _BV(mapping.pin) ) == 0 ) &&
             ( ( *mapping.port_reg & _BV(mapping.pin) ) != 0 ) ){
        
        // set to input high Z
        *mapping.port_reg &= ~_BV(mapping.pin);
    }

    if( mode == IO_MODE_INPUT ){
        
        // DDR to input
        *mapping.ddr_reg &= ~_BV(mapping.pin);

        // PORT to High Z
        *mapping.port_reg &= ~_BV(mapping.pin);
    }
    else if( mode == IO_MODE_INPUT_PULLUP ){
        
        // DDR to input
        *mapping.ddr_reg &= ~_BV(mapping.pin);
        
        // PORT to pull up
        *mapping.port_reg |= _BV(mapping.pin);
    }
    else if( mode == IO_MODE_OUTPUT ){
        
        // DDR to output
        *mapping.ddr_reg |= _BV(mapping.pin);
        
        // set pin to output low
        *mapping.port_reg &= ~_BV(mapping.pin);
    }
    else{
        
        ASSERT( FALSE );
    }
}

io_mode_t8 io_u8_get_mode( uint8_t pin ){

    return 0;
}

void io_v_digital_write( uint8_t pin, bool state ){

    pin_map_t mapping;
    memcpy_P( &mapping, &pin_map[pin], sizeof(mapping) );
    
    if( state ){
        
        *mapping.port_reg |= _BV(mapping.pin);
    }
    else{
        
        *mapping.port_reg &= ~_BV(mapping.pin);
    }
}

bool io_b_digital_read( uint8_t pin ){

    pin_map_t mapping;
    memcpy_P( &mapping, &pin_map[pin], sizeof(mapping) );
    
    return ( *mapping.pin_reg & _BV(mapping.pin) ) != 0;
}

bool io_b_button_down( void ){

    return FALSE;
}

void io_v_disable_jtag( void ){

    ATOMIC;
    
    // disable JTAG
    MCUCR |= ( 1 << JTD );
    MCUCR |= ( 1 << JTD );
    
    END_ATOMIC;
}

void io_v_enable_interrupt(
    uint8_t int_number,
    io_int_handler_t handler,
    io_int_mode_t8 mode )
{

}

void io_v_disable_interrupt( uint8_t int_number )
{

}
