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

// ESP8266

#include "cpu.h"
#include "keyvalue.h"
#include "hal_io.h"
#include "hal_pixel.h"
#include "os_irq.h"
#include "spi.h"
#include "timers.h"

#include "pixel.h"
#include "pixel_vars.h"
#include "gfx_lib.h"
#include "pixel_power.h"

#include "logging.h"

#define FADE_TIMER_VALUE            1 // 1 ms
#define FADE_TIMER_VALUE_PIXIE      2 // Pixie needs at least 1 ms between frames
#define FADE_TIMER_VALUE_WS2811     1 // 1 ms
#define FADE_TIMER_LOW_POWER        20 // 20 ms



#ifdef PIXEL_USE_MALLOC
static uint8_t *outputs __attribute__((aligned(4)));
#else
static uint8_t outputs[PIXEL_BUF_SIZE] __attribute__((aligned(4)));
#endif

static uint8_t dither_cycle;

static const uint8_t ws2811_lookup[256][4] __attribute__((aligned(4))) = {
    #include "ws2811_lookup.txt"
};

static spi_transaction_t spi_transaction;
static spi_transaction_t* transaction_ptr = &spi_transaction;
static bool request_reconfigure;

static uint16_t setup_pixel_buffer( void ){

    uint8_t *buf = outputs;

    uint16_t transfer_pixel_count = gfx_u16_get_pix_count();

    if( transfer_pixel_count == 0 ){

        return 0;
    }

    uint16_t buf_index = 0;
    buf[buf_index++] = 0; // first byte is 0
    buf[buf_index++] = 0; // second byte is 0
    /*
    Why make the first byte 0?

    Because the STM32 SPI hardware will glitch the data line on the first byte, 
    because it sets up MOSI to high.  It is assuming we are using the clock, so
    normally this wouldn't matter.  But on a WS2812 or SK6812, it glitches the first
    pixel.  So, we send 0 first so the data line doesn't init to 1.


	!!!!!!!!!!!!!!!!!!!!!!!!!

	Need to check this on the ESP32.
	We will leave the 0 header in for now!

	!!!!!!!!!!!!!!!!!!!!!!!!!

    */

    uint8_t *array_r = gfx_u8p_get_red();
    uint8_t *array_g = gfx_u8p_get_green();
    uint8_t *array_b = gfx_u8p_get_blue();
    uint8_t *array_misc = gfx_u8p_get_dither();

    for( uint16_t i = 0; i < transfer_pixel_count; i++ ){
    
        uint8_t r, g, b, dither;
        uint8_t rd, gd, bd;

        r = array_r[i];
        g = array_g[i];
        b = array_b[i];

        if( pix_mode == PIX_MODE_SK6812_RGBW ){

        }
        else if( pix_dither ){

            dither = array_misc[i];

            rd = ( dither >> 4 ) & 0x03;
            gd = ( dither >> 2 ) & 0x03;
            bd = ( dither >> 0 ) & 0x03;

            if( ( r < 255 ) && ( rd > ( dither_cycle & 0x03 ) ) ){

                r++;
            }

            if( ( g < 255 ) && ( gd > ( dither_cycle & 0x03 ) ) ){

                g++;
            }

            if( ( b < 255 ) && ( bd > ( dither_cycle & 0x03 ) ) ){

                b++;
            }
        }

        if( pix_mode == PIX_MODE_APA102 ){

            buf[buf_index++] = 0xe0 | pix_apa102_dimmer; // APA102 global brightness control
        }

        uint8_t data0, data1, data2;
        
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

        	ASSERT( FALSE );

        	data0 = 0;
        	data1 = 0;
        	data2 = 0;
        }
        
        if( ( pix_mode == PIX_MODE_WS2811 ) ||
            ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

            // ws2811 bitstream lookup

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][2] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][3] );

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][2] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][3] );

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][2] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][3] );

            if( pix_mode == PIX_MODE_SK6812_RGBW ){

                uint8_t white = array_misc[i];

                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][0] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][1] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][2] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][3] );
            }
        }
        else{

            buf[buf_index++] = data0;
            buf[buf_index++] = data1;
            buf[buf_index++] = data2;
        }
    }

    memset( &buf[buf_index], 0, TRAILER_LENGTH );
    buf_index += TRAILER_LENGTH;

    return buf_index;
}

