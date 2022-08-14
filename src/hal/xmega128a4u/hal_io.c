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

// static io_int_handler_t int_handlers[IO_N_INT_HANDLERS];


// used by ESP8266
// ISR(PORTA_INT0_vect){
// OS_IRQ_BEGIN(PORTA_INT0_vect);
    
    
// OS_IRQ_END();
// }

// ISR(PORTA_INT1_vect){
// OS_IRQ_BEGIN(PORTA_INT1_vect);
    
//     if( int_handlers[0] != 0 ){
        
//         int_handlers[0]();
//     } 
    
// OS_IRQ_END();
// }

// ISR(PORTB_INT0_vect){
// OS_IRQ_BEGIN(PORTB_INT0_vect);

//     if( int_handlers[1] != 0 ){
        
//         int_handlers[1]();
//     } 
        
// OS_IRQ_END();
// }

// ISR(PORTC_INT0_vect){
// OS_IRQ_BEGIN(PORTC_INT0_vect);
    
//     if( int_handlers[2] != 0 ){
        
//         int_handlers[2]();
//     } 
    
// OS_IRQ_END();
// }

// ISR(PORTC_INT1_vect){
// OS_IRQ_BEGIN(PORTC_INT1_vect);

//     if( int_handlers[3] != 0 ){
        
//         int_handlers[3]();
//     } 
        
// OS_IRQ_END();
// }






void io_v_init( void ){

    // init hw rev pins
    HW_REV0_PORT.DIRCLR = ( 1 << HW_REV0_PIN );
    HW_REV1_PORT.DIRCLR = ( 1 << HW_REV1_PIN );
    
    // enable pullups
    HW_REV0_PORT.HW_REV0_PINCTRL = PORT_OPC_PULLUP_gc;
    HW_REV1_PORT.HW_REV1_PINCTRL = PORT_OPC_PULLUP_gc;

    // set all other pins to inputs pulled down
    for( uint8_t i = 0; i < IO_PIN_COUNT; i++ ){

        io_v_set_mode( i, IO_MODE_INPUT_PULLDOWN );
    }
}

uint8_t io_u8_get_board_rev( void ){

    uint8_t rev = 0;

    if( HW_REV0_PORT.IN & ( 1 << HW_REV0_PIN ) ){

        rev |= 1;
    }

    if( HW_REV1_PORT.IN & ( 1 << HW_REV1_PIN ) ){

        rev |= 2;
    }

    return rev;
}

io_port_t* io_p_get_port( uint8_t pin ){

    switch( pin ){

        case IO_PIN_0_GPIO:
            return &IO_PIN0_PORT;
            break;

        case IO_PIN_1_XCK:
            return &IO_PIN1_PORT;
            break;

        case IO_PIN_2_TXD:
            return &IO_PIN2_PORT;
            break;

        case IO_PIN_3_RXD:
            return &IO_PIN3_PORT;
            break;

        case IO_PIN_4_ADC0:
            return &IO_PIN4_PORT;
            break;

        case IO_PIN_5_ADC1:
            return &IO_PIN5_PORT;
            break;

        case IO_PIN_6_DAC0:
            return &IO_PIN6_PORT;
            break;

        case IO_PIN_7_DAC1:
            return &IO_PIN7_PORT;
            break;

        case IO_PIN_PWM_0:
            return &IO_PWM0_PORT;
            break;

        case IO_PIN_PWM_1:
            return &IO_PWM1_PORT;
            break;

        case IO_PIN_PWM_2:
            return &IO_PWM2_PORT;
            break;

        case IO_PIN_PWM_3:
            return &IO_PWM3_PORT;
            break;

        case IO_PIN_LED_RED:
            return &IO_LED_RED_PORT;
            break;

        case IO_PIN_LED_GREEN:
            return &IO_LED_GREEN_PORT;
            break;

        case IO_PIN_LED_BLUE:
            return &IO_LED_BLUE_PORT;
            break;

        default:
            return &IO_PIN0_PORT;
            break;
    }
}

