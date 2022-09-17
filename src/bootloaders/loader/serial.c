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
#include "cpu.h"

#include "loader.h"
#include "bootcore.h"
#include "eeprom.h"
#include "serial.h"
#include "boot_serial.h"



extern const char sw_id[7];

static uint8_t buf[PAGE_SIZE];

static uint32_t addr;


// serial command processor loop
void serial_v_loop( bool _timeout ){

    boot_serial_v_init_serial( _timeout );

    while( !boot_serial_b_timed_out() ){

        uint8_t cmd = boot_serial_u8_receive_char();

        if( cmd == CMD_ENTER_PROG_MODE ){

            boot_serial_v_send_char( 13 );
        }
        else if( cmd == CMD_AUTO_INC_ADDR ){

            boot_serial_v_send_char( 'Y' ); // respond with yes
        }
        else if( cmd == CMD_SET_ADDR ){

            addr = boot_serial_u8_receive_char() << 8;
            addr |= boot_serial_u8_receive_char();

            boot_serial_v_send_char( 13 );
        }/*
        else if( cmd == CMD_WRITE_PROGMEM_LOW ){

            boot_serial_u8_receive_char();

            boot_serial_v_send_char( 13 );
        }
        else if( cmd == CMD_WRITE_PROGMEM_HIGH ){

            boot_serial_u8_receive_char();

            boot_serial_v_send_char( 13 );
        }
        else if( cmd == CMD_ISSUE_PAGE_WRITE ){

            boot_serial_v_send_char( 13 );
        }
        */
        //else if( cmd == CMD_READ_LOCK_BITS ){
        //}
        /*
        else if( cmd == CMD_READ_PROGMEM ){

            boot_serial_v_send_char( 0 );
            boot_serial_v_send_char( 0 );
        }
        else if( cmd == CMD_READ_DATAMEM ){

            boot_serial_v_send_char( 0 );
        }
        else if( cmd == CMD_WRITE_DATA_MEM ){

            boot_serial_u8_receive_char();

            boot_serial_v_send_char( 13 );
        }
        */
        else if( cmd == CMD_CHIP_ERASE ){

            ldr_v_erase_app();

            boot_serial_v_send_char( 13 );
        }
        /*
        else if( cmd == CMD_WRITE_LOCK_BITS ){
        }
        else if( cmd == CMD_READ_FUSE_BITS ){
        }
        else if( cmd == CMD_READ_HIGH_FUSE_BITS ){
        }
        else if( cmd == CMD_READ_EXT_FUSE_BITS ){
        }
        */
        else if( cmd == CMD_LEAVE_PROG_MODE ){

            boot_serial_v_send_char( 13 );
        }
        else if( cmd == CMD_SELECT_DEVICE_TYPE ){

            boot_serial_u8_receive_char();

            boot_serial_v_send_char( 13 );
        }
        else if( cmd == CMD_READ_SIGNATURE ){

            boot_serial_v_send_char( 0x01 );
            boot_serial_v_send_char( 0xA7 );
            boot_serial_v_send_char( 0x1E );
        }
        else if( cmd == CMD_READ_DEVICE_CODES ){

            boot_serial_v_send_char( 0x00 );
        }
        else if( cmd == CMD_READ_SW_ID ){

            for( uint8_t i = 0; i < sizeof(sw_id); i++ ){

                boot_serial_v_send_char( sw_id[i] );
            }
        }
        else if( cmd == CMD_READ_SW_VERSION ){

            boot_serial_v_send_char( LDR_VERSION_MAJOR );
            boot_serial_v_send_char( LDR_VERSION_MINOR );
        }
        else if( cmd == CMD_READ_HW_VERSION ){
            // this command is not documented in the AVR109 app note,
            // but AVRDUDE queries for it.

            boot_serial_v_send_char( '1' );
            boot_serial_v_send_char( '0' );
        }
        else if( cmd == CMD_READ_PROG_TYPE ){

            boot_serial_v_send_char( 'S' ); // serial
        }
        else if( cmd == CMD_EXIT_BOOTLOADER ){

            boot_serial_v_send_char( 13 );

            // run app
            return;
        }
        else if( cmd == CMD_CHECK_BLOCK_SUPPORT ){

            boot_serial_v_send_char( 'Y' ); // respond with yes
            boot_serial_v_send_char( PAGE_SIZE >> 8 );
            boot_serial_v_send_char( PAGE_SIZE & 0xff );
        }
        else if( cmd == CMD_START_BLOCK_LOAD ){

            uint16_t len;
            len = boot_serial_u8_receive_char() << 8;
            len |= boot_serial_u8_receive_char();

            uint8_t type = boot_serial_u8_receive_char();

            for( uint16_t i = 0; i < len; i++ ){

                buf[i] = boot_serial_u8_receive_char();
            }

            if( type == 'F' ){

                // write page data to app page
                boot_v_write_page( addr / PAGE_SIZE, buf );

                addr += PAGE_SIZE;
            }
            else if( type == 'E' ){

                ee_v_write_block( addr, buf, len );

                addr += len;
            }

            boot_serial_v_send_char( 13 );
        }
        else if( cmd == CMD_START_BLOCK_READ ){

            uint16_t len;
            len = boot_serial_u8_receive_char() << 8;
            len |= boot_serial_u8_receive_char();

            uint8_t type = boot_serial_u8_receive_char();

            for( uint16_t i = 0; i < len; i++ ){

                if( type == 'F' ){

                    boot_serial_v_send_char( pgm_read_byte_far( addr ) );
                    addr++;
                }
                else if( type == 'E' ){

                    boot_serial_v_send_char( ee_u8_read_byte( addr ) );
                    addr++;
                }
            }
        }
        // unknown command
        else{

            boot_serial_v_send_char( '?' );
        }
    }
}
