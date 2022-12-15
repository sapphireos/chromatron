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

#ifndef _LED_DETECT_H
#define _LED_DETECT_H


#define LED_UNIT_TYPE_NONE          0
#define LED_UNIT_TYPE_STRAND50      1


typedef struct{
    uint8_t unit_type;
    uint8_t led_type;
    uint16_t pix_count;
    uint16_t pix_size_x;
    uint16_t pix_size_y;
} led_profile_t;

typedef struct{
    uint64_t unit_id;
    uint8_t unit_type;
} led_unit_t;

void led_detect_v_init( void );

void led_detect_v_run_detect( void );
bool led_detect_b_led_connected( void );

#endif
