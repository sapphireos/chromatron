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

#include <stdarg.h>

#include "sapphire.h"

#include "datetime.h"

#include "ssd1306.h"

#include "fonts.h"

#define DEFAULT_CONTRAST 0x8F

// static uint8_t current_dimmer = DEFAULT_CONTRAST;
static uint8_t target_dimmer = DEFAULT_CONTRAST;

static bool invert;
static uint16_t debug;

KV_SECTION_META kv_meta_t ssd1306_kv[] = {
    {CATBUS_TYPE_BOOL,      0, KV_FLAGS_PERSIST, 0,    0, "ssd1306_enable"},   
};

KV_SECTION_OPT kv_meta_t ssd1306_opt_kv[] = {
    {CATBUS_TYPE_UINT8,     0, 0, &target_dimmer,           0, "ssd1306_dimmer"},   
    {CATBUS_TYPE_UINT16,    0, 0, &debug,                   0, "ssd1306_debug"},   
    {CATBUS_TYPE_BOOL,      0, KV_FLAGS_PERSIST, &invert,   0, "ssd1306_invert"},   
};

#define BUF_SIZE 64

static uint8_t lcd_width = 128;
static uint8_t lcd_height = 32;

static uint8_t disp_buf[128 * 32 / 8];

static uint16_t cursor_x, cursor_y;
static uint8_t current_font;

static void command_n( uint8_t cmd, uint8_t *data, uint16_t len ){
    
    ASSERT( len <= BUF_SIZE );

    uint8_t buf[BUF_SIZE + 2];

    buf[0] = 0;
    buf[1] = cmd;
    memcpy( &buf[2], data, len );

    i2c_v_write( SSD1306_I2C_ADDR, buf, len + 2 );
}

static void command( uint8_t cmd ){

    command_n( cmd, 0, 0 );
}

static void command1( uint8_t cmd, uint8_t data ){

    command_n( cmd, &data, sizeof(data) );
}

static void command2( uint8_t cmd, uint8_t data1, uint8_t data2 ){

    uint8_t data[2] = {
        data1,
        data2
    };

    command_n( cmd, data, sizeof(data) );
}


static void data_n( uint8_t *data, uint8_t len ){
    
    // ASSERT( len <= BUF_SIZE );

    uint8_t buf[512 + 1];

    buf[0] = 0x40;
    memcpy( &buf[1], data, len );

    i2c_v_write( SSD1306_I2C_ADDR, buf, len + 1 );
}

static void refresh_display( void ){

    command2( SSD1306_CMD_SET_PAGE_ADDR, 0x00, lcd_height - 1 );
    command2( SSD1306_CMD_SET_COL_ADDR, 0x00, lcd_width - 1 );

    data_n( &disp_buf[0],   128 );
    data_n( &disp_buf[128], 128 );
    data_n( &disp_buf[256], 128 );
    data_n( &disp_buf[384], 128 );
}

static void write_pixel( uint16_t x, uint16_t y, uint8_t val ){

    if( x >= lcd_width ){

        return;
    }

    if( y >= lcd_height ){

        return;
    }

    if( invert ){

        x = ( lcd_width - 1 ) - x;
        y = ( lcd_height - 1 ) - y;
    }

    if( val != 0 ){

        val = 1;
    }

    uint16_t index = x + ( y / 8 ) * lcd_width;
    uint8_t byte = ( val << ( y % 8 ) );

    if( val == 0 ){

        disp_buf[index] &= ~byte;
    }
    else{

        disp_buf[index] |= byte;
    }
}


void render_character( uint8_t c ){

    glyph_info_t info = fonts_g_get_glyph( current_font, c );

    uint8_t *bitmap = info.bitmap;
    uint8_t bit = 0;
    uint8_t bitmap_index = 0;
    uint8_t byte = bitmap[bitmap_index];
            
    for( uint8_t y = 0; y < info.height_px; y++ ){
        for( uint8_t x = 0; x < info.width_px; x++ ){
            
            uint8_t val = 0;
            if( byte & 0x80 ){

                val = 1;
            }

            write_pixel( x + cursor_x + info.x_offset, y + cursor_y + info.y_offset + info.y_advance, val );
            
            // trace_printf("X: %d Y: %d bit: %d idx: %d val: %d\r\n", x, y, bit, bitmap_index, val);

            byte <<= 1;
            bit += 1;
            if( bit % 8 == 0 ){

                bitmap_index++;
                byte = bitmap[bitmap_index];
            }
        } 
    }

    cursor_x += info.x_advance;

    if( cursor_x >= lcd_width ){

        cursor_y += info.y_advance;

        if( cursor_y >= lcd_height ){

            cursor_y %= lcd_height;
        }

        cursor_x = 0;
    }
}

void ssd1306_v_set_cursor( uint8_t x, uint8_t y ){

    cursor_x = x;
    cursor_y = y;
}

void ssd1306_v_write_char( char c ){

    if( c == '\n' ){

        glyph_info_t info = fonts_g_get_glyph( current_font, c );

        cursor_y += info.y_advance;
        cursor_x = 0;

        return;
    }

    render_character( c );
}

void ssd1306_v_write_str( char *s ){

    while( *s != 0 ){

        ssd1306_v_write_char( *s );

        s++;
    }
}

void ssd1306_v_printf( char * format, ... ){

    va_list ap;

    // parse variable arg list
    va_start( ap, format );

    char buf[128];

    // print string
    uint32_t len = vsnprintf_P( buf, sizeof(buf), format, ap );

    va_end( ap );

    buf[len] = 0;

    ssd1306_v_write_str( buf );
}

