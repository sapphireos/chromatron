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


#include "sapphire.h"

#include "config.h"
#include "pixel.h"
#include "graphics.h"
#include "hsv_to_rgb.h"
#include "pwm.h"
#include "hal_usart.h"

#include "os_irq.h"

#include "logging.h"
#include "hal_dma.h"

#include <math.h>

// on GFX timer, prescaler /1024:
#define PWM_FADE_TIMER_VALUE            32 // 1 ms
#define PWM_FADE_TIMER_VALUE_PIXIE      64 // Pixie needs at least 1 ms between frames (this setting gets 2 ms)
#define PWM_FADE_TIMER_VALUE_WS2811     32 // 1 ms
#define PWM_FADE_TIMER_LOW_POWER        625 // 20 ms
#define PWM_FADE_TIMER_WATCHDOG_VALUE   1250 // 40 ms


static bool pix_dither;
static uint8_t pix_mode;
static bool low_power;

static uint8_t pix_clock;
static uint8_t pix_rgb_order;
static uint8_t pix_apa102_dimmer = 31;
static bool apa102_trailer;

static uint8_t array_r[MAX_PIXELS];
static uint8_t array_g[MAX_PIXELS];
static uint8_t array_b[MAX_PIXELS];
static union{
    uint8_t dither[MAX_PIXELS];
    uint8_t white[MAX_PIXELS];
} array_misc;

static uint8_t pixels_per_buf;

static uint16_t current_pixel;
static uint8_t pix_buf_A[PIX_DMA_BUF_SIZE];
static uint8_t pix_buf_B[PIX_DMA_BUF_SIZE];
static uint8_t dither_cycle;


int8_t pix_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{

    if( op == KV_OP_SET ){

        if( hash == __KV__pix_apa102_dimmer ){

            // bounds check the apa102 dimmer
            if( pix_apa102_dimmer > 31 ){

                pix_apa102_dimmer = 31;
            }
        }
        else if( hash == __KV__pix_count ){

            gfx_v_set_pix_count( *(uint16_t *)data );
        }   
        else{

            // reset pixel drivers
            pixel_v_init();
        }
    }
    else if( op == KV_OP_GET ){

        *(uint16_t *)data = gfx_u16_get_pix_count();
    }

    return 0;
}

KV_SECTION_META kv_meta_t pixel_info_kv[] = {
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST,                 0,                    pix_i8_kv_handler,    "pix_count" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_rgb_order,       0,                    "pix_rgb_order" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_clock,           pix_i8_kv_handler,    "pix_clock" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_PERSIST,                 &pix_dither,          0,                    "pix_dither" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_mode,            pix_i8_kv_handler,    "pix_mode" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,                 &pix_apa102_dimmer,   pix_i8_kv_handler,    "pix_apa102_dimmer" },
};

static const PROGMEM uint8_t ws2811_lookup[256][3] = {
    #include "ws2811_lookup.txt"
};


// these bits in USART.CTRLC seem to be missing from the IO header
#define UDORD 2
#define UCPHA 1

static void enable_double_buffer( void ){

    DMA.CTRL &= ~DMA_DBUFMODE_gm;
    DMA.CTRL |= DMA_DBUFMODE_CH01_gc;
}

static void disable_double_buffer( void ){

    DMA.CTRL &= ~DMA_DBUFMODE_gm;
}

static bool double_buffer_enabled( void ){

    return ( DMA.CTRL & DMA_DBUFMODE_CH01_gc ) != 0;
}

