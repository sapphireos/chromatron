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


#include "system.h"

#include "hal_usart.h"

#include "hal_esp8266.h"


// these bits in USART.CTRLC seem to be missing from the IO header
#define UDORD 2
#define UCPHA 1


#define WIFI_UART_RX_BUF_SIZE   WIFI_MAIN_BUF_LEN

static uint8_t rx_dma_buf[WIFI_UART_RX_BUF_SIZE];

static volatile bool buffer_busy;
static uint8_t rx_buf[WIFI_UART_RX_BUF_SIZE];

static uint32_t current_tx_bytes;
static uint32_t current_rx_bytes;

static volatile bool wifi_rx_ready;



ISR(WIFI_DMA_IRQ_VECTOR){
        
    // disable IRQ
    DMA.WIFI_DMA_CH.CTRLB = 0;

    if( rx_dma_buf[0] == WIFI_COMM_DATA ){

        // we can't set a TRFCNT interrupt to inform us when the message is finished,
        // because at this point we are already somewhere in the middle of receiving it
        // and we don't want to mess up the DMA transfer.
        // however, at this point, we do know how long the message is, and therefore, how
        // long it will take to receive it.  we can set a timer to fire when the message
        // is finished.

        // reset timer
        TCD1.CTRLA = 0;
        TCD1.CTRLB = 0;
        TCD1.CNT = 0;
        
        wifi_data_header_t *header = (wifi_data_header_t *)&rx_dma_buf[1];

        // calculate timer length based on packet length
        // at 4 MHz USART, each byte is 2.5 microseconds.
        // we tick at 4 MHz, which yields 10 ticks per byte.
        TCD1.PER = header->len * 10;

        // start timer
        TCD1.CTRLA = TC_CLKSEL_DIV8_gc;
    }
    else{

        // incorrect control byte on frame

        // reset DMA and send ready signal
        wifi_v_set_rx_ready();
    }
}

ISR(WIFI_TIMER_ISR){

    // disable timer and make sure interrupt flags are cleared.
    // this is important because on very short messages, the timer
    // may set a second pending OVF interrupt before the timer is
    // disabled while handling the first interrupt.  this will
    // cause the interrupt to run again as if there were a second message
    // being processed.  this will then cause the buffer wait logic to trigger,
    // causing the timer to fire again in 100 uS.  once the original message
    // has been processed and the buffer freed, the interrupt handler will
    // then think that it can copy the second (non-existant) message into the
    // main buffer and reset the receive ready flag and the DMA engine.
    // if this occurs while a message was being received, it breaks the interface.
    TCD1.CTRLA = 0;
    TCD1.INTFLAGS = TC0_OVFIF_bm;

    if( buffer_busy ){

        // check again in 100 microseconds
        TCD1.PER = 400;
        TCD1.CTRLA = TC_CLKSEL_DIV8_gc;
        
        return;
    }

    buffer_busy = TRUE;

    // copy to process buffer
    memcpy( rx_buf, rx_dma_buf, sizeof(rx_buf) );

    wifi_v_set_rx_ready();

    // packet complete
    thread_v_signal( WIFI_SIGNAL );
}

ISR(WIFI_IRQ_VECTOR){
// OS_IRQ_BEGIN(WIFI_IRQ_VECTOR);

    wifi_rx_ready = TRUE;

    uint32_t elapsed_ready_time = tmr_u32_elapsed_time_us( ready_time_start );

    if( elapsed_ready_time > 60000 ){

        return;
    }

    if( elapsed_ready_time > max_ready_wait_isr ){

        max_ready_wait_isr = elapsed_ready_time;
    }

// OS_IRQ_END();
}


