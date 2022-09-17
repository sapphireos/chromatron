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

#ifndef _HSV_TO_RGB_H
#define _HSV_TO_RGB_H

// #define ENABLE_BG_CAL

void gfx_v_hsv_to_rgb(
    uint16_t h,
    uint16_t s,
    uint16_t v,
    uint16_t *r,
    uint16_t *g,
    uint16_t *b );

void gfx_v_hsv_to_rgbw(
    uint16_t h,
    uint16_t s,
    uint16_t v,
    uint16_t *r,
    uint16_t *g,
    uint16_t *b,
    uint16_t *w );

void gfx_v_hsv_to_rgbw8(
    uint16_t h,
    uint16_t s,
    uint16_t v,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *w );

#endif