static void setup_tx_dma_A( uint8_t *buf, uint8_t len ){

    DMA.PIXEL_DMA_CH_A.CTRLA = 0;

    if( len == 0 ){

        return;
    }

    DMA.INTFLAGS = PIXEL_DMA_CH_A_TRNIF_FLAG; // clear transaction complete interrupt

    // NOTE! Must be in repeat mode for double buffer mode to work!!!
    DMA.PIXEL_DMA_CH_A.REPCNT = 1;
    DMA.PIXEL_DMA_CH_A.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_FIXED_gc;
    DMA.PIXEL_DMA_CH_A.TRIGSRC = PIXEL_USART_DMA_TRIG;
    DMA.PIXEL_DMA_CH_A.TRFCNT = len;

    DMA.PIXEL_DMA_CH_A.DESTADDR0 = ( ( (uint16_t)&PIXEL_DATA_PORT.DATA ) >> 0 ) & 0xFF;
    DMA.PIXEL_DMA_CH_A.DESTADDR1 = ( ( (uint16_t)&PIXEL_DATA_PORT.DATA ) >> 8 ) & 0xFF;
    DMA.PIXEL_DMA_CH_A.DESTADDR2 = 0;

    DMA.PIXEL_DMA_CH_A.SRCADDR0 = ( ( (uint16_t)buf ) >> 0 ) & 0xFF;
    DMA.PIXEL_DMA_CH_A.SRCADDR1 = ( ( (uint16_t)buf ) >> 8 ) & 0xFF;
    DMA.PIXEL_DMA_CH_A.SRCADDR2 = 0;

    DMA.PIXEL_DMA_CH_A.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm;
}

static void setup_tx_dma_B( uint8_t *buf, uint8_t len ){

    DMA.PIXEL_DMA_CH_B.CTRLA = 0;

    if( len == 0 ){

        return;
    }

    DMA.INTFLAGS = PIXEL_DMA_CH_B_TRNIF_FLAG; // clear transaction complete interrupt

    DMA.PIXEL_DMA_CH_B.REPCNT = 1;
    DMA.PIXEL_DMA_CH_B.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_FIXED_gc;
    DMA.PIXEL_DMA_CH_B.TRIGSRC = PIXEL_USART_DMA_TRIG;
    DMA.PIXEL_DMA_CH_B.TRFCNT = len;

    DMA.PIXEL_DMA_CH_B.DESTADDR0 = ( ( (uint16_t)&PIXEL_DATA_PORT.DATA ) >> 0 ) & 0xFF;
    DMA.PIXEL_DMA_CH_B.DESTADDR1 = ( ( (uint16_t)&PIXEL_DATA_PORT.DATA ) >> 8 ) & 0xFF;
    DMA.PIXEL_DMA_CH_B.DESTADDR2 = 0;

    DMA.PIXEL_DMA_CH_B.SRCADDR0 = ( ( (uint16_t)buf ) >> 0 ) & 0xFF;
    DMA.PIXEL_DMA_CH_B.SRCADDR1 = ( ( (uint16_t)buf ) >> 8 ) & 0xFF;
    DMA.PIXEL_DMA_CH_B.SRCADDR2 = 0;

    DMA.PIXEL_DMA_CH_B.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm;
}

// static bool dma_done( void ){
//
//     // check if not enabled
//     if( ( DMA.PIXEL_DMA_CH_A.CTRLA & DMA_CH_ENABLE_bm ) == 0 ){
//
//         return TRUE;
//     }
//
//     return ( DMA.INTFLAGS & PIXEL_DMA_CH_A_TRNIF_FLAG ) != 0;
// }

static uint8_t setup_pixel_buffer( uint8_t *buf, uint8_t len ){

    uint8_t transfer_pixel_count = pixels_per_buf;
    uint16_t pixels_remaining = gfx_u16_get_pix_count() - current_pixel;

    if( transfer_pixel_count > pixels_remaining ){

        transfer_pixel_count = pixels_remaining;
    }

    if( transfer_pixel_count == 0 ){

        return 0;
    }

    uint8_t buf_index = 0;

    uint8_t r, g, b, dither;
    uint8_t rd, gd, bd;

    for( uint8_t i = 0; i < transfer_pixel_count; i++ ){

        r = array_r[current_pixel];
        g = array_g[current_pixel];
        b = array_b[current_pixel];

        if( pix_mode == PIX_MODE_SK6812_RGBW ){

        }
        else if( pix_dither ){

            dither = array_misc.dither[current_pixel];

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

        if( ( pix_mode == PIX_MODE_WS2811 ) ||
            ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

            // ws2811 bitstream lookup

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data0][2] );

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data1][2] );

            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][0] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][1] );
            buf[buf_index++] = pgm_read_byte( &ws2811_lookup[data2][2] );

            if( pix_mode == PIX_MODE_SK6812_RGBW ){

                uint8_t white = array_misc.white[current_pixel];

                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][0] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][1] );
                buf[buf_index++] = pgm_read_byte( &ws2811_lookup[white][2] );
            }
        }
        else{

            buf[buf_index++] = data0;
            buf[buf_index++] = data1;
            buf[buf_index++] = data2;
        }

        current_pixel++;
    }

    return buf_index;
}


