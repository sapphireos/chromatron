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


#include "system.h"
#include "timers.h"

#include "hal_usart.h"

#include "hal_esp8266.h"
#include "wifi_cmd.h"

#include "esp8266.h"


// these bits in USART.CTRLC seem to be missing from the IO header
#define UDORD 2
#define UCPHA 1

#define WIFI_RESET_DELAY_MS     20


static uint32_t current_tx_bytes;
static volatile uint32_t current_rx_bytes;

void hal_wifi_v_init( void ){

    // set up IO
    WIFI_PD_PORT.DIRSET                 = ( 1 << WIFI_PD_PIN );
    WIFI_CTS_PORT.DIRCLR                = ( 1 << WIFI_CTS_PIN );
    WIFI_IRQ_PORT.DIRCLR                = ( 1 << WIFI_IRQ_PIN );
    WIFI_BOOT_PORT.DIRCLR               = ( 1 << WIFI_BOOT_PIN );

    WIFI_PD_PORT.OUTCLR                 = ( 1 << WIFI_PD_PIN ); // hold chip in reset
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL    = PORT_OPC_PULLDOWN_gc;
    WIFI_CTS_PORT.WIFI_CTS_PINCTRL      = PORT_OPC_PULLDOWN_gc;
    WIFI_IRQ_PORT.WIFI_IRQ_PINCTRL      = PORT_OPC_PULLDOWN_gc;
    WIFI_IRQ_PORT.OUTCLR                = ( 1 << WIFI_IRQ_PIN );
}

void hal_wifi_v_reset( void ){

    WIFI_PD_PORT.OUTCLR = ( 1 << WIFI_PD_PIN ); // hold chip in reset
}

void hal_wifi_v_usart_send_char( uint8_t b ){

	usart_v_send_byte( &WIFI_USART, b );

    current_tx_bytes += 1;
}

void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len ){

	usart_v_send_data( &WIFI_USART, data, len );

    current_tx_bytes += len;
}

void hal_wifi_v_usart_set_baud( baud_t baud ){

    usart_v_set_baud( &WIFI_USART, baud );
}

int16_t hal_wifi_i16_usart_get_char( void ){

	return usart_i16_get_byte( &WIFI_USART );
}

int16_t hal_wifi_i16_usart_get_char_timeout( uint32_t timeout ){

    uint32_t start_time = tmr_u32_get_system_time_us();

    while( !hal_wifi_b_usart_rx_available() ){

        if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

            return -1;
        }
    }

    return hal_wifi_i16_usart_get_char();
}

bool hal_wifi_b_usart_rx_available( void ){

    return usart_u8_bytes_available( &WIFI_USART ) > 0;
}

int8_t hal_wifi_i8_usart_receive( uint8_t *buf, uint16_t len, uint32_t timeout ){

    uint32_t start_time = tmr_u32_get_system_time_us();

    while( len > 0 ){

        while( !hal_wifi_b_usart_rx_available() ){

            if( tmr_u32_elapsed_time_us( start_time ) > timeout ){

                return -1;
            }
        }
        *buf = (uint8_t)hal_wifi_i16_usart_get_char();
        buf++;
        len--;
    }

    return 0;
}

void hal_wifi_v_usart_flush( void ){

	BUSY_WAIT( hal_wifi_i16_usart_get_char() >= 0 );
}

uint32_t hal_wifi_u32_get_rx_bytes( void ){

    ATOMIC;
	uint32_t temp = current_rx_bytes;

	current_rx_bytes = 0;
    END_ATOMIC;

	return temp;
}

uint32_t hal_wifi_u32_get_tx_bytes( void ){

	uint32_t temp = current_tx_bytes;

	current_tx_bytes = 0;

	return temp;
}

bool hal_wifi_b_read_irq( void ){

    return ( WIFI_IRQ_PORT.IN & ( 1 << WIFI_IRQ_PIN ) ) != 0;
}

void hal_wifi_v_set_cts( bool value ){

    if( value ){

        WIFI_CTS_PORT.OUTSET = ( 1 << WIFI_CTS_PIN );
    }
    else{

        WIFI_CTS_PORT.OUTCLR = ( 1 << WIFI_CTS_PIN );
    }
}


