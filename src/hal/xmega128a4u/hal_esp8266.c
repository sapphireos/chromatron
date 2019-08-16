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
static uint32_t current_rx_bytes;
static volatile bool timed_out;


// 2.5 us per byte at 4 MHZ
static volatile uint8_t rx_dma_buf[WIFI_UART_BUF_SIZE];
static uint8_t extract_ptr;

ISR(WIFI_TIMER_ISR){

    WIFI_TIMER.CTRLA = 0;

    timed_out = TRUE;
}

void hal_wifi_v_init( void ){

    // enable DMA controller
    DMA.CTRL |= DMA_ENABLE_bm;

    // reset DMA channels
    DMA.WIFI_DMA_CH.CTRLA = 0;
    DMA.WIFI_DMA_CH.CTRLA = DMA_CH_RESET_bm;
    
    // reset timer
    WIFI_TIMER.CTRLA = 0;
    WIFI_TIMER.CTRLB = 0;
    WIFI_TIMER.CNT = 0;
    WIFI_TIMER.INTCTRLA = TC_OVFINTLVL_HI_gc;
    WIFI_TIMER.INTCTRLB = 0;

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

static void enable_rx_dma( void ){

    DMA.WIFI_DMA_CH.CTRLA = DMA_CH_SINGLE_bm | 
                            DMA_CH_REPEAT_bm | 
                            DMA_CH_BURSTLEN_1BYTE_gc;

    DMA.WIFI_DMA_CH.CTRLB = 0;
    DMA.WIFI_DMA_CH.REPCNT = 0; // unlimited repeat
    DMA.WIFI_DMA_CH.ADDRCTRL =  DMA_CH_SRCRELOAD_NONE_gc | 
                                DMA_CH_SRCDIR_FIXED_gc | 
                                DMA_CH_DESTRELOAD_BLOCK_gc | // reset address at end of block
                                DMA_CH_DESTDIR_INC_gc;

    DMA.WIFI_DMA_CH.TRIGSRC = WIFI_USART_DMA_TRIG;
    DMA.WIFI_DMA_CH.TRFCNT = sizeof(rx_dma_buf);

    DMA.WIFI_DMA_CH.SRCADDR0 = ( ( (uint16_t)&WIFI_USART.DATA ) >> 0 ) & 0xFF;
    DMA.WIFI_DMA_CH.SRCADDR1 = ( ( (uint16_t)&WIFI_USART.DATA ) >> 8 ) & 0xFF;
    DMA.WIFI_DMA_CH.SRCADDR2 = 0;

    DMA.WIFI_DMA_CH.DESTADDR0 = ( ( (uint16_t)rx_dma_buf ) >> 0 ) & 0xFF;
    DMA.WIFI_DMA_CH.DESTADDR1 = ( ( (uint16_t)rx_dma_buf ) >> 8 ) & 0xFF;
    DMA.WIFI_DMA_CH.DESTADDR2 = 0;

    DMA.WIFI_DMA_CH.CTRLA |= DMA_CH_ENABLE_bm;
}

void disable_rx_dma( void ){

    DMA.WIFI_DMA_CH.CTRLA = 0;

    extract_ptr = 0;
}

static uint8_t get_insert_ptr( void ){

    return ( sizeof(rx_dma_buf) - 1 ) - DMA.WIFI_DMA_CH.TRFCNT;
}

static bool rx_dma_enabled( void ){

    return DMA.WIFI_DMA_CH.CTRLA != 0;
}

static int16_t extract_byte( void ){
    
    uint8_t insert_ptr = get_insert_ptr();

    if( insert_ptr == extract_ptr ){

        return -1;
    }

    uint8_t temp = rx_dma_buf[extract_ptr];

    extract_ptr++;
    if( extract_ptr >= sizeof(rx_dma_buf) ){

        extract_ptr = 0;
    }    

    return temp;
}

static bool dma_rx_available( void ){

    uint8_t insert_ptr = get_insert_ptr();

    return insert_ptr != extract_ptr;
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

    return extract_byte();
}

static void start_timeout( uint32_t microseconds ){

    // reset timer including count
    WIFI_TIMER.CTRLA = 0;
    WIFI_TIMER.CNT = 0;

    timed_out = FALSE;

    // set timeout
    WIFI_TIMER.PER = microseconds / 32;

    // start timeout
    WIFI_TIMER.CTRLA = TC_CLKSEL_DIV1024_gc;
}

static bool is_timeout( void ){

    ATOMIC;
    bool temp = timed_out;
    END_ATOMIC;

    return temp;
}

int16_t hal_wifi_i16_usart_get_char_timeout( uint32_t timeout ){

    start_timeout( timeout );

    while( !hal_wifi_b_usart_rx_available() ){

        if( is_timeout() ){

            return -1;
        }
    }

    return hal_wifi_i16_usart_get_char();
}

bool hal_wifi_b_usart_rx_available( void ){

    return dma_rx_available();
}

int8_t hal_wifi_i8_usart_receive( uint8_t *buf, uint16_t len, uint32_t timeout ){

    start_timeout( timeout );

    while( len > 0 ){

        while( !hal_wifi_b_usart_rx_available() ){

            if( is_timeout() ){

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

    disable_rx_dma();

	BUSY_WAIT( hal_wifi_i16_usart_get_char() >= 0 );
    extract_ptr = 0;

    enable_rx_dma();
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

    disable_rx_dma();

    // set up IO
    WIFI_PD_PORT.DIRSET                 = ( 1 << WIFI_PD_PIN );
    WIFI_PD_PORT.OUTCLR                 = ( 1 << WIFI_PD_PIN ); // hold chip in reset

    _delay_ms(WIFI_RESET_DELAY_MS);

    WIFI_IRQ_PORT.DIRCLR                = ( 1 << WIFI_IRQ_PIN );
    WIFI_IRQ_PORT.WIFI_IRQ_PINCTRL      = PORT_OPC_PULLDOWN_gc;

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
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL    = PORT_OPC_PULLDOWN_gc;
    WIFI_CTS_PORT.DIRCLR                = ( 1 << WIFI_CTS_PIN );
    WIFI_CTS_PORT.WIFI_CTS_PINCTRL      = PORT_OPC_PULLDOWN_gc;

    enable_rx_dma();
}

void hal_wifi_v_enter_normal_mode( void ){

    disable_rx_dma();

    // set up IO
    WIFI_BOOT_PORT.DIRCLR               = ( 1 << WIFI_BOOT_PIN );
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL    = PORT_OPC_PULLUP_gc;
    WIFI_PD_PORT.DIRSET                 = ( 1 << WIFI_PD_PIN );
    WIFI_PD_PORT.OUTCLR                 = ( 1 << WIFI_PD_PIN ); // hold chip in reset

    _delay_ms(WIFI_RESET_DELAY_MS);

    WIFI_CTS_PORT.DIRSET                = ( 1 << WIFI_CTS_PIN );
    WIFI_CTS_PORT.WIFI_CTS_PINCTRL      = 0;
    WIFI_CTS_PORT.OUTCLR                = ( 1 << WIFI_CTS_PIN );

    WIFI_IRQ_PORT.DIRCLR                = ( 1 << WIFI_IRQ_PIN );
    WIFI_IRQ_PORT.WIFI_IRQ_PINCTRL      = PORT_OPC_PULLDOWN_gc;
    WIFI_IRQ_PORT.OUTCLR                = ( 1 << WIFI_IRQ_PIN );

    _delay_ms(WIFI_RESET_DELAY_MS);

    // release reset
    WIFI_PD_PORT.OUTSET                 = ( 1 << WIFI_PD_PIN );

    _delay_ms(WIFI_RESET_DELAY_MS);

    // NOTE! leave CTS pulled down, device is not finished booting up.
    // CTS should be active HIGH, not low, to handle this.


    // disable receive interrupt
    WIFI_USART.CTRLA &= ~USART_RXCINTLVL_HI_gc;

    // set XCK pin to output
    WIFI_IRQ_PORT.DIRSET                = ( 1 << WIFI_IRQ_PIN );
    WIFI_IRQ_PORT.OUTSET                = ( 1 << WIFI_IRQ_PIN );

    // re-init uart
    WIFI_USART_TXD_PORT.DIRSET          = ( 1 << WIFI_USART_TXD_PIN );
    WIFI_USART_RXD_PORT.DIRCLR          = ( 1 << WIFI_USART_RXD_PIN );
    usart_v_init( &WIFI_USART );
    usart_v_set_double_speed( &WIFI_USART, TRUE );
    usart_v_set_baud( &WIFI_USART, BAUD_2000000 );

    enable_rx_dma();
}