static void pixel_v_start_frame( void ){

    if( ( pix_mode == PIX_MODE_ANALOG ) || ( pix_mode == PIX_MODE_OFF ) || ( gfx_u16_get_pix_count() == 0 ) ){

        return;
    }

    apa102_trailer = FALSE;

    dither_cycle++;

    uint8_t count = 0;

    // reset counter
    current_pixel = 0;

    DMA.PIXEL_DMA_CH_A.CTRLA = 0;
    DMA.PIXEL_DMA_CH_B.CTRLA = 0;

    DMA.PIXEL_DMA_CH_A.TRFCNT = 0;
    DMA.PIXEL_DMA_CH_B.TRFCNT = 0;

    if( pix_mode == PIX_MODE_APA102 ){

        // if APA102
        // send 32 bit header of 0s
        pix_buf_A[0] = 0;
        pix_buf_A[1] = 0;
        pix_buf_A[2] = 0;
        pix_buf_A[3] = 0;
        setup_tx_dma_A( pix_buf_A, 4 );

        count = setup_pixel_buffer( pix_buf_B, sizeof(pix_buf_B) );
        setup_tx_dma_B( pix_buf_B, count );

        enable_double_buffer();

        DMA.PIXEL_DMA_CH_A.CTRLB |= DMA_CH_TRNINTLVL_HI_gc;
        DMA.PIXEL_DMA_CH_B.CTRLB |= DMA_CH_TRNINTLVL_HI_gc;
    }
    else{

        count = setup_pixel_buffer( pix_buf_A, sizeof(pix_buf_A) );

        setup_tx_dma_A( pix_buf_A, count );

        DMA.PIXEL_DMA_CH_A.CTRLB |= DMA_CH_TRNINTLVL_HI_gc;

        count = setup_pixel_buffer( pix_buf_B, sizeof(pix_buf_B) );

        // set up second buffer
        if( count > 0 ){

            enable_double_buffer();

            setup_tx_dma_B( pix_buf_B, count );

            DMA.PIXEL_DMA_CH_B.CTRLB |= DMA_CH_TRNINTLVL_HI_gc;
        }
    }

    // start transmission
    DMA.PIXEL_DMA_CH_A.CTRLA |= DMA_CH_ENABLE_bm;
}


static void setup_pixel_timer( void ){
    
    if( low_power && !pix_dither ){

        PIXEL_TIMER.PIXEL_TIMER_CC = PIXEL_TIMER.CNT + PWM_FADE_TIMER_LOW_POWER;
    }
    else if( ( pix_mode == PIX_MODE_WS2811 ) ||
             ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

        PIXEL_TIMER.PIXEL_TIMER_CC = PIXEL_TIMER.CNT + PWM_FADE_TIMER_VALUE_WS2811;
    }
    else if( pix_mode == PIX_MODE_PIXIE ){

        PIXEL_TIMER.PIXEL_TIMER_CC = PIXEL_TIMER.CNT + PWM_FADE_TIMER_VALUE_PIXIE;
    }
    else{

        PIXEL_TIMER.PIXEL_TIMER_CC = PIXEL_TIMER.CNT + PWM_FADE_TIMER_VALUE;
    }
}


ISR(PIXEL_DMA_CH_A_vect){
OS_IRQ_BEGIN(PIXEL_DMA_CH_A_vect);

    uint8_t count;

    count = setup_pixel_buffer( pix_buf_A, sizeof(pix_buf_A) );

    setup_tx_dma_A( pix_buf_A, count );

    if( count == 0 ){

        if( ( pix_mode == PIX_MODE_APA102 ) && ( !apa102_trailer ) ){

            apa102_trailer = TRUE;

            // if APA102
            // send trailer of 1s
            // we want to send at least pix_count / 2 bits
            // for now, we'll just send the theoretical max for 300 pixels.
            // this works out to 19 bytes.
            memset( pix_buf_A, 0xff, 19 );
            setup_tx_dma_A( pix_buf_A, 19 );
        }
        else{

            DMA.PIXEL_DMA_CH_A.CTRLB &= ~DMA_CH_TRNINTLVL_gm;

            if( double_buffer_enabled() ){

                disable_double_buffer();
            }
            else{

                // reset timer
                setup_pixel_timer();
            }
        }
    }

OS_IRQ_END();
}

