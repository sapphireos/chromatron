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

#ifndef _SSD1306_H_
#define _SSD1306_H_

#define SSD1306_I2C_ADDR	0x3C


#define SSD1306_CMD_CONTRAST		0x81
#define SSD1306_CMD_DISP_USE_RAM	0xA4
#define SSD1306_CMD_DISP_ALL_ON		0xA5
#define SSD1306_CMD_DISP_NORMAL		0xA6
#define SSD1306_CMD_DISP_INVERT		0xA7
#define SSD1306_CMD_DISP_OFF		0xAE
#define SSD1306_CMD_DISP_ON 		0xAF
#define SSD1306_CMD_MEM_MODE		0x20
#define SSD1306_CMD_SET_CLOCK_DIV	0xD5
#define SSD1306_CMD_SET_DISP_OFFSET	0xD3
#define SSD1306_CMD_SET_MUX_RATIO	0xA8
#define SSD1306_CMD_SET_PRECHARGE	0xD9
#define SSD1306_CMD_SET_COM_PINS	0xDA
#define SSD1306_CMD_SET_CHARGE_PUMP	0x8D
#define SSD1306_CMD_SET_SEG_REMAP   0xA0
#define SSD1306_CMD_SET_START_LINE  0x40
#define SSD1306_CMD_SET_COMSCAN_INC 0xC0
#define SSD1306_CMD_SET_COMSCAN_DEC 0xC8
#define SSD1306_CMD_SET_COM_PINS 	0xDA
#define SSD1306_CMD_SET_VCOM_LEVEL 	0xDB
#define SSD1306_CMD_SET_PAGE_ADDR   0x22
#define SSD1306_CMD_SET_COL_ADDR    0x21

void ssd1306_v_set_cursor( uint8_t x, uint8_t y );
void ssd1306_v_write_char( char c );
void ssd1306_v_write_str( char *s );
void ssd1306_v_printf( char * format, ... );

void ssd1306_v_clear( void );
void ssd1306_v_home( void );
void ssd1306_v_set_contrast( uint8_t contrast );


void ssd1306_v_init( void );





#endif
