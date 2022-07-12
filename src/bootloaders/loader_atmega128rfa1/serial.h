/* 
 * <license>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This file is part of the Sapphire Operating System
 *
 * Copyright 2013 Sapphire Open Systems
 *
 * </license>
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