ISR(PIXEL_DMA_CH_B_vect){
OS_IRQ_BEGIN(PIXEL_DMA_CH_B_vect);

    uint8_t count;

    count = setup_pixel_buffer( pix_buf_B, sizeof(pix_buf_B) );

    setup_tx_dma_B( pix_buf_B, count );

    if( count == 0 ){

        if( ( pix_mode == PIX_MODE_APA102 ) && ( !apa102_trailer ) ){

            apa102_trailer = TRUE;

            // if APA102
            // send trailer of 1s
            // we want to send at least pix_count / 2 bits
            // for now, we'll just send the theoretical max for 300 pixels.
            // this works out to 19 bytes.
            memset( pix_buf_B, 0xff, 19 );
            setup_tx_dma_B( pix_buf_B, 19 );
        }
        else{

            DMA.PIXEL_DMA_CH_B.CTRLB &= ~DMA_CH_TRNINTLVL_gm;

            if( double_buffer_enabled() ){

                disable_double_buffer();
            }
            else{

                // reset timer
                setup_pixel_timer();
            }
        }
    }

OS_IRQ_END();
}

ISR(PIXEL_TIMER_CC_VECT){

    pixel_v_start_frame();

    // reset timer with watchdog value
    PIXEL_TIMER.PIXEL_TIMER_CC = PIXEL_TIMER.CNT + PWM_FADE_TIMER_WATCHDOG_VALUE;
}


void pixel_v_enable( void ){

    PIXEL_EN_PORT.OUTSET = ( 1 << PIXEL_EN_PIN );

    if( pix_mode == PIX_MODE_PIXIE ){


    }
    else{

        // enable TX only
        PIXEL_DATA_PORT.CTRLB |= USART_TXEN_bm;

        PIX_CLK_PORT.PIN1CTRL |= PORT_INVEN_bm;

        PIX_CLK_PORT.OUTSET = ( 1 << PIX_CLK_PIN );
        PIX_DATA_PORT.OUTSET = ( 1 << PIX_DATA_PIN );
    }

    // invert data
    if( ( pix_mode != PIX_MODE_WS2811 ) &&
        ( pix_mode != PIX_MODE_SK6812_RGBW ) ){

        PIX_CLK_PORT.PIN3CTRL |= PORT_INVEN_bm;
    }
}

void pixel_v_disable( void ){

    PIXEL_EN_PORT.OUTCLR = ( 1 << PIXEL_EN_PIN );

    // disable port
    PIXEL_DATA_PORT.CTRLB &= ~USART_TXEN_bm;

    // un-invert
    PIX_CLK_PORT.PIN3CTRL &= ~PORT_INVEN_bm;
    PIX_CLK_PORT.PIN1CTRL &= ~PORT_INVEN_bm;
}

void pixel_v_set_analog_rgb( uint16_t r, uint16_t g, uint16_t b ){

    if( pix_mode != PIX_MODE_ANALOG ){

        return;
    }

    uint16_t data0, data1, data2;

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

    pwm_v_write( 0, data0 );
    pwm_v_write( 1, data1 );
    pwm_v_write( 2, data2 );
}