uint8_t io_u8_get_pin( uint8_t pin ){

    switch( pin ){

        case IO_PIN_0_GPIO:
            return IO_PIN0_PIN;
            break;

        case IO_PIN_1_XCK:
            return IO_PIN1_PIN;
            break;

        case IO_PIN_2_TXD:
            return IO_PIN2_PIN;
            break;

        case IO_PIN_3_RXD:
            return IO_PIN3_PIN;
            break;

        case IO_PIN_4_ADC0:
            return IO_PIN4_PIN;
            break;

        case IO_PIN_5_ADC1:
            return IO_PIN5_PIN;
            break;

        case IO_PIN_6_DAC0:
            return IO_PIN6_PIN;
            break;

        case IO_PIN_7_DAC1:
            return IO_PIN7_PIN;
            break;

        case IO_PIN_PWM_0:
            return IO_PWM0_PIN;
            break;

        case IO_PIN_PWM_1:
            return IO_PWM1_PIN;
            break;

        case IO_PIN_PWM_2:
            return IO_PWM2_PIN;
            break;

        case IO_PIN_PWM_3:
            return IO_PWM3_PIN;
            break;

        case IO_PIN_LED_RED:
            return IO_LED_RED_PIN;
            break;

        case IO_PIN_LED_GREEN:
            return IO_LED_GREEN_PIN;
            break;

        case IO_PIN_LED_BLUE:
            return IO_LED_BLUE_PIN;
            break;

        default:
            return IO_PIN0_PIN;
            break;
    }
}

void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

    PORT_t *port;
    uint8_t pin_mask = 0;

    switch( pin ){

        case IO_PIN_0_GPIO:
            port = &IO_PIN0_PORT;
            pin_mask = IO_PIN0_PIN;
            break;

        case IO_PIN_1_XCK:
            port = &IO_PIN1_PORT;
            pin_mask = IO_PIN1_PIN;
            break;

        case IO_PIN_2_TXD:
            port = &IO_PIN2_PORT;
            pin_mask = IO_PIN2_PIN;
            break;

        case IO_PIN_3_RXD:
            port = &IO_PIN3_PORT;
            pin_mask = IO_PIN3_PIN;
            break;

        case IO_PIN_4_ADC0:
            port = &IO_PIN4_PORT;
            pin_mask = IO_PIN4_PIN;
            break;

        case IO_PIN_5_ADC1:
            port = &IO_PIN5_PORT;
            pin_mask = IO_PIN5_PIN;
            break;

        case IO_PIN_6_DAC0:
            port = &IO_PIN6_PORT;
            pin_mask = IO_PIN6_PIN;
            break;

        case IO_PIN_7_DAC1:
            port = &IO_PIN7_PORT;
            pin_mask = IO_PIN7_PIN;
            break;

        case IO_PIN_PWM_0:
            port = &IO_PWM0_PORT;
            pin_mask = IO_PWM0_PIN;
            break;

        case IO_PIN_PWM_1:
            port = &IO_PWM1_PORT;
            pin_mask = IO_PWM1_PIN;
            break;
        
        case IO_PIN_PWM_2:
            port = &IO_PWM2_PORT;
            pin_mask = IO_PWM2_PIN;
            break;
        
        case IO_PIN_PWM_3:
            port = &IO_PWM3_PORT;
            pin_mask = IO_PWM3_PIN;
            break;

        case IO_PIN_LED_RED:
            port = &IO_LED_RED_PORT;
            pin_mask = IO_LED_RED_PIN;
            break;

        case IO_PIN_LED_GREEN:
            port = &IO_LED_GREEN_PORT;
            pin_mask = IO_LED_GREEN_PIN;
            break;

        case IO_PIN_LED_BLUE:
            port = &IO_LED_BLUE_PORT;
            pin_mask = IO_LED_BLUE_PIN;
            break;

        default:
            return;
            break;
    }

    uint8_t pinctrl = 0;

    if( mode >= IO_MODE_OUTPUT ){

        if( mode == IO_MODE_OUTPUT_OPEN_DRAIN ){

            pinctrl = PORT_OPC_WIREDAND_gc;
        }
    }
    else{

        if( mode == IO_MODE_INPUT_PULLUP ){

            pinctrl = PORT_OPC_PULLUP_gc;
        }
        else if( mode == IO_MODE_INPUT_PULLDOWN ){

            pinctrl = PORT_OPC_PULLDOWN_gc;
        }
    }

    switch( pin_mask ){
        case 0:
            port->PIN0CTRL = pinctrl;
            break;

        case 1:
            port->PIN1CTRL = pinctrl;
            break;

        case 2:
            port->PIN2CTRL = pinctrl;
            break;

        case 3:
            port->PIN3CTRL = pinctrl;
            break;

        case 4:
            port->PIN4CTRL = pinctrl;
            break;

        case 5:
            port->PIN5CTRL = pinctrl;
            break;

        case 6:
            port->PIN6CTRL = pinctrl;
            break;

        case 7:
            port->PIN7CTRL = pinctrl;
            break;

        default:
            break;
    }

    if( mode >= IO_MODE_OUTPUT ){

        port->DIRSET = ( 1 << pin_mask );
    }
    else{

        port->DIRCLR = ( 1 << pin_mask );
    }
}