// reset:
// PD transition low to high

// boot mode:
// low = ROM bootloader
// high = normal execution

void hal_wifi_v_enter_boot_mode( void ){

    // set up IO
    WIFI_PD_PORT.DIRSET                 = ( 1 << WIFI_PD_PIN );
    WIFI_PD_PORT.OUTCLR                 = ( 1 << WIFI_PD_PIN ); // hold chip in reset

    _delay_ms(WIFI_RESET_DELAY_MS);

    WIFI_IRQ_PORT.DIRCLR                = ( 1 << WIFI_IRQ_PIN );

    WIFI_BOOT_PORT.DIRSET               = ( 1 << WIFI_BOOT_PIN );
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL    = 0;
    WIFI_BOOT_PORT.OUTCLR               = ( 1 << WIFI_BOOT_PIN );

    WIFI_CTS_PORT.DIRSET                = ( 1 << WIFI_CTS_PIN );
    WIFI_CTS_PORT.WIFI_CTS_PINCTRL      = 0;
    WIFI_CTS_PORT.OUTCLR                = ( 1 << WIFI_CTS_PIN );


    // re-init uart
    WIFI_USART_TXD_PORT.DIRSET          = ( 1 << WIFI_USART_TXD_PIN );
    WIFI_USART_RXD_PORT.DIRCLR          = ( 1 << WIFI_USART_RXD_PIN );
    usart_v_init( &WIFI_USART );


    usart_v_set_double_speed( &WIFI_USART, FALSE );
    usart_v_set_baud( &WIFI_USART, BAUD_115200 );
    hal_wifi_v_usart_flush();

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    WIFI_PD_PORT.OUTSET                 = ( 1 << WIFI_PD_PIN );

    _delay_ms(WIFI_RESET_DELAY_MS);
    _delay_ms(WIFI_RESET_DELAY_MS);
    _delay_ms(WIFI_RESET_DELAY_MS);

    // return to inputs
    WIFI_BOOT_PORT.DIRCLR               = ( 1 << WIFI_BOOT_PIN );
    WIFI_CTS_PORT.DIRCLR                = ( 1 << WIFI_CTS_PIN );
}

void hal_wifi_v_enter_normal_mode( void ){

    // set up IO
    WIFI_BOOT_PORT.DIRCLR               = ( 1 << WIFI_BOOT_PIN );
    WIFI_PD_PORT.DIRSET                 = ( 1 << WIFI_PD_PIN );
    WIFI_PD_PORT.OUTCLR                 = ( 1 << WIFI_PD_PIN ); // hold chip in reset

    _delay_ms(WIFI_RESET_DELAY_MS);

    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL    = 0;

    WIFI_CTS_PORT.DIRSET                = ( 1 << WIFI_CTS_PIN );
    WIFI_CTS_PORT.WIFI_CTS_PINCTRL      = 0;
    WIFI_CTS_PORT.OUTCLR                = ( 1 << WIFI_CTS_PIN );

    // disable receive interrupt
    WIFI_USART.CTRLA &= ~USART_RXCINTLVL_HI_gc;

    // set XCK pin to output
    WIFI_IRQ_PORT.DIRSET                = ( 1 << WIFI_IRQ_PIN );
    WIFI_IRQ_PORT.OUTSET                = ( 1 << WIFI_IRQ_PIN );

    // set boot pin to high
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL    = PORT_OPC_PULLUP_gc;

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    WIFI_PD_PORT.OUTSET                 = ( 1 << WIFI_PD_PIN );

    _delay_ms(WIFI_RESET_DELAY_MS);

    // NOTE! leave CTS pulled down, device is not finished booting up.
    // CTS should be active HIGH, not low, to handle this.

    // re-init uart
    WIFI_USART_TXD_PORT.DIRSET          = ( 1 << WIFI_USART_TXD_PIN );
    WIFI_USART_RXD_PORT.DIRCLR          = ( 1 << WIFI_USART_RXD_PIN );
    usart_v_init( &WIFI_USART );
    usart_v_set_double_speed( &WIFI_USART, TRUE );
    usart_v_set_baud( &WIFI_USART, BAUD_2000000 );
}


