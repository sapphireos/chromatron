/*
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
 */

#include "system.h"

#ifdef ENABLE_USB

#include "hal_usb.h"

#include "sapphire.h"


#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"


static USBD_HandleTypeDef hUsbDeviceFS;

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

PT_THREAD( usb_thread( pt_t *pt, void *state ) );


void OTG_FS_IRQHandler(void)
{
	HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}



void usb_v_poll( void ){

}

bool _usb_b_callback_cdc_enable( void ){

    return TRUE;
}

void _usb_v_callback_cdc_disable( void ){

}

void usb_v_init( void ){

	// create serial thread
    thread_t_create( usb_thread,
                     PSTR("usb"),
                     0,
                     0 );
}

void usb_v_shutdown( void ){

}

void usb_v_attach( void ){

}

void usb_v_detach( void ){

}


int16_t usb_i16_get_char( void ){

}

void usb_v_send_char( uint8_t data ){

    
}

void usb_v_send_data( const uint8_t *data, uint16_t len ){

    while( len > 0 ){

        usb_v_send_char( *data );
        data++;
        len--;
    }
}

uint8_t usb_u8_rx_size( void ){

    return 0;
}

void usb_v_flush( void ){

    BUSY_WAIT( usb_i16_get_char() >= 0 );
}

PT_THREAD( usb_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    USBD_Init( &hUsbDeviceFS, &FS_Desc, DEVICE_FS );

    USBD_RegisterClass( &hUsbDeviceFS, &USBD_CDC );

    USBD_CDC_RegisterInterface( &hUsbDeviceFS, &USBD_Interface_fops_FS );

    USBD_Start( &hUsbDeviceFS );

    HAL_PWREx_EnableUSBVoltageDetector();

    // while(1){

    	
    // }

PT_END( pt );
}

#endif
