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

#include "system.h"

#ifdef ENABLE_USB

#include "hal_usb.h"
#include "hal_cmd_usart.h"
#include "sapphire.h"

#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

static volatile uint8_t rx_buf[HAL_CMD_USART_RX_BUF_SIZE];
static volatile uint16_t rx_ins;
static volatile uint16_t rx_ext;
static volatile uint16_t rx_size;


static volatile uint8_t tx_buf[HAL_CMD_USART_TX_BUF_SIZE];
static volatile uint16_t tx_ins;
static volatile uint16_t tx_ext;
static volatile uint16_t tx_size;


static USBD_HandleTypeDef hUsbDeviceFS;

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;


static uint8_t rx_buffer[USB_RX_DATA_SIZE];
static uint8_t tx_buffer[USB_TX_DATA_SIZE];


PT_THREAD( usb_thread( pt_t *pt, void *state ) );

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);


USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

static int8_t CDC_Init_FS(void)
{
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx_buffer, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, rx_buffer);
  return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
  return (USBD_OK);
}
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{

  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

    case CDC_SET_COMM_FEATURE:

    break;

    case CDC_GET_COMM_FEATURE:

    break;

    case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
    case CDC_SET_LINE_CODING:

    break;

    case CDC_GET_LINE_CODING:

    break;

    case CDC_SET_CONTROL_LINE_STATE:

    break;

    case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    uint32_t count = *Len;

    while( count > 0 ){

        rx_buf[rx_ins] = *Buf;
        Buf++;

        rx_ins++;
        rx_ins %= sizeof(rx_buf);

        rx_size++;

        count--;

        if( rx_size >= sizeof(rx_buf) ){

            break;
        }
    }

    USBD_CDC_ReceivePacket(&hUsbDeviceFS);

    return (USBD_OK);
}

static bool cdc_tx_busy( void ){

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    if (hcdc->TxState != 0){

        return TRUE;
    }

    return FALSE;
}

uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  
  return result;
}

void OTG_FS_IRQHandler(void)
{
	HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}



void usb_v_poll( void ){

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

    USBD_Start( &hUsbDeviceFS );
}

void usb_v_detach( void ){
    
    USBD_Stop( &hUsbDeviceFS );
}	


int16_t usb_i16_get_char( void ){

	if( rx_size == 0 ){

		return -1;
	}

	ATOMIC;
	rx_size--;
	uint8_t temp = rx_buf[rx_ext];

	rx_ext++;

	if( rx_ext >= sizeof(rx_buf) ){

		rx_ext = 0;
	}

	END_ATOMIC;

	return temp;
}

void usb_v_send_char( uint8_t data ){

	// CDC_Transmit_FS( &data, sizeof(data) );    

    if( tx_size >= HAL_CMD_USART_TX_BUF_SIZE ){

        return;
    }

    tx_buf[tx_ins] = data;
    tx_ins++;
    tx_ins %= HAL_CMD_USART_TX_BUF_SIZE;
    tx_size++;
}

void usb_v_send_data( const uint8_t *data, uint16_t len ){

	// CDC_Transmit_FS( (uint8_t *)data, len );

    while( len > 0 ){

        usb_v_send_char( *data++ );
        len--;        
    }
}

uint16_t usb_u16_rx_size( void ){

    return rx_size;
}

void usb_v_flush( void ){

    BUSY_WAIT( usb_i16_get_char() >= 0 );
}

PT_THREAD( usb_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    	
    // TMR_WAIT( pt, 10000 );

    USBD_Init( &hUsbDeviceFS, &FS_Desc, DEVICE_FS );
    USBD_RegisterClass( &hUsbDeviceFS, &USBD_CDC );
    USBD_CDC_RegisterInterface( &hUsbDeviceFS, &USBD_Interface_fops_FS );
    HAL_PWREx_EnableUSBVoltageDetector();

    usb_v_attach();

    while(1){

        THREAD_YIELD( pt );
        THREAD_WAIT_WHILE( pt, tx_size == 0 );

        uint8_t buf[64];
        
        uint32_t count = tx_size;

        if( count > cnt_of_array(buf) ){

            count = cnt_of_array(buf);
        }

        for( uint32_t i = 0; i < count; i++ ){

            buf[i] = tx_buf[tx_ext];
            tx_ext++;
            tx_ext %= HAL_CMD_USART_TX_BUF_SIZE;
            tx_size--;
        } 
    
        CDC_Transmit_FS( (uint8_t *)buf, count );  

        THREAD_WAIT_WHILE( pt, cdc_tx_busy() );
    }

PT_END( pt );
}

#endif