void ssd1306_v_clear( void ){

    memset( disp_buf, 0, sizeof(disp_buf) );
}

void ssd1306_v_home( void ){

    cursor_x = 0;
    cursor_y = 0;
}

void ssd1306_v_set_contrast( uint8_t contrast ){

    command1( SSD1306_CMD_CONTRAST, contrast );   
}

PT_THREAD( ssd1306_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    // send first command twice, the first write seems to consistently fail
    command(  SSD1306_CMD_DISP_OFF );
    command(  SSD1306_CMD_DISP_OFF );

    command1( SSD1306_CMD_SET_CLOCK_DIV, 0x80 );
    command1( SSD1306_CMD_SET_MUX_RATIO, lcd_height - 1 );
    command1( SSD1306_CMD_SET_DISP_OFFSET, 0 );
    command(  SSD1306_CMD_SET_START_LINE | 0 );
    command1( SSD1306_CMD_SET_CHARGE_PUMP, 0x14 ); // on
    // command1( SSD1306_CMD_SET_CHARGE_PUMP, 0x10 ); // off
    command1( SSD1306_CMD_MEM_MODE, 0x00 );
    command(  SSD1306_CMD_SET_SEG_REMAP | 1 );
    command(  SSD1306_CMD_SET_COMSCAN_DEC );
    command1( SSD1306_CMD_SET_COM_PINS, 0x02 );
    // command1( SSD1306_CMD_SET_COM_PINS, 0x12 );
    command1( SSD1306_CMD_CONTRAST, DEFAULT_CONTRAST );
    command1( SSD1306_CMD_SET_PRECHARGE, 0xF1 ); // if charge pump is ON
    // command1( SSD1306_CMD_SET_PRECHARGE, 0x22 ); // if charge pump is OFF
    command1( SSD1306_CMD_SET_VCOM_LEVEL, 0x40 );
    command(  SSD1306_CMD_DISP_USE_RAM );
    // command(  SSD1306_CMD_DISP_ALL_ON );
    command(  SSD1306_CMD_DISP_NORMAL );
    // command(  SSD1306_CMD_DISP_INVERT );
    command(  SSD1306_CMD_DISP_ON );

    // disp_buf[0] = 0x01;
    // disp_buf[1] = 0x80;
    // disp_buf[510] = 0xFF;
    // disp_buf[511] = 0x81;


    // for( uint16_t i = 0; i < sizeof(disp_buf); i++ ){

    //     disp_buf[i] = i;
    // }

    // disp_buf[0] = 0xFD;

    // disp_buf[4] = 0x52;
    // disp_buf[3] = 0xBE;
    // disp_buf[2] = 0xA5;
    // disp_buf[1] = 0x7D;
    // disp_buf[0] = 0x4A;

    
    cursor_x = 0;
    cursor_y = 0;

    // disp_buf[511] = 0xFD;

    // write_pixel( 0, 0, 1 );
    // write_pixel( 8, 7, 1 );

    // write_pixel( 0, 8, 1 );

    // write_pixel( 2, 0, 1 );
    // render_character( 'A' );
    // render_character( 'B' );
    // render_character( 'C' );
    // render_character( '$' );
    // render_character( '!' );
    // write_pixel( 0, 0, 1 );
    // write_pixel( 0, 1, 1 );
    // write_pixel( 0, 2, 1 );
    // write_pixel( 0, 3, 1 );
    // write_pixel( 0, 4, 1 );
    // write_pixel( 0, 5, 1 );
    // write_pixel( 0, 6, 0 );
    // write_pixel( 0, 7, 1 );

    // ssd1306_v_write_str("Jeremy Rocks!");

    
    refresh_display();


    while( TRUE ){

        TMR_WAIT( pt, 100 );

        // if( ( current_dimmer == 0 ) && ( target_dimmer == 0 ) ){
        if( target_dimmer == 0 ){

            ssd1306_v_clear();
            refresh_display();

            continue;
        }

        ssd1306_v_home();
        ssd1306_v_clear();

        char iso8601[ISO8601_STRING_MIN_LEN];
        ntp_ts_t ntp = ntp_t_local_now();
        datetime_t now;
        datetime_v_seconds_to_datetime( ntp.seconds, &now );
        datetime_v_to_iso8601( iso8601, sizeof(iso8601), &now );

        ssd1306_v_printf("%4d   RSSI: %2d\n%s", debug, wifi_i8_rssi(), iso8601);

        ssd1306_v_set_contrast( target_dimmer );

        refresh_display();



        // if( target_dimmer > current_dimmer ){

        //     current_dimmer += 4;

        //     if( target_dimmer < current_dimmer ){

        //         current_dimmer = target_dimmer;
        //     }

        //     ssd1306_v_set_contrast( current_dimmer );
        // }
        // else if( target_dimmer < current_dimmer ){

        //     current_dimmer += 4;

        //     if( target_dimmer > current_dimmer ){

        //         current_dimmer = target_dimmer;
        //     }

        //     ssd1306_v_set_contrast( current_dimmer );
        // }
    }

PT_END( pt );	
}


void ssd1306_v_init( void ){

    if( !kv_b_get_boolean( __KV__ssd1306_enable ) ){

        return;
    }
    
    kv_v_add_db_info( ssd1306_opt_kv, sizeof(ssd1306_opt_kv) );

    i2c_v_init( I2C_BAUD_400K );

    thread_t_create( ssd1306_thread,
                     PSTR("ssd1306"),
                     0,
                     0 );
}

