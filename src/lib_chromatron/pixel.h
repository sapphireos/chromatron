// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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

#ifndef _PIXEL_H_
#define _PIXEL_H_

#include "graphics.h"
#include "keyvalue.h"
#include "pix_modes.h"


#define PIX_DMA_BUF_SIZE 192


#define PIXEL_EN_PORT           PORTA
#define PIXEL_EN_PIN            7

// PWM 2
#define PIX_CLK_PORT            PORTC
#define PIX_CLK_PIN             1

// PWM 4
#define PIX_DATA_PORT           PORTC
#define PIX_DATA_PIN            3

#define PIXEL_DATA_PORT             USARTC0

// DMA
#define PIXEL_DMA_CH_A              CH0
#define PIXEL_DMA_CH_A_TRNIF_FLAG   DMA_CH0TRNIF_bm
#define PIXEL_DMA_CH_A_vect         DMA_CH0_vect

#define PIXEL_DMA_CH_B              CH1
#define PIXEL_DMA_CH_B_TRNIF_FLAG   DMA_CH1TRNIF_bm
#define PIXEL_DMA_CH_B_vect         DMA_CH1_vect

// Timer
#define PIXEL_TIMER                 GFX_TIMER
#define PIXEL_TIMER_CC              CCD
#define PIXEL_TIMER_CC_VECT         GFX_TIMER_CCD_vect
#define PIXEL_TIMER_CC_INTLVL       TC_CCDINTLVL_HI_gc

#define PIXEL_USART_DMA_TRIG        DMA_CH_TRIGSRC_USARTC0_DRE_gc


void pixel_v_init( void );

void pixel_v_set_analog_rgb( uint16_t r, uint16_t g, uint16_t b );

bool pixel_b_enabled( void );

uint8_t pixel_u8_get_mode( void );

void pixel_v_load_rgb(
    uint16_t index,
    uint16_t len,
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    uint8_t *d );

void pixel_v_load_rgb16(
    uint16_t index,
    uint16_t r,
    uint16_t g,
    uint16_t b );

void pixel_v_load_hsv(
    uint16_t index,
    uint16_t len,
    uint16_t *h,
    uint16_t *s,
    uint16_t *v );

void pixel_v_get_rgb_totals( uint16_t *r, uint16_t *g, uint16_t *b );

// reset all arrays to 0
void pixel_v_clear( void );

#endif