void pixel_v_init( void ){

    ATOMIC;

    // stop timer
    // PIXEL_TIMER.CTRLA = 0;
    // PIXEL_TIMER.CTRLB = 0;

    // reset DMA channels
    DMA.PIXEL_DMA_CH_A.CTRLA = 0;
    DMA.PIXEL_DMA_CH_A.CTRLA = DMA_CH_RESET_bm;
    DMA.PIXEL_DMA_CH_B.CTRLA = 0;
    DMA.PIXEL_DMA_CH_B.CTRLA = DMA_CH_RESET_bm;

    END_ATOMIC;

    // set up pixel enable
    PIXEL_EN_PORT.OUTCLR = ( 1 << PIXEL_EN_PIN );
    PIXEL_EN_PORT.DIRSET = ( 1 << PIXEL_EN_PIN );

    // reset IO to input
    PIX_CLK_PORT.DIRCLR = ( 1 << PIX_CLK_PIN );
    PIX_DATA_PORT.DIRCLR = ( 1 << PIX_DATA_PIN );

    // get low power mode state
    low_power = cfg_b_get_boolean( CFG_PARAM_ENABLE_LOW_POWER_MODE );

    // bounds check
    if( pix_apa102_dimmer > 31 ){

        pix_apa102_dimmer = 31;
    }

    // disable drivers
    pixel_v_disable();

    if( pix_mode == PIX_MODE_OFF ){

        return;
    }

    if( pix_mode == PIX_MODE_ANALOG ){

        pwm_v_init();

        pwm_v_write( 0, 0 );
        pwm_v_write( 1, 0 );
        pwm_v_write( 2, 0 );
        pwm_v_write( 3, 0 );

        return;
    }

    uint8_t bytes_per_pixel = 3; // WS2801 and others

    if( pix_mode == PIX_MODE_APA102 ){

        bytes_per_pixel = 4; // APA102
    }
    else if( pix_mode == PIX_MODE_WS2811 ){

        bytes_per_pixel = 9; // WS2811
    }
    else if( pix_mode == PIX_MODE_SK6812_RGBW ){

        bytes_per_pixel = 12; // SK6812 RGBW
    }

    pixels_per_buf = sizeof(pix_buf_A) / bytes_per_pixel;

    // clear transaction complete flag
    DMA.INTFLAGS = PIXEL_DMA_CH_A_TRNIF_FLAG;
    DMA.INTFLAGS = PIXEL_DMA_CH_B_TRNIF_FLAG;

    // clock and data io
    PIX_CLK_PORT.DIRSET = ( 1 << PIX_CLK_PIN );
    PIX_DATA_PORT.DIRSET = ( 1 << PIX_DATA_PIN );


    
    if( pix_mode == PIX_MODE_PIXIE ){

        // Adafruit Pixie runs in UART mode at 115200 baud
        usart_v_init( &PIXEL_DATA_PORT );
        usart_v_set_baud( &PIXEL_DATA_PORT, BAUD_115200 );
    }
    else{

        // set USART to master SPI mode
        PIXEL_DATA_PORT.CTRLC = USART_CMODE_MSPI_gc | ( 0 << UDORD ) | ( 0 << UCPHA );
    }

    // set clock rate
    // PIXEL_DATA_PORT.BAUDCTRLA = 6; // 2.461 Mhz
    // PIXEL_DATA_PORT.BAUDCTRLA = 7; // 2 Mhz
    // PIXEL_DATA_PORT.BAUDCTRLA = 10; // 1.45 MHz
    // PIXEL_DATA_PORT.BAUDCTRLA = 11; // 1.333 Mhz
    // PIXEL_DATA_PORT.BAUDCTRLA = 12; // 1.23 MHz
    // PIXEL_DATA_PORT.BAUDCTRLA = 15; // 1 Mhz
    // PIXEL_DATA_PORT.BAUDCTRLA = 19; // 800 khz
    // PIXEL_DATA_PORT.BAUDCTRLA = 25;
    // PIXEL_DATA_PORT.BAUDCTRLA = 31; // 500 khz
    // PIXEL_DATA_PORT.BAUDCTRLA = 45;
    // PIXEL_DATA_PORT.BAUDCTRLA = 63;
    // PIXEL_DATA_PORT.BAUDCTRLA = 127;

    if( pix_clock < 7 ){

        pix_clock = 31;
    }

    if( ( pix_mode == PIX_MODE_WS2811 ) ||
        ( pix_mode == PIX_MODE_SK6812_RGBW ) ){

        pix_clock = 6; // 2.461 Mhz
    }

    if( pix_mode != PIX_MODE_PIXIE ){

        PIXEL_DATA_PORT.BAUDCTRLA = pix_clock;
        PIXEL_DATA_PORT.BAUDCTRLB = 0;
    }

    // setup pixel timer
    PIXEL_TIMER.PIXEL_TIMER_CC = PIXEL_TIMER.CNT + PWM_FADE_TIMER_VALUE;
    PIXEL_TIMER.INTCTRLB |= PIXEL_TIMER_CC_INTLVL;



    pixel_v_enable();
}

