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


#ifndef _NVM_XMEGA128A4U_H
#define _NVM_XMEGA128A4U_H

#include "system.h"

void nvm_v_init( void );
bool nvm_b_busy( void );
void nvm_v_load_flash_buffer( uint32_t addr, uint16_t data );
void nvm_v_write_flash_page( uint32_t addr );
void nvm_v_erase_flash_page( uint32_t addr );

#endif