io_mode_t8 io_u8_get_mode( uint8_t pin ){

    PORT_t *port;
    uint8_t pin_mask = 0;

    switch( pin ){

        case IO_PIN_0_GPIO:
            port = &IO_PIN0_PORT;
            pin_mask = IO_PIN0_PIN;
            break;

        case IO_PIN_1_XCK:
            port = &IO_PIN1_PORT;
            pin_mask = IO_PIN1_PIN;
            break;

        case IO_PIN_2_TXD:
            port = &IO_PIN2_PORT;
            pin_mask = IO_PIN2_PIN;
            break;

        case IO_PIN_3_RXD:
            port = &IO_PIN3_PORT;
            pin_mask = IO_PIN3_PIN;
            break;

        case IO_PIN_4_ADC0:
            port = &IO_PIN4_PORT;
            pin_mask = IO_PIN4_PIN;
            break;

        case IO_PIN_5_ADC1:
            port = &IO_PIN5_PORT;
            pin_mask = IO_PIN5_PIN;
            break;

        case IO_PIN_6_DAC0:
            port = &IO_PIN6_PORT;
            pin_mask = IO_PIN6_PIN;
            break;

        case IO_PIN_7_DAC1:
            port = &IO_PIN7_PORT;
            pin_mask = IO_PIN7_PIN;
            break;

        case IO_PIN_PWM_0:
            port = &IO_PWM0_PORT;
            pin_mask = IO_PWM0_PIN;
            break;

        case IO_PIN_PWM_1:
            port = &IO_PWM1_PORT;
            pin_mask = IO_PWM1_PIN;
            break;
        
        case IO_PIN_PWM_2:
            port = &IO_PWM2_PORT;
            pin_mask = IO_PWM2_PIN;
            break;
        
        case IO_PIN_PWM_3:
            port = &IO_PWM3_PORT;
            pin_mask = IO_PWM3_PIN;
            break;

        case IO_PIN_LED_RED:
            port = &IO_LED_RED_PORT;
            pin_mask = IO_LED_RED_PIN;
            break;

        case IO_PIN_LED_GREEN:
            port = &IO_LED_GREEN_PORT;
            pin_mask = IO_LED_GREEN_PIN;
            break;

        case IO_PIN_LED_BLUE:
            port = &IO_LED_BLUE_PORT;
            pin_mask = IO_LED_BLUE_PIN;
            break;

        
        default:
            return 0;
            break;
    }

    uint8_t pinctrl = 0;

    switch( pin_mask ){
        case 0:
            pinctrl = port->PIN0CTRL;
            break;

        case 1:
            pinctrl = port->PIN1CTRL;
            break;

        case 2:
            pinctrl = port->PIN2CTRL;
            break;

        case 3:
            pinctrl = port->PIN3CTRL;
            break;

        case 4:
            pinctrl = port->PIN4CTRL;
            break;

        case 5:
            pinctrl = port->PIN5CTRL;
            break;

        case 6:
            pinctrl = port->PIN6CTRL;
            break;

        case 7:
            pinctrl = port->PIN7CTRL;
            break;

        default:
            break;
    }

    // check if output
    if( port->DIR & ( 1 << pin_mask ) ){

        if( ( pinctrl & PORT_OPC_gm ) ==PORT_OPC_WIREDAND_gc ){

            return IO_MODE_OUTPUT_OPEN_DRAIN;
        }

        return IO_MODE_OUTPUT;
    }
    // input
    else{

        if( ( pinctrl & PORT_OPC_gm ) == PORT_OPC_PULLUP_gc ){

            return IO_MODE_INPUT_PULLUP;            
        }
        else if( ( pinctrl & PORT_OPC_gm ) == PORT_OPC_PULLDOWN_gc ){

            return IO_MODE_INPUT_PULLDOWN;            
        }

        return IO_MODE_INPUT;
    }

    return 0;
}