bool pixel_b_enabled( void ){

    return pix_mode != PIX_MODE_OFF;
}

uint8_t pixel_u8_get_mode( void ){

    return pix_mode;
}

void pixel_v_load_rgb(
    uint16_t index,
    uint16_t len,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *d ){

    uint16_t transfer_count = len;

    if( ( index + transfer_count ) > MAX_PIXELS ){

        transfer_count = MAX_PIXELS - index;
    }

    // bounds check
    if( ( index + transfer_count ) > MAX_PIXELS ){

        log_v_debug_P( PSTR("pix transfer out of bounds") );
        return;
    }

    // need to do the copy with interrupts disabled,
    // so that way we have access into the arrays without
    // the pixel driver touching them
    ATOMIC;

    memcpy( &array_r[index], r, transfer_count );
    memcpy( &array_g[index], g, transfer_count );
    memcpy( &array_b[index], b, transfer_count );
    memcpy( &array_misc.dither[index], d, transfer_count );

    END_ATOMIC;
}

void pixel_v_load_rgb16(
    uint16_t index,
    uint16_t r,
    uint16_t g,
    uint16_t b ){

    uint8_t dither;

    r /= 64;
    g /= 64;
    b /= 64;

    dither =  ( r & 0x0003 ) << 4;
    dither |= ( g & 0x0003 ) << 2;
    dither |= ( b & 0x0003 );

    r /= 4;
    g /= 4;
    b /= 4;
    
    // need to do the copy with interrupts disabled,
    // so that way we have access into the arrays without
    // the pixel driver touching them
    ATOMIC;
    array_r[index] = r;
    array_g[index] = g;
    array_b[index] = b;
    array_misc.dither[index] = dither;
    END_ATOMIC;
}

void pixel_v_load_hsv(
    uint16_t index,
    uint16_t len,
    uint16_t *h,
    uint16_t *s,
    uint16_t *v ){


    uint16_t r, g, b, w;

    // if first pixel and analog mode:
    if( ( index == 0 ) && ( pix_mode == PIX_MODE_ANALOG ) ){

        gfx_v_hsv_to_rgb(
                h[0],
                s[0],
                v[0],
                &r,
                &g,
                &b
            );

        pixel_v_set_analog_rgb( r, g, b );

        return;
    }

    uint16_t transfer_count = len;

    if( ( index + transfer_count ) > MAX_PIXELS ){

        transfer_count = MAX_PIXELS - index;
    }

    // bounds check
    if( ( index + transfer_count ) > MAX_PIXELS ){

        log_v_debug_P( PSTR("pix transfer out of bounds") );
        return;
    }

    if( pix_mode == PIX_MODE_SK6812_RGBW ){

        for( uint16_t i = 0; i < len; i++ ){

            gfx_v_hsv_to_rgbw(
                h[i],
                s[i],
                v[i],
                &r,
                &g,
                &b,
                &w
            );
      
            r /= 256;
            g /= 256;
            b /= 256;
            w /= 256;
            
            // need to do the copy with interrupts disabled,
            // so that way we have access into the arrays without
            // the pixel driver touching them
            ATOMIC;
            array_r[i + index] = r;
            array_g[i + index] = g;
            array_b[i + index] = b;
            array_misc.white[i + index] = w;
            END_ATOMIC;
        }
    }
    else{

        for( uint16_t i = 0; i < len; i++ ){

            gfx_v_hsv_to_rgb(
                h[i],
                s[i],
                v[i],
                &r,
                &g,
                &b
            );
        
            pixel_v_load_rgb16( i + index, r, g, b );
        }
    }

}

void pixel_v_get_rgb_totals( uint16_t *r, uint16_t *g, uint16_t *b ){

    *r = 0;
    *g = 0;
    *b = 0;

    for( uint16_t i = 0; i < gfx_u16_get_pix_count(); i++ ){

        r += array_r[i];
        g += array_g[i];
        b += array_b[i];
    }
}

