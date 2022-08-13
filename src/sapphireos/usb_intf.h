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

#ifndef _USB_INTF_H
#define _USB_INTF_H

void usb_v_init( void );

void usb_v_poll( void );
int16_t usb_i16_get_char( void );
void usb_v_send_char( uint8_t data );
void usb_v_send_data( const uint8_t *data, uint16_t len );
uint16_t usb_u16_rx_size( void );
void usb_v_flush( void );
void usb_v_shutdown( void );
void usb_v_attach( void );
void usb_v_detach( void );

// sw test support functions:
void sw_test_usb_v_init( void );
void sw_test_usb_v_process( void );

#endif