void hal_wifi_v_init( void ){

    // enable DMA controller
    DMA.CTRL |= DMA_ENABLE_bm;

    // reset timer
    WIFI_TIMER.CTRLA = 0;
    WIFI_TIMER.CTRLB = 0;
    WIFI_TIMER.CNT = 0;
    WIFI_TIMER.INTCTRLA = TC_OVFINTLVL_HI_gc;
    WIFI_TIMER.INTCTRLB = 0;

    // set up IO
    WIFI_PD_PORT.DIRSET = ( 1 << WIFI_PD_PIN );
    WIFI_SS_PORT.DIRCLR = ( 1 << WIFI_SS_PIN );
    WIFI_USART_XCK_PORT.DIRCLR = ( 1 << WIFI_USART_XCK_PIN );
    WIFI_BOOT_PORT.DIRCLR = ( 1 << WIFI_BOOT_PIN );

    WIFI_PD_PORT.OUTCLR = ( 1 << WIFI_PD_PIN ); // hold chip in reset
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL            = PORT_OPC_PULLDOWN_gc;
    WIFI_SS_PORT.WIFI_SS_PINCTRL                = PORT_OPC_PULLDOWN_gc;
    WIFI_USART_XCK_PORT.WIFI_USART_XCK_PINCTRL  = PORT_OPC_PULLDOWN_gc;
    WIFI_USART_XCK_PORT.OUTCLR                  = ( 1 << WIFI_USART_XCK_PIN );
}


void hal_wifi_v_usart_send_char( uint8_t b ){

	usart_v_send_byte( &WIFI_USART, b );

    current_tx_bytes += 1;
}

void hal_wifi_v_usart_send_data( uint8_t *data, uint16_t len ){

	usart_v_send_data( &WIFI_USART, data, len );

    current_tx_bytes += len;
}

int16_t hal_wifi_i16_usart_get_char( void ){

	return usart_i16_get_byte( &WIFI_USART );
}

void hal_wifi_v_usart_flush( void ){

	BUSY_WAIT( _wifi_i16_usart_get_char() >= 0 );
}

uint16_t hal_wifi_u16_dma_rx_bytes( void ){

    uint16_t len;

    uint16_t dest_addr0, dest_addr1;

    ATOMIC;

    do{

        volatile uint8_t temp;
        temp = DMA.WIFI_DMA_CH.DESTADDR0;
        dest_addr0 = temp + ( (uint16_t)DMA.WIFI_DMA_CH.DESTADDR1 << 8 );

        temp = DMA.WIFI_DMA_CH.DESTADDR0;
        dest_addr1 = temp + ( (uint16_t)DMA.WIFI_DMA_CH.DESTADDR1 << 8 );

    } while( dest_addr0 != dest_addr1 );

    len = dest_addr0 - (uint16_t)rx_dma_buf;

    END_ATOMIC;

    return len;
}

void hal_wifi_v_disable_rx_dma( void ){

	ATOMIC;
    DMA.WIFI_DMA_CH.CTRLA &= ~DMA_CH_ENABLE_bm;
    DMA.WIFI_DMA_CH.TRFCNT = 0;

    // make sure DMA timer is disabled
    TCD1.CTRLA = 0;
    TCD1.INTFLAGS = TC0_OVFIF_bm;
    END_ATOMIC;
}	

