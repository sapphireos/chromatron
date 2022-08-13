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

#include "system.h"
#include "hal_cpu.h"
#include "hal_nvm.h"

#define WRITE_CHUNK_SIZE 32 // 256 bit writes to flash


void nvm_v_init( void ){

}

bool nvm_b_busy( void ){

    return FALSE;
}


void nvm_v_write_flash_page( uint32_t addr, uint8_t *data ){

	HAL_FLASH_Unlock();
	
	for( uint16_t i = 0; i < PAGE_SIZE; i += WRITE_CHUNK_SIZE ){

		HAL_FLASH_Program( FLASH_TYPEPROGRAM_FLASHWORD, FLASH_START + addr + i, (uint64_t)(uint32_t)data );
		data += WRITE_CHUNK_SIZE;
	}    

    HAL_FLASH_Lock();
}

void nvm_v_erase_app_flash( void ){

	uint32_t error;
    FLASH_EraseInitTypeDef erase;
    erase.TypeErase 		= FLASH_TYPEERASE_SECTORS; 
    erase.Banks             = FLASH_BANK_1; 
    erase.Sector 			= 1; 
    erase.NbSectors 		= 6; 
    erase.VoltageRange 		= FLASH_VOLTAGE_RANGE_3; 

    HAL_FLASH_Unlock();

    HAL_FLASHEx_Erase(&erase, &error);

    HAL_FLASH_Lock();
}


