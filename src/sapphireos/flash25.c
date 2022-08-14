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

#include "system.h"
#include "flash_fs_partitions.h"
#include "flash25.h"
#include "hal_flash25.h"

uint16_t block0_unlock;


void flash25_v_init( void ){

    #ifdef __SIM__

    flash25_v_erase_chip();

    #else

    hal_flash25_v_init();
    
    #endif
}

uint8_t flash25_u8_read_mfg_id( void ){

	flash25_device_info_t info;

	flash25_v_read_device_info( &info );

	return info.mfg_id;
}

// returns capacity in bytes
uint32_t flash25_u32_read_capacity_from_info( void ){

	flash25_device_info_t info;

	flash25_v_read_device_info( &info );

    trace_printf("Flash ID: Mfg: 0x%02x ID1: 0x%02x ID2: 0x%02x\r\n", info.mfg_id, info.dev_id_1, info.dev_id_2);

	uint32_t capacity = 0;

	switch( info.mfg_id ){

        // need to make all of this a lookup table
        case FLASH_MFG_SST:

			if( info.dev_id_1 == FLASH_DEV_ID1_SST25 ){

				if( info.dev_id_2 == FLASH_DEV_ID2_SST25_4MBIT ){

					capacity = 524288;
				}
                else if( info.dev_id_2 == FLASH_DEV_ID2_SST25_8MBIT ){

					capacity = 1048576;
				}
			}

			break;

        case FLASH_MFG_ST:
        case FLASH_MFG_WINBOND:
        case FLASH_MFG_BERG:
        case FLASH_MFG_GIGADEVICE:
        case FLASH_MFG_BOYA_BOHONG:

			if( info.dev_id_1 == FLASH_DEV_ID1_WINBOND_x30 ){

                if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_4MBIT ){

                    capacity = 524288;
                }
            }
            else if( info.dev_id_1 == FLASH_DEV_ID1_WINBOND_x40 ){

                if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_8MBIT ){

                    capacity = 1048576;
                }
                else if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_16MBIT ){

                    capacity = 2097152;
                }
                else if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_32MBIT ){

                    capacity = 4194304;
                }
                else if( info.dev_id_2 == FLASH_DEV_ID2_WINBOND_64MBIT ){

                    capacity = 8388608;
                }
            }

            break;

        case FLASH_MFG_CYPRESS:

            if( info.dev_id_2 == FLASH_DEV_ID2_CYPRESS_16MBIT ){

                capacity = 2097152;
            }

            break;

		default:
			break;
	}

	return capacity;
}

void flash25_v_unlock_block0( void ){

    block0_unlock = BLOCK0_UNLOCK_CODE;
}

