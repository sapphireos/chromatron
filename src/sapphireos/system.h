/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2018  Jeremy Billheimer
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

#ifndef _SYSTEM_H
#define _SYSTEM_H

#include "cpu.h"
#include "boot_data.h"
#include "target.h"
#include "hal_watchdog.h"
#include "hal_cpu.h"
#include "bool.h"

//#define ENABLE_MEMORY_DUMP
// #define ATOMIC_TIMING
// #define EXPERIMENTAL_ATOMIC

// no user selectable options exist below this line!

#ifdef __SIM__
    #define FW_INFO_SECTION
#else
    #define FW_INFO_SECTION __attribute__ ((section (".fwinfo"), used))
#endif

#define FW_ID_LENGTH 16
#define OS_NAME_LEN  128
#define OS_VER_LEN  16
#define FW_NAME_LEN  128
#define FW_VER_LEN  16

typedef struct{
    uint32_t fw_length;
    uint8_t fwid[FW_ID_LENGTH];
    char os_name[OS_NAME_LEN];
    char os_version[OS_VER_LEN];
    char firmware_name[FW_NAME_LEN];
    char firmware_version[FW_VER_LEN];
} fw_info_t;

typedef struct{
	uint64_t serial_number;
	uint64_t mac;
	uint16_t model;
	uint16_t rev;
} hw_info_t;

#define SYS_REBOOT_SAFE         -2
#define SYS_REBOOT_NOAPP        -3 // not yet supported
#define SYS_REBOOT_LOADFW       -4
#define SYS_REBOOT_FORMATFS     -5
#define SYS_REBOOT_RECOVERY     -6
#define SYS_REBOOT_RESET_CONFIG -7

typedef uint8_t sys_mode_t8;
#define SYS_MODE_NORMAL     0
#define SYS_MODE_SAFE       1
#define SYS_MODE_NO_APP     2
#define SYS_MODE_FORMAT     3



typedef uint8_t sys_error_t;
#define SYS_ERR_NONE                    0
#define SYS_ERR_INVALID_HANDLE          1
#define SYS_ERR_HANDLE_UNALLOCATED      2
#define SYS_ERR_INVALID_CANARY          3
#define SYS_ERR_MEM_BLOCK_IS_DIRTY      4

typedef uint32_t sys_warnings_t;
#define SYS_WARN_MEM_FULL               0x0001
#define SYS_WARN_NETMSG_FULL            0x0002
#define SYS_WARN_FLASHFS_FAIL           0x0004
#define SYS_WARN_FLASHFS_HARD_ERROR     0x0008
#define SYS_WARN_CONFIG_FULL            0x0010
#define SYS_WARN_CONFIG_WRITE_FAIL      0x0020
#define SYS_WARN_EVENT_LOG_OVERFLOW     0x0040
#define SYS_WARN_MISSING_KV_INDEX       0x0080
#define SYS_WARN_SYSTEM_ERROR           0x8000


#define FLASH_STRING(x) PSTR(x)
#define FLASH_STRING_T  PGM_P


// Critical Section
//
// Notes:  ATOMIC creates a local copy of SREG, but only copies the I bit.
// END_ATOMIC restores the original value of the SREG I bit, but does not modify any other bits.
// This guarantees interrupts will be disabled within the critical section, but does not
// enable interrupts if they were already disabled.  Since it also doesn't modify any of the
// other SREG bits, it will not disturb instructions after the critical section.
// An example of this would be performing a comparison inside the critical section, and then
// performing a branch immediately thereafter.  If we restored SREG in its entirety, we will
// have destroyed the result of the compare and the code will not execute as intended.
#ifdef AVR

    #ifdef EXPERIMENTAL_ATOMIC
        #define ATOMIC _sys_v_enter_critical()
        #define END_ATOMIC _sys_v_exit_critical(FLASH_STRING( __FILE__ ), __LINE__)
    #else
        #ifndef ATOMIC_TIMING
            #define ATOMIC uint8_t __sreg_i = ( SREG & 0b10000000 ); cli()
            #define END_ATOMIC SREG |= __sreg_i
        #else
            #define ATOMIC uint8_t __sreg_i = ( SREG & 0b10000000 ); cli(); _sys_v_start_atomic_timestamp()
            #define END_ATOMIC SREG |= __sreg_i; _sys_v_end_atomic_timestamp()
        #endif
    #endif
