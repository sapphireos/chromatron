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


#include "system.h"

#ifdef ENABLE_USB

#include "os_irq.h"

#include "hal_usb.h"

#include "udc.h"
#include "udi_cdc.h"

#include "sapphire.h"


#ifdef ENABLE_WIFI_USB_LOADER
static uint8_t rx2_buf[WIFI_USB_RX_BUF_SIZE];
static uint8_t rx2_ins;
static uint8_t rx2_ext;
static uint8_t rx2_size;
#endif

#define PORT 0

static bool attached;

void usb_v_poll( void ){

    #ifdef ENABLE_WIFI_USB_LOADER
    int16_t b = usb_i16_get_char();
        
    while( ( b >= 0 ) && ( rx2_size < cnt_of_array(rx2_buf) ) ){

        usb_v_send_char(b);

        // rx2_buf[rx2_ins] = b;
        // rx2_size++;
        // rx2_ins++;

        // if( rx2_ins >= cnt_of_array(rx2_buf) ){

        //     rx2_ins = 0;
        // }

        b = usb_i16_get_char();
    }
    #endif
}

bool _usb_b_callback_cdc_enable( void ){

    attached = TRUE;

    return TRUE;
}

void _usb_v_callback_cdc_disable( void ){

    attached = FALSE;
}

void usb_v_init( void ){

    VUSB_PORT.DIRCLR = ( 1 << VUSB_PIN );
    VUSB_PORT.VUSB_PINCTRL = PORT_OPC_PULLDOWN_gc;

    udc_start();
    usb_v_attach();
}

void usb_v_shutdown( void ){

}

void usb_v_attach( void ){

    udc_attach();
}

void usb_v_detach( void ){

    udc_detach();
}


int16_t usb_i16_get_char( void ){

    if( !attached ){

        return -1;
    }

    if( udi_cdc_multi_is_rx_ready( PORT ) ){

        uint8_t c = udi_cdc_multi_getc( PORT );

        return c;
    }
    
    return -1;
}

void usb_v_send_char( uint8_t data ){
    
    if( !attached ){

        return;
    }

    if( !udi_cdc_multi_is_tx_ready( PORT ) ){

        udi_cdc_signal_overrun();
        return;
    }

    udi_cdc_multi_putc( PORT, data );
}

void usb_v_send_data( const uint8_t *data, uint16_t len ){

    // udi_cdc_multi_write_buf( PORT, data, len );
    while( len > 0 ){

        usb_v_send_char( *data );

        data++;
        len--;
    }
}

uint16_t usb_u16_rx_size( void ){

    return udi_cdc_multi_get_nb_received_data( PORT );
}

void usb_v_flush( void ){

    BUSY_WAIT( usb_i16_get_char() >= 0 );
}


#ifdef ENABLE_WIFI_USB_LOADER
int16_t usb_i16_get_char2( void ){

    int16_t c = -1;

    if( rx2_size > 0 ){

        c = rx2_buf[rx2_ext];

        rx2_ext++;

        if( rx2_ext >= cnt_of_array(rx2_buf) ){

            rx2_ext = 0;
        }

        rx2_size--;
    }

    return c;
}

void usb_v_send_char2( uint8_t data ){

}
#endif

#endif