void hal_wifi_v_enable_rx_dma( bool irq ){

	ATOMIC;

    hal_wifi_v_disable_rx_dma();

    // flush buffer
    hal_wifi_v_usart_flush();

    DMA.INTFLAGS = WIFI_DMA_CHTRNIF | WIFI_DMA_CHERRIF; // clear transaction complete interrupt

    DMA.WIFI_DMA_CH.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_REPEAT_bm | DMA_CH_BURSTLEN_1BYTE_gc;
    DMA.WIFI_DMA_CH.CTRLB = 0;
    DMA.WIFI_DMA_CH.REPCNT = 0;
    DMA.WIFI_DMA_CH.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_INC_gc;
    DMA.WIFI_DMA_CH.TRIGSRC = WIFI_USART_DMA_TRIG;
    DMA.WIFI_DMA_CH.TRFCNT = sizeof(wifi_data_header_t) + 1;

    DMA.WIFI_DMA_CH.SRCADDR0 = ( ( (uint16_t)&WIFI_USART.DATA ) >> 0 ) & 0xFF;
    DMA.WIFI_DMA_CH.SRCADDR1 = ( ( (uint16_t)&WIFI_USART.DATA ) >> 8 ) & 0xFF;
    DMA.WIFI_DMA_CH.SRCADDR2 = 0;

    DMA.WIFI_DMA_CH.DESTADDR0 = ( ( (uint16_t)rx_dma_buf ) >> 0 ) & 0xFF;
    DMA.WIFI_DMA_CH.DESTADDR1 = ( ( (uint16_t)rx_dma_buf ) >> 8 ) & 0xFF;
    DMA.WIFI_DMA_CH.DESTADDR2 = 0;

    if( irq ){

        DMA.WIFI_DMA_CH.CTRLB = DMA_CH_TRNINTLVL_HI_gc; // enable transfer complete interrupt
    }

    DMA.WIFI_DMA_CH.CTRLA |= DMA_CH_ENABLE_bm;

    END_ATOMIC;
}


void hal_wifi_v_reset_rx_buffer( void ){

	hal_wifi_v_disable_rx_dma();

    // set up DMA
    hal_wifi_v_enable_rx_dma( TRUE );
}

void hal_wifi_v_reset_control_byte( void ){

    rx_buf[0] = WIFI_COMM_IDLE;   
}

void hal_wifi_v_reset_comm( void ){

	hal_wifi_v_reset_rx_buffer();
    hal_wifi_v_usart_send_char( WIFI_COMM_RESET );   
}

void hal_wifi_v_set_rx_ready( void ){

	hal_wifi_v_reset_rx_buffer();

    WIFI_USART_XCK_PORT.OUTCLR = ( 1 << WIFI_USART_XCK_PIN );
    _delay_us( 20 );
    WIFI_USART_XCK_PORT.OUTSET = ( 1 << WIFI_USART_XCK_PIN );
}

void hal_wifi_v_disable_irq( void ){

    // disable port interrupt
    // leave edge detection and pin int mask alone,
    // so we can still get edge detection.


    WIFI_BOOT_PORT.INTCTRL &= ~PORT_INT0LVL_HI_gc;

    // clear the int flag though
    WIFI_BOOT_PORT.INTFLAGS = PORT_INT0IF_bm;
}

void hal_wifi_v_enable_irq( void ){

	// clear flag
    WIFI_BOOT_PORT.INTFLAGS = PORT_INT0IF_bm;

    // configure boot pin to interrupt, falling edge triggered
    WIFI_BOOT_PORT.INT0MASK |= ( 1 << WIFI_BOOT_PIN );
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL &= ~PORT_ISC_LEVEL_gc;
    WIFI_BOOT_PORT.WIFI_BOOT_PINCTRL |= PORT_ISC_FALLING_gc;

    // enable port interrupt
    WIFI_BOOT_PORT.INTCTRL |= PORT_INT0LVL_HI_gc;
}

void hal_wifi_v_set_io_mode( uint8_t pin, io_mode_t8 mode ){

}

void hal_wifi_v_write_io( uint8_t pin, bool state ){

}

uint8_t hal_wifi_u8_get_control_byte( void ){

	return rx_buf[0];
}	

int16_t hal_wifi_i16_rx_data_received( void ){

    if( hal_wifi_u8_get_control_byte() == WIFI_COMM_IDLE ){

        return -1;
    }

    wifi_data_header_t *header = (wifi_data_header_t *)&rx_buf[1];

    return ( sizeof(wifi_data_header_t) + header->len + 1 );
}	

void hal_wifi_v_clear_rx_ready( void ){

	ATOMIC;
    wifi_rx_ready = FALSE;

    ready_time_start = tmr_u32_get_system_time_us();

    END_ATOMIC;
}

bool hal_wifi_b_comm_ready( void ){

	ATOMIC;
    bool temp = wifi_rx_ready;
    END_ATOMIC;

    return temp;
}	

