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
 
#ifndef _SERIAL_H
#define _SERIAL_H

#define SERIAL_TIMEOUT_COUNT        7150000 // about an 8 second delay at 16 MHz

// AVR109 commands:
// not all are implemented
#define CMD_ENTER_PROG_MODE         'P'
#define CMD_AUTO_INC_ADDR           'a'
#define CMD_SET_ADDR                'A'
#define CMD_WRITE_PROGMEM_LOW       'c'
#define CMD_WRITE_PROGMEM_HIGH      'C'
#define CMD_ISSUE_PAGE_WRITE        'm'
#define CMD_READ_LOCK_BITS          'r'
#define CMD_READ_PROGMEM            'R'
#define CMD_READ_DATAMEM            'd'
#define CMD_WRITE_DATA_MEM          'D'
#define CMD_CHIP_ERASE              'e'
#define CMD_WRITE_LOCK_BITS         'I'
#define CMD_READ_FUSE_BITS          'F'
#define CMD_READ_HIGH_FUSE_BITS     'N'
#define CMD_READ_EXT_FUSE_BITS      'Q'
#define CMD_LEAVE_PROG_MODE         'L'
#define CMD_SELECT_DEVICE_TYPE      'T'
#define CMD_READ_SIGNATURE          's'
#define CMD_READ_DEVICE_CODES       't'
#define CMD_READ_SW_ID              'S'
#define CMD_READ_SW_VERSION         'V'
#define CMD_READ_HW_VERSION         'v'
#define CMD_READ_PROG_TYPE          'p'
#define CMD_SET_LED                 'x'
#define CMD_CLEAR_LED               'y'
#define CMD_EXIT_BOOTLOADER         'E'
#define CMD_CHECK_BLOCK_SUPPORT     'b'
#define CMD_START_BLOCK_LOAD        'B'
#define CMD_START_BLOCK_READ        'g'


void serial_v_loop( bool _timeout );

#endif

