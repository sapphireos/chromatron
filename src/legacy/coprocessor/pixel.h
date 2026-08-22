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

#ifndef _PIXEL_H_
#define _PIXEL_H_

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

// #define PIXEL_DATA_PORT             USARTC0 // moved to hal_usart.h

// DMA
#define PIXEL_DMA_CH_A              CH0
#define PIXEL_DMA_CH_A_TRNIF_FLAG   DMA_CH0TRNIF_bm
#define PIXEL_DMA_CH_A_vect         DMA_CH0_vect

#define PIXEL_DMA_CH_B              CH1
#define PIXEL_DMA_CH_B_TRNIF_FLAG   DMA_CH1TRNIF_bm
#define PIXEL_DMA_CH_B_vect         DMA_CH1_vect

// Timer
#define PIXEL_TIMER                 TCD0
#define PIXEL_TIMER_CC              CCD
#define PIXEL_TIMER_CC_VECT         TCD0_CCD_vect
#define PIXEL_TIMER_CC_INTLVL       TC_CCDINTLVL_HI_gc

#define PIXEL_USART_DMA_TRIG        DMA_CH_TRIGSRC_USARTC0_DRE_gc


void pixel_v_init( void );

void pixel_v_set_pix_count( uint16_t value );
void pixel_v_set_pix_mode( uint8_t value );
void pixel_v_set_pix_dither( bool value );
void pixel_v_set_pix_clock( uint32_t value );
void pixel_v_set_rgb_order( uint8_t value );
void pixel_v_set_apa102_dimmer( uint8_t value );

uint8_t *pixel_u8p_get_red( void );
uint8_t *pixel_u8p_get_green( void );
uint8_t *pixel_u8p_get_blue( void );
uint8_t *pixel_u8p_get_dither( void );

void pixel_v_enable( void );
void pixel_v_disable( void );

// reset all arrays to 0
void pixel_v_clear( void );

#endif