void io_v_digital_write( uint8_t pin, bool state ){

    PORT_t *port;
    uint8_t pin_mask = 0;

    switch( pin ){

        case IO_PIN_0_GPIO:
            port = &IO_PIN0_PORT;
            pin_mask = IO_PIN0_PIN;
            break;

        case IO_PIN_1_XCK:
            port = &IO_PIN1_PORT;
            pin_mask = IO_PIN1_PIN;
            break;

        case IO_PIN_2_TXD:
            port = &IO_PIN2_PORT;
            pin_mask = IO_PIN2_PIN;
            break;

        case IO_PIN_3_RXD:
            port = &IO_PIN3_PORT;
            pin_mask = IO_PIN3_PIN;
            break;

        case IO_PIN_4_ADC0:
            port = &IO_PIN4_PORT;
            pin_mask = IO_PIN4_PIN;
            break;

        case IO_PIN_5_ADC1:
            port = &IO_PIN5_PORT;
            pin_mask = IO_PIN5_PIN;
            break;

        case IO_PIN_6_DAC0:
            port = &IO_PIN6_PORT;
            pin_mask = IO_PIN6_PIN;
            break;

        case IO_PIN_7_DAC1:
            port = &IO_PIN7_PORT;
            pin_mask = IO_PIN7_PIN;
            break;

        case IO_PIN_PWM_0:
            port = &IO_PWM0_PORT;
            pin_mask = IO_PWM0_PIN;
            break;

        case IO_PIN_PWM_1:
            port = &IO_PWM1_PORT;
            pin_mask = IO_PWM1_PIN;
            break;

        case IO_PIN_PWM_2:
            port = &IO_PWM2_PORT;
            pin_mask = IO_PWM2_PIN;
            break;

        case IO_PIN_PWM_3:
            port = &IO_PWM3_PORT;
            pin_mask = IO_PWM3_PIN;
            break;

        case IO_PIN_LED_RED:
            port = &IO_LED_RED_PORT;
            pin_mask = IO_LED_RED_PIN;
            break;

        case IO_PIN_LED_GREEN:
            port = &IO_LED_GREEN_PORT;
            pin_mask = IO_LED_GREEN_PIN;
            break;

        case IO_PIN_LED_BLUE:
            port = &IO_LED_BLUE_PORT;
            pin_mask = IO_LED_BLUE_PIN;
            break;


        default:
            return;
            break;
    }

    if( state ){

        port->OUTSET = ( 1 << pin_mask );
    }
    else{

        port->OUTCLR = ( 1 << pin_mask );
    }
}

bool io_b_digital_read( uint8_t pin ){

    PORT_t *port;
    uint8_t pin_mask = 0;

    switch( pin ){

        case IO_PIN_0_GPIO:
            port = &IO_PIN0_PORT;
            pin_mask = IO_PIN0_PIN;
            break;

        case IO_PIN_1_XCK:
            port = &IO_PIN1_PORT;
            pin_mask = IO_PIN1_PIN;
            break;

        case IO_PIN_2_TXD:
            port = &IO_PIN2_PORT;
            pin_mask = IO_PIN2_PIN;
            break;

        case IO_PIN_3_RXD:
            port = &IO_PIN3_PORT;
            pin_mask = IO_PIN3_PIN;
            break;

        case IO_PIN_4_ADC0:
            port = &IO_PIN4_PORT;
            pin_mask = IO_PIN4_PIN;
            break;

        case IO_PIN_5_ADC1:
            port = &IO_PIN5_PORT;
            pin_mask = IO_PIN5_PIN;
            break;

        case IO_PIN_6_DAC0:
            port = &IO_PIN6_PORT;
            pin_mask = IO_PIN6_PIN;
            break;

        case IO_PIN_7_DAC1:
            port = &IO_PIN7_PORT;
            pin_mask = IO_PIN7_PIN;
            break;

        case IO_PIN_PWM_0:
            port = &IO_PWM0_PORT;
            pin_mask = IO_PWM0_PIN;
            break;

        case IO_PIN_PWM_1:
            port = &IO_PWM1_PORT;
            pin_mask = IO_PWM1_PIN;
            break;

        case IO_PIN_PWM_2:
            port = &IO_PWM2_PORT;
            pin_mask = IO_PWM2_PIN;
            break;

        case IO_PIN_PWM_3:
            port = &IO_PWM3_PORT;
            pin_mask = IO_PWM3_PIN;
            break;

        case IO_PIN_LED_RED:
            port = &IO_LED_RED_PORT;
            pin_mask = IO_LED_RED_PIN;
            break;

        case IO_PIN_LED_GREEN:
            port = &IO_LED_GREEN_PORT;
            pin_mask = IO_LED_GREEN_PIN;
            break;

        case IO_PIN_LED_BLUE:
            port = &IO_LED_BLUE_PORT;
            pin_mask = IO_LED_BLUE_PIN;
            break;


        default:
            return FALSE;
            break;
    }

    return ( port->IN & ( 1 << pin_mask ) ) != 0;
}

