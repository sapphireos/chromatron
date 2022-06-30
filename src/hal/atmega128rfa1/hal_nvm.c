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

#include "system.h"
#include "hal_cpu.h"
#include "hal_nvm.h"


void load_flash_page_buffer( uint32_t addr, uint16_t data ){

    // addr is passed as:
    // B3  B2  B1  B0
    // r25 r24 r23 r22
    //
    // data is passed:
    // B1  B0
    // r21 r20

    // asm volatile(
    //     // save registers
    //     "push r31\n" // ZH
    //     "push r30\n" // ZL
    //     "in r27, %0\n"
    //     "push r27\n"

    //     // load high byte (bits 16 to 23) of 'addr' into RAMPZ
    //     "out %0, r24\n"
    //     // move lower 16 bits to Z
    //     "movw r30, r22\n"

    //     // save NVM_CMD in r24
    //     "lds r24, %1\n"

    //     // load the load flash buffer command
    //     "ldi r18, %2\n"

    //     // save command to NVM_CMD
    //     "sts %1, r18\n"

    //     // move 'data' to r0:r1
    //     "movw r0, r20\n"

    //     // execute!
    //     "spm\n"

    //     // GCC assumes r1 to be 0
    //     "clr r1\n"

    //     // restore NVM_CMD
    //     "sts %1, r24\n"

    //     // restore registers
    //     "pop r27\n"
    //     "out %0, r27\n"
    //     "pop r30\n"
    //     "pop r31\n"
    //     :
    //     :"I" (_SFR_IO_ADDR(RAMPZ)), "n" (_SFR_IO_ADDR(NVM_CMD)), "M" (NVM_CMD_LOAD_FLASH_BUFFER_gc)
    //     :"r27"
    // );
}

void nvm_spm_cmd( uint32_t addr, uint8_t cmd ){

    // addr is passed as:
    // B3  B2  B1  B0
    // r25 r24 r23 r22
    //
    // cmd is passed (always start on even registers):
    // B0
    // r20

    // asm volatile(
    //     // save registers
    //     "push r31\n"
    //     "push r30\n"
    //     "in r27, %0\n"
    //     "push r27\n"

    //     // load high byte (bits 16 to 23) of 'addr' into RAMPZ
    //     "out %0, r24\n"
    //     // move lower 16 bits to Z
    //     "movw r30, r22\n"

    //     // save NVM_CMD in r24
    //     "lds r24, %1\n"

    //     // save 'cmd' to NVM_CMD
    //     "sts %1, r20\n"

    //     // load the CCP enable
    //     "ldi r20, %2\n"
    //     // enable CCP
    //     "sts %3, r20\n"

    //     // execute!
    //     "spm\n"

    //     // restore NVM_CMD
    //     "sts %1, r24\n"

    //     // restore registers
    //     "pop r27\n"
    //     "out %0, r27\n"
    //     "pop r30\n"
    //     "pop r31\n"
    //     :
    //     :"I" (_SFR_IO_ADDR(RAMPZ)), "n" (_SFR_IO_ADDR(NVM_CMD)), "M" (CCP_SPM_gc), "I" (_SFR_IO_ADDR(CCP))
    //     :"r27"
    // );
}

// static void exec( void ){
//
//     ATOMIC;
//
//     // enable configuration change
//     ENABLE_CONFIG_CHANGE;
//
//     NVM.CTRLA |= NVM_CMDEX_bm;
//
//     END_ATOMIC;
//
//     NVM.CMD = NVM_CMD_NO_OPERATION_gc;
// }


void nvm_v_init( void ){

    // NVM.CMD = NVM_CMD_NO_OPERATION_gc;

    while( nvm_b_busy() );
}

bool nvm_b_busy( void ){

    // return ( NVM.STATUS & NVM_NVMBUSY_bm ) != 0;
}

void nvm_v_load_flash_buffer( uint32_t addr, uint16_t data ){

    load_flash_page_buffer( addr, data );

    while( nvm_b_busy() );
}

void nvm_v_write_flash_page( uint32_t addr ){

    while( nvm_b_busy() );

    // nvm_spm_cmd( addr, NVM_CMD_WRITE_APP_PAGE_gc );

    while( nvm_b_busy() );
}

void nvm_v_erase_flash_page( uint32_t addr ){

    // nvm_spm_cmd( addr, NVM_CMD_ERASE_APP_PAGE_gc );

    while( nvm_b_busy() );
}