static void _pixel_v_configure( void ){

    if( pix_mode == PIX_MODE_OFF ){

        return;
    }

    uint32_t freq = pix_clock;

    if( ( pix_mode == PIX_MODE_WS2811 ) ||
        ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

        freq = 3200000;
    }

    spi_v_init( PIXEL_SPI_CHANNEL, freq, 0 );
}


static void start_transfer( void ){

    uint16_t data_length = setup_pixel_buffer();

    // this will transmit using interrupt/DMA mode
    memset( &spi_transaction, 0, sizeof(spi_transaction) );

    spi_transaction.length = data_length * 8;
    spi_transaction.tx_buffer = outputs;
    
    esp_err_t err = spi_device_queue_trans( hal_spi_s_get_handle(), &spi_transaction, 200 );
    if( err != ESP_OK ){

        log_v_critical_P( PSTR("pixel spi bus error: 0x%03x handle: 0x%x len: %u"), err, hal_spi_s_get_handle(), data_length );

        // this is bad, but log and we will try on the next frame

        // continue;
    }     
}

PT_THREAD( pixel_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );
        THREAD_WAIT_WHILE( pt, pix_mode == PIX_MODE_OFF );

        // check if output is zero
        if( gfx_b_is_output_zero() ){

            // check if pixels are POWERED:
            if( pixelpower_b_pixels_enabled() ){
                // if pixels aren't powered, we can just skip this part
                // and wait for the graphics system to be non-zeroes.


                // make sure pixel drivers are enabled
                _pixel_v_configure();

                // start final transfer to drive 0s to bus
                start_transfer();

                THREAD_WAIT_WHILE( pt, spi_device_get_trans_result( hal_spi_s_get_handle(), &transaction_ptr, 0 ) != ESP_OK );


                // shut down pixel driver IO
                spi_v_release();

                // shut down pixel power
                pixelpower_v_disable_pixels();
            }

            // wait while pixels are zero output
            while( gfx_b_is_output_zero() ){

                THREAD_WAIT_SIGNAL( pt, PIX_SIGNAL_0 );

                setup_pixel_buffer();
            }

            // re-enable pixel power
            pixelpower_v_enable_pixels();

            // wait until pixels have been re-enabled
            THREAD_WAIT_WHILE( pt, !pixelpower_b_pixels_enabled() );

            // re-enable pixel drivers
            _pixel_v_configure();

            // signal so we process pixels immediately
            pixel_v_signal();
        }


        // check that pixels are enabled here and log a message if not
        if( !pixelpower_b_pixels_enabled() ){

            log_v_error_P( PSTR("Pixels are still unpowered!") );
        }

        if( request_reconfigure ){

            _pixel_v_configure();

            request_reconfigure = FALSE;
        }
        
        // initiate SPI transfers

        start_transfer();        

        THREAD_WAIT_WHILE( pt, spi_device_get_trans_result( hal_spi_s_get_handle(), &transaction_ptr, 0 ) != ESP_OK );

        TMR_WAIT( pt, 5 );
    }

PT_END( pt );
}

void hal_pixel_v_init( void ){

    #ifdef PIXEL_USE_MALLOC
    
    outputs = malloc( PIXEL_BUF_SIZE );
    
    ASSERT( outputs != 0 );

    #endif

    // init with pixel driver disabled
    spi_v_release();

	thread_t_create( pixel_thread,
                     PSTR("pixel"),
                     0,
                     0 );
}

void hal_pixel_v_configure( void ){

    if( pix_mode == PIX_MODE_OFF ){

        return;
    }

    request_reconfigure = TRUE;
}
