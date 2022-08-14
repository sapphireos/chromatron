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
 
#include "cpu.h"

#include "loader.h"
#include "bootcore.h"
#include "target.h"
#include "eeprom.h"
#include "serial.h"
#include "system.h"


extern const char sw_id[7];

static uint8_t buf[PAGE_SIZE];

static uint32_t addr;

static bool timeout_enable;
static bool timeout;


void init_serial( void ){

    // 115200 bps at 16 MHz with double speed bit set.
    UBRR0 = 16;
    //UBRR0 = 103; // 19200

    UCSR0A = ( 1 << U2X0 ); // enable double speed mode
    UCSR0B = ( 1 << RXEN0 ) | ( 1 << TXEN0 );
    UCSR0C = ( 1 << UCSZ01 ) | ( 1 << UCSZ00 );
    
}

uint8_t receive_char( void ){
    
    if( timeout ){
        
        return 0;
    }

    uint32_t count = 0;

    // wait for character
    while( ( UCSR0A & ( 1 << RXC0 ) ) == 0 ){
            
        count++;

        if( count > SERIAL_TIMEOUT_COUNT ){
            
            if( timeout_enable ){
                
                timeout = TRUE;

                return 0;
            }
        }
    }
    
    return UDR0;
}

void send_char( uint8_t c ){
    
    // wait for data register empty
    while( ( UCSR0A & ( 1 << UDRE0 ) ) == 0 );
    
    UDR0 = c;
}


// serial command processor loop
void serial_v_loop( bool _timeout ){
    
    timeout_enable = _timeout;
    timeout = FALSE;

    init_serial();
    
    while( !timeout ){
        
        ldr_v_clear_green_led();

        uint8_t cmd = receive_char();
        
        ldr_v_set_green_led();

        if( cmd == CMD_ENTER_PROG_MODE ){
            
            send_char( 13 ); 
        }
        else if( cmd == CMD_AUTO_INC_ADDR ){
            
            send_char( 'Y' ); // respond with yes
        }
        else if( cmd == CMD_SET_ADDR ){

            addr = receive_char() << 8;
            addr |= receive_char();

            send_char( 13 );
        }/*
        else if( cmd == CMD_WRITE_PROGMEM_LOW ){
            
            receive_char();

            send_char( 13 );
        }
        else if( cmd == CMD_WRITE_PROGMEM_HIGH ){
            
            receive_char();

            send_char( 13 );
        }
        else if( cmd == CMD_ISSUE_PAGE_WRITE ){
            
            send_char( 13 );
        }
        */
        //else if( cmd == CMD_READ_LOCK_BITS ){
        //}
        /*
        else if( cmd == CMD_READ_PROGMEM ){
            
            send_char( 0 );
            send_char( 0 );
        }
        else if( cmd == CMD_READ_DATAMEM ){
            
            send_char( 0 );
        }
        else if( cmd == CMD_WRITE_DATA_MEM ){
            
            receive_char();
            
            send_char( 13 );
        }
        */
        else if( cmd == CMD_CHIP_ERASE ){
            
            ldr_v_erase_app();

            send_char( 13 );
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
            
            send_char( 13 );
        }
        else if( cmd == CMD_SELECT_DEVICE_TYPE ){
            
            receive_char();

            send_char( 13 );
        }
        else if( cmd == CMD_READ_SIGNATURE ){
            
            send_char( 0x01 );
            send_char( 0xA7 );
            send_char( 0x1E );
        }
        else if( cmd == CMD_READ_DEVICE_CODES ){
            
            send_char( 0x00 );
        }
        else if( cmd == CMD_READ_SW_ID ){
            
            for( uint8_t i = 0; i < sizeof(sw_id); i++ ){
                
                send_char( sw_id[i] );
            }
        }
        else if( cmd == CMD_READ_SW_VERSION ){
            
            send_char( LDR_VERSION_MAJOR );
            send_char( LDR_VERSION_MINOR );
        }
        else if( cmd == CMD_READ_HW_VERSION ){
            // this command is not documented in the AVR109 app note,
            // but AVRDUDE queries for it.

            send_char( '1' );
            send_char( '0' );
        }
        else if( cmd == CMD_READ_PROG_TYPE ){
        
            send_char( 'S' ); // serial
        }
        /*
        else if( cmd == CMD_SET_LED ){
        }
        else if( cmd == CMD_CLEAR_LED ){
        }
        */
        else if( cmd == CMD_EXIT_BOOTLOADER ){
            
            send_char( 13 );

            // run app
            return;
        }
        else if( cmd == CMD_CHECK_BLOCK_SUPPORT ){
             
            send_char( 'Y' ); // respond with yes
            send_char( PAGE_SIZE >> 8 );
            send_char( PAGE_SIZE & 0xff );
        }
        else if( cmd == CMD_START_BLOCK_LOAD ){
            
            uint16_t len;
            len = receive_char() << 8;
            len |= receive_char();
            
            uint8_t type = receive_char();

            for( uint16_t i = 0; i < len; i++ ){
                
                buf[i] = receive_char();
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
        
            send_char( 13 );
        }
        else if( cmd == CMD_START_BLOCK_READ ){

            uint16_t len;
            len = receive_char() << 8;
            len |= receive_char();
            
            uint8_t type = receive_char();
            
            for( uint16_t i = 0; i < len; i++ ){
                
                if( type == 'F' ){
                    
                    send_char( pgm_read_byte_far( addr ) );
                    addr++;
                }
                else if( type == 'E' ){
                    
                    send_char( ee_u8_read_byte( addr ) );
                    addr++;
                }
            }
        }
        // unknown command
        else{
            
            send_char( '?' );
        }
    }
}











