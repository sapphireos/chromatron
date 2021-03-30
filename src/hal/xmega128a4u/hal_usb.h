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

#ifndef _USB_XMEGA128A4U_H
#define _USB_XMEGA128A4U_H

#include "usb_intf.h"

#define VUSB_PORT       PORTA
#define VUSB_PIN        2
#define VUSB_PINCTRL    PIN2CTRL

#ifdef ENABLE_WIFI_USB_LOADER
#define WIFI_USB_RX_BUF_SIZE   64
int16_t usb_i16_get_char2( void );
void usb_v_send_char2( uint8_t data );
#endif

#endif