#else
    #define ATOMIC
    #define END_ATOMIC
#endif

#ifdef __SIM__
    #define __SOURCE_FILE__ PSTR(__FILE__)
#else
    // define filename constant for use in asserts and other things.
    // use this instead of __FILE__, so the compile doesn't include
    // the file name string multiple times.
    static const PROGMEM char __SOURCE_FILE__[] = __BASE_FILE__;
    // use this define to revert to using __FILE__ everywhere.
    // #define __SOURCE_FILE__ PSTR(__FILE__)
#endif

//#undef INCLUDE_COMPILER_ASSERTS
//#undef INCLUDE_ASSERTS
//#define INCLUDE_ASSERT_TEXT

// This setting will allow the system to disable assertion traps at runtime.  Do not enable this option unless there is an extremely good reason for it.
//#define ALLOW_ASSERT_DISABLE

// define compile assert macro
// if the expression evaluates to false
#ifdef INCLUDE_COMPILER_ASSERTS
	#define COMPILER_ASSERT(expression) switch(0) { case 0 : case (expression) : ; }
#else
	#define COMPILER_ASSERT(expression)
#endif

#ifdef INCLUDE_ASSERTS
	#ifdef INCLUDE_ASSERT_TEXT

		#define ASSERT(expr)  if( !(expr) ){  assert( FLASH_STRING( #expr ), __SOURCE_FILE__, __LINE__); }
		#define ASSERT_MSG(expr, str) if( !(expr) ){ assert( FLASH_STRING( #str ), __SOURCE_FILE__, __LINE__); }
	#else

		// #define ASSERT(expr)  if( !(expr) ){  assert( 0, FLASH_STRING( __FILE__ ), __LINE__); }
        #define ASSERT(expr)  if( !(expr) ){  assert( 0, __SOURCE_FILE__, __LINE__); }
		#define ASSERT_MSG(expr, str) ASSERT(expr)
	#endif
	void assert(FLASH_STRING_T str_expr, FLASH_STRING_T file, int line);
#else
	#define ASSERT(expr)
	#define ASSERT_MSG(expr, str)
#endif


// Count of array macro
#define cnt_of_array( array ) ( sizeof( array ) / sizeof( array[0] ) )



void sys_v_init( void );
void sys_v_check_io_for_safe_mode( void );
bool sys_b_brownout( void );

sys_mode_t8 sys_u8_get_mode( void );
loader_status_t8 sys_u8_get_loader_status( void );

void sys_v_get_fw_id( uint8_t id[FW_ID_LENGTH] );
void sys_v_get_os_version( char ver[OS_VER_LEN] );
void sys_v_get_fw_version( char ver[FW_VER_LEN] );
void sys_v_get_fw_info( fw_info_t *fw_info );
uint32_t sys_v_get_fw_length( void );
void sys_v_get_hw_info( hw_info_t *hw_info );

void sys_reboot( void ) __attribute__((noreturn));
void sys_reboot_to_loader( void ) __attribute__((noreturn));
void sys_v_load_fw( void );
void sys_v_load_recovery( void );
void sys_v_reboot_delay( sys_mode_t8 mode );

bool sys_b_shutdown( void );

void sys_v_wdt_reset( void );
void sys_v_init_watchdog( void );
void sys_v_disable_watchdog( void );

void sys_v_get_boot_data( boot_data_t *data );
boot_mode_t8 sys_m_get_boot_mode( void );

#ifdef ALLOW_ASSERT_DISABLE
void sys_v_enable_assertion_trap( void );
void sys_v_disable_assertion_trap( void );
void sys_v_reset_assertion_trap( void );
bool sys_b_asserted( void );
#endif

void sys_v_set_error( sys_error_t error );

void sys_v_set_warnings( sys_warnings_t flags );
sys_warnings_t sys_u32_get_warnings( void );

void sys_v_enable_interrupts( void );
void sys_v_disable_interrupts( void );

#ifdef EXPERIMENTAL_ATOMIC
void _sys_v_enter_critical( void );
void _sys_v_exit_critical( FLASH_STRING_T file, int line  );
#endif

#ifdef ATOMIC_TIMING
void _sys_v_start_atomic_timestamp( void );
void _sys_v_end_atomic_timestamp( void );
#endif

#endif
