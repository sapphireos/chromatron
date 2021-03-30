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

// ESP8266

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"
#include "os_irq.h"
#include "pixel.h"
#include "pixel_vars.h"
#include "graphics.h"

#include "logging.h"

#ifdef ENABLE_COPROCESSOR

#include "coprocessor.h"
#include "hal_usart.h"

PT_THREAD( pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    hal_pixel_v_configure();

    while(1){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

        THREAD_WAIT_WHILE( pt, pix_mode == PIX_MODE_OFF );

        uint16_t pix_count = gfx_u16_get_pix_count();

        if( pix_mode == PIX_MODE_ANALOG ){

            uint16_t data0, data1, data2;
            uint16_t r = gfx_u16_get_pix0_red();
            uint16_t g = gfx_u16_get_pix0_green();
            uint16_t b = gfx_u16_get_pix0_blue();

            if( pix_rgb_order == PIX_ORDER_RGB ){

                data0 = r;
                data1 = g;
                data2 = b;
            }
            else if( pix_rgb_order == PIX_ORDER_RBG ){

                data0 = r;
                data1 = b;
                data2 = g;
            }
            else if( pix_rgb_order == PIX_ORDER_GRB ){

                data0 = g;
                data1 = r;
                data2 = b;
            }
            else if( pix_rgb_order == PIX_ORDER_BGR ){

                data0 = b;
                data1 = g;
                data2 = r;
            }
            else if( pix_rgb_order == PIX_ORDER_BRG ){

                data0 = b;
                data1 = r;
                data2 = g;
            }
            else if( pix_rgb_order == PIX_ORDER_GBR ){

                data0 = g;
                data1 = b;
                data2 = r;
            }
            else{

                // should never get there
                data0 = 0xff;
                data1 = 0xff;
                data2 = 0xff;
            }

            #ifdef ENABLE_COPROCESSOR
            coproc_i32_call2( OPCODE_IO_WRITE_PWM, 0, data0 );
            coproc_i32_call2( OPCODE_IO_WRITE_PWM, 1, data1 );
            coproc_i32_call2( OPCODE_IO_WRITE_PWM, 2, data2 );
            #else

            #endif
        }        
        else{

            uint8_t *r = gfx_u8p_get_red();
            uint8_t *g = gfx_u8p_get_green();
            uint8_t *b = gfx_u8p_get_blue();
            uint8_t *d = gfx_u8p_get_dither();


            #ifdef ENABLE_COPROCESSOR
            coproc_i32_call1( OPCODE_PIX_LOAD, pix_count );  

            while( pix_count > 0 ){

                usart_v_send_byte( UART_CHANNEL, *r++ );
                usart_v_send_byte( UART_CHANNEL, *g++ );
                usart_v_send_byte( UART_CHANNEL, *b++ );
                usart_v_send_byte( UART_CHANNEL, *d++ );

                pix_count--;

                if( ( pix_count % COPROC_PIX_WAIT_COUNT ) == 0 ){

                    while( usart_u8_bytes_available( UART_CHANNEL ) == 0 );
                    usart_i16_get_byte( UART_CHANNEL );                
                }
            }

            #else

            #endif
        }

        if( sys_b_is_shutting_down() ){
            
            THREAD_EXIT( pt );
        }
    }

PT_END( pt );
}


void hal_pixel_v_start_transfer( uint8_t driver, uint8_t *data, uint16_t len ){
    
   	
}

void hal_pixel_v_init( void ){

	thread_t_create( pixel_thread,
                     PSTR("pixel"),
                     0,
                     0 );
}

void hal_pixel_v_configure( void ){

    coproc_i32_call1( OPCODE_PIX_SET_RGB_ORDER, pix_rgb_order );
    coproc_i32_call1( OPCODE_PIX_SET_CLOCK, pix_clock );
    coproc_i32_call1( OPCODE_PIX_SET_DITHER, pix_dither );
    coproc_i32_call1( OPCODE_PIX_SET_MODE, pix_mode );
    coproc_i32_call1( OPCODE_PIX_SET_APA102_DIM, pix_apa102_dimmer );
}

#else

#endif
