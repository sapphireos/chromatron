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

#ifndef _HAL_FLASH25_H
#define _HAL_FLASH25_H


#define CHIP_ENABLE()       FLASH_CS_PORT &= ~_BV( FLASH_CS_PIN )
#define CHIP_DISABLE()      FLASH_CS_PORT |= _BV( FLASH_CS_PIN )

#define WRITE_PROTECT()     FLASH_WP_PORT &= ~_BV( FLASH_WP_PIN )
#define WRITE_UNPROTECT()   FLASH_WP_PORT |= _BV( FLASH_WP_PIN )

#define AAI_STATUS()        ( SPI_PIN & _BV(SPI_MISO) )


void hal_flash25_v_init( void );


#endif