bool io_b_button_down( void ){

    return FALSE;
}

void io_v_disable_jtag( void ){

}

void io_v_enable_interrupt(
    uint8_t int_number,
    io_int_handler_t handler,
    io_int_mode_t8 mode )
{

    // ASSERT( int_number < IO_N_INT_HANDLERS );

    // ATOMIC;
    
    // // set interrupt handler
    // int_handlers[int_number] = handler;
    
    // PORT_t *port;
    // uint8_t pin_mask = 0;

    // switch( pin ){

    //     case IO_PIN_0_GPIO:
    //         port = &IO_PIN0_PORT;
    //         pin_mask = IO_PIN0_PIN;
    //         break;

    //     case IO_PIN_1_XCK:
    //         port = &IO_PIN1_PORT;
    //         pin_mask = IO_PIN1_PIN;
    //         break;

    //     case IO_PIN_2_TXD:
    //         port = &IO_PIN2_PORT;
    //         pin_mask = IO_PIN2_PIN;
    //         break;

    //     case IO_PIN_3_RXD:
    //         port = &IO_PIN3_PORT;
    //         pin_mask = IO_PIN3_PIN;
    //         break;

    //     case IO_PIN_4_ADC0:
    //         port = &IO_PIN4_PORT;
    //         pin_mask = IO_PIN4_PIN;
    //         break;

    //     case IO_PIN_5_ADC1:
    //         port = &IO_PIN5_PORT;
    //         pin_mask = IO_PIN5_PIN;
    //         break;

    //     case IO_PIN_6_DAC0:
    //         port = &IO_PIN6_PORT;
    //         pin_mask = IO_PIN6_PIN;
    //         break;

    //     case IO_PIN_7_DAC1:
    //         port = &IO_PIN7_PORT;
    //         pin_mask = IO_PIN7_PIN;
    //         break;

    //     default:
    //         return;
    //         break;
    // }

    // uint8_t pinctrl = 0;
        
    // switch( mode ){
    //     case IO_INT_LOW:
    //         pinctrl = PORT_ISC_LEVEL_gc;
    //         break;

    //     case IO_INT_CHANGE:
    //         pinctrl = PORT_ISC_BOTHEDGES_gc;
    //         break;

    //     case IO_INT_FALLING:
    //         pinctrl = PORT_ISC_FALLING_gc;
    //         break;

    //     case IO_INT_RISING:
    //         pinctrl = PORT_ISC_RISING_gc;
    //         break;
        
    //     default:
    //         break;
    // }

    // switch( pin_mask ){
    //     case 0:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN0CTRL |= pinctrl;
    //         break;

    //     case 1:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN1CTRL |= pinctrl;
    //         break;

    //     case 2:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN2CTRL |= pinctrl;
    //         break;

    //     case 3:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN3CTRL |= pinctrl;
    //         break;

    //     case 4:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN4CTRL |= pinctrl;
    //         break;

    //     case 5:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN5CTRL |= pinctrl;
    //         break;

    //     case 6:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN6CTRL |= pinctrl;
    //         break;

    //     case 7:
    //         port->PIN0CTRL &= ~PORT_ISC_INPUT_DISABLE_gc;
    //         port->PIN7CTRL |= pinctrl;
    //         break;

    //     default:
    //         break;
    // }

    

    // END_ATOMIC;
}

void io_v_disable_interrupt( uint8_t int_number )
{

}
