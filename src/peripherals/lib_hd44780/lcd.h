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
#ifndef _LCD_H
#define _LCD_H

#include "sapphire.h"

#define LCD_RS_PIN          1
#define LCD_ENABLE_PIN      2
#define LCD_ENABLE2_PIN     3
#define LCD_DATA_PIN_D4     4
#define LCD_DATA_PIN_D5     5
#define LCD_DATA_PIN_D6     6
#define LCD_DATA_PIN_D7     7

// strap RW pin to ground for write only


/*#if (LCD_SIZE_Y > 2 )    
    #define LCD_ENABLE_2_PRESENT
#endif

#ifdef LCD_ENABLE_2_PRESENT
    #define LCD_ENABLE_2_GPIO IO_PIN_3_RXD
#endif*/

void lcd_v_init( uint8_t size_x, uint8_t size_y );

void lcd_v_clear( void );
void lcd_v_write( char *s, uint8_t x, uint8_t y );
void lcd_v_write_P( PGM_P s, uint8_t x, uint8_t y );
void lcd_v_cursor( uint8_t x, uint8_t y );
void lcd_v_printf_P( uint8_t x, uint8_t y, PGM_P format, ... );

#endif
