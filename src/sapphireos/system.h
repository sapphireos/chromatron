/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2019  Jeremy Billheimer
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
#include "cnt_of_array.h"

//#define ENABLE_MEMORY_DUMP
// #define ATOMIC_TIMING
// #define EXPERIMENTAL_ATOMIC

// no user selectable options exist below this line!

#ifndef FW_INFO_SECTION
    #define FW_INFO_SECTION
#endif


#define FW_ID_LENGTH 16
#define OS_NAME_LEN  128
#define OS_VER_LEN  16
#define FW_NAME_LEN  128
#define FW_VER_LEN  16

typedef struct __attribute__((packed)){
    uint32_t fw_length;
    uint8_t fwid[FW_ID_LENGTH];
    char os_name[OS_NAME_LEN];
    char os_version[OS_VER_LEN];
    char firmware_name[FW_NAME_LEN];
    char firmware_version[FW_VER_LEN];
} fw_info_t;


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

#define SYS_RECOVERY_BOOT_COUNT         5

#ifndef ATOMIC
    #define ATOMIC
#endif
#ifndef END_ATOMIC
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

#ifdef BOOTLOADER
    #undef INCLUDE_ASSERTS
#endif

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
    #define assert(str_expr, file, line)
#endif

void sys_v_init( void );
void sys_v_check_io_for_safe_mode( void );
bool sys_b_is_recovery_mode( void );
void sys_v_check_recovery_mode( void );
bool sys_b_brownout( void );

sys_mode_t8 sys_u8_get_mode( void );
loader_status_t8 sys_u8_get_loader_status( void );

void sys_v_get_fw_id( uint8_t id[FW_ID_LENGTH] );
void sys_v_get_os_version( char ver[OS_VER_LEN] );
void sys_v_get_fw_version( char ver[FW_VER_LEN] );
void sys_v_get_fw_info( fw_info_t *fw_info );
uint32_t sys_v_get_fw_length( void );

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

#endif
