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


#include "cpu.h"

#include "boot_data.h"
#include "system.h"
#include "config.h"
#include "threading.h"
#include "timers.h"
#include "target.h"
#include "datetime.h"
#include "io.h"
#include "keyvalue.h"
#include "flash25.h"
#include "os_irq.h"
#include "watchdog.h"
#include "hal_cpu.h"
#include "usb_intf.h"

#ifdef ENABLE_WCOM
#include "wcom_neighbors.h"
#endif

#ifdef ENABLE_PRESENCE
#include "presence.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define NO_LOGGING
#include "logging.h"

//#define NO_EVENT_LOGGING
#include "event_log.h"


#ifdef ALLOW_ASSERT_DISABLE
static bool disable_assertions;
static bool asserted;
#endif

static uint8_t reset_source;

static sys_mode_t8 sys_mode;

static sys_error_t sys_error;
static sys_warnings_t warnings;

static uint8_t reboot_delay = 2;

static bool interrupts_enabled = FALSE;

#ifdef EXPERIMENTAL_ATOMIC
static uint8_t atomic_counter;
#endif

#ifdef ATOMIC_TIMING
static bool critical;
static uint64_t atomic_timestamp;
static uint32_t atomic_longest_time;
static FLASH_STRING_T atomic_longest_file;
static int atomic_longest_line;
#endif

#ifndef BOOTLOADER
FW_INFO_SECTION fw_info_t fw_info;
#endif

// bootloader shared memory
extern boot_data_t BOOTDATA boot_data;

// local functions:
void reboot( void ) __attribute__((noreturn));

int8_t sys_kv_reboot_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        // return all 0s
        memset( data, 0, len );
    }
    else if( op == KV_OP_SET ){

        int8_t mode;
        memcpy( &mode, data, sizeof(mode) );

        if( mode > 0 ){

            reboot_delay = mode;
            sys_v_reboot_delay( SYS_MODE_NORMAL );
        }
        else{

            switch( mode ){

                case SYS_REBOOT_SAFE:
                    sys_v_reboot_delay( SYS_MODE_SAFE );
                    break;

                case SYS_REBOOT_LOADFW:
                    sys_v_load_fw();
                    break;

                case SYS_REBOOT_FORMATFS:
                    sys_v_reboot_delay( SYS_MODE_FORMAT );
                    break;

                case SYS_REBOOT_RECOVERY:
                    sys_v_load_recovery();
                    break;

                default:
                    break;
            }
        }
    }

    return 0;
}

int8_t sys_kv_fw_info_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

        fw_info_t fw_info;
        sys_v_get_fw_info( &fw_info );

        memset( data, 0, len );

        if( hash == __KV__firmware_id ){

            memcpy( data, fw_info.fwid, len );
        }
        else if( hash == __KV__firmware_name ){

            strlcpy( data, fw_info.firmware_name, len );
        }
        else if( hash == __KV__firmware_version ){

            strlcpy( data, fw_info.firmware_version, len );
        }
        else if( hash == __KV__os_name ){

            strlcpy( data, fw_info.os_name, len );
        }
        else if( hash == __KV__os_version ){

            strlcpy( data, fw_info.os_version, len );
        }
    }

    return 0;
}

PT_THREAD( sys_reboot_thread( pt_t *pt, void *state ) );


KV_SECTION_META kv_meta_t sys_info_kv[] = {
    { SAPPHIRE_TYPE_INT8,   0, 0,                   0, sys_kv_reboot_handler,            "reboot" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &sys_mode,                       0,  "sys_mode" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &boot_data.boot_mode,            0,  "boot_mode" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &boot_data.loader_version_minor, 0,  "loader_version_minor" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &boot_data.loader_version_major, 0,  "loader_version_major" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &boot_data.loader_status,        0,  "loader_status" },
    { SAPPHIRE_TYPE_UINT32, 0, KV_FLAGS_READ_ONLY,  &warnings,                       0,  "sys_warnings" },
    { SAPPHIRE_TYPE_UINT8,  0, KV_FLAGS_READ_ONLY,  &reset_source,                   0,  "reset_source" },
    #ifdef ATOMIC_TIMING
    { SAPPHIRE_TYPE_UINT32, 0, 0,                   &atomic_longest_time,            0,  "sys_atomic_longest_time" },
    #endif
    { SAPPHIRE_TYPE_KEY128,    0, KV_FLAGS_READ_ONLY,  0, sys_kv_fw_info_handler,          "firmware_id" },
    { SAPPHIRE_TYPE_STRING32,  0, KV_FLAGS_READ_ONLY,  0, sys_kv_fw_info_handler,       "firmware_name" },
    { SAPPHIRE_TYPE_STRING32,  0, KV_FLAGS_READ_ONLY,  0, sys_kv_fw_info_handler,       "firmware_version" },
    { SAPPHIRE_TYPE_STRING32,  0, KV_FLAGS_READ_ONLY,  0, sys_kv_fw_info_handler,       "os_name" },
    { SAPPHIRE_TYPE_STRING32,  0, KV_FLAGS_READ_ONLY,  0, sys_kv_fw_info_handler,       "os_version" },
};

void sys_v_init( void ){

    cpu_v_init();

	// check that boot_data is the right size
	COMPILER_ASSERT( sizeof( boot_data ) <= 8 );

	// increment reboot counter
	boot_data.reboots++;

    // initialize loader command
    boot_data.loader_command = LDR_CMD_NONE;

    #ifndef __SIM__

	// check reset source
	reset_source = cpu_u8_get_reset_source();
    cpu_v_clear_reset_source();

	// power on reset (or JTAG)
	if( ( reset_source & RESET_SOURCE_POWER_ON ) ||  // power on
        ( reset_source & RESET_SOURCE_JTAG ) || // JTAG
        ( reset_source & RESET_SOURCE_EXTERNAL ) || // external reset
        ( reset_source & RESET_SOURCE_BROWNOUT ) ){ // internal BOD

        sys_mode = SYS_MODE_NORMAL;
		boot_data.reboots = 0; // initialize reboot counter
    }
    // check if format requested
    else if( boot_data.boot_mode == BOOT_MODE_FORMAT ){

        sys_mode = SYS_MODE_FORMAT;
    }
	// check if there was reset not caused by a commanded reboot
	else if( boot_data.boot_mode != BOOT_MODE_REBOOT ){

        sys_mode = SYS_MODE_SAFE;
	}
    else{

       sys_mode = SYS_MODE_NORMAL;
    }

    // set boot mode to normal
    boot_data.boot_mode = BOOT_MODE_NORMAL;

    cpu_v_remap_isrs();

	#endif
}

void sys_v_check_io_for_safe_mode( void ){

    if( io_b_button_down() ){

        sys_mode = SYS_MODE_SAFE;
    }
}

bool sys_b_brownout( void ){

    return reset_source & RESET_SOURCE_BROWNOUT;
}

sys_mode_t8 sys_u8_get_mode( void ){

    return sys_mode;
}

loader_status_t8 sys_u8_get_loader_status( void ){

    return boot_data.loader_status;
}

void sys_v_get_boot_data( boot_data_t *data ){

	*data = boot_data;
}

boot_mode_t8 sys_m_get_boot_mode( void ){

	return boot_data.boot_mode;
}

#ifdef ALLOW_ASSERT_DISABLE
void sys_v_enable_assertion_trap( void ){

    disable_assertions = FALSE;
}

void sys_v_disable_assertion_trap( void ){

    disable_assertions = TRUE;
}

void sys_v_reset_assertion_trap( void ){

    asserted = FALSE;
}

bool sys_b_asserted( void ){

    return asserted;
}
#endif

void sys_v_set_error( sys_error_t error ){

    sys_error = error;
}

void sys_v_set_warnings( sys_warnings_t flags ){

    // disabling the log writes,
    // which will fail if called from an IRQ

    // if( !( warnings & flags ) ){

    //     if( flags & SYS_WARN_MEM_FULL ){

    //         log_v_warn_P( PSTR("Mem full") );
    //     }

    //     if( flags & SYS_WARN_NETMSG_FULL ){

    //         log_v_warn_P( PSTR("Netmsg full") );
    //     }

    //     if( flags & SYS_WARN_CONFIG_FULL ){

    //         log_v_warn_P( PSTR("Config full") );
    //     }

    //     if( flags & SYS_WARN_CONFIG_WRITE_FAIL ){

    //         log_v_warn_P( PSTR("Config write fail") );
    //     }
    // }

    ATOMIC;

    warnings |= flags;

    END_ATOMIC;
}

sys_warnings_t sys_u32_get_warnings( void ){

    ATOMIC;

    uint32_t temp = warnings;

    END_ATOMIC;

    return temp;
}

void sys_v_get_fw_id( uint8_t id[FW_ID_LENGTH] ){

    #ifdef __SIM__
        memset( id, 2, FW_ID_LENGTH );
    #else
        memcpy_P( id, (void *)FW_INFO_ADDRESS + offsetof(fw_info_t, fwid), FW_ID_LENGTH );
    #endif
}

void sys_v_get_os_version( char ver[OS_VER_LEN] ){

    #ifdef __SIM__
        memset( id, 2, OS_VER_LEN );
    #else
        memcpy_P( ver, (void *)FW_INFO_ADDRESS + offsetof(fw_info_t, os_version), OS_VER_LEN );
    #endif
}

void sys_v_get_fw_version( char ver[FW_VER_LEN] ){

    #ifdef __SIM__
        memset( id, 2, FW_VER_LEN );
    #else
        memcpy_P( ver, (void *)FW_INFO_ADDRESS + offsetof(fw_info_t, firmware_version), FW_VER_LEN );
    #endif
}

void sys_v_get_fw_info( fw_info_t *fw_info ){

    #ifdef __SIM__

    #else
        memcpy_P( fw_info, (void *)FW_INFO_ADDRESS, sizeof(fw_info_t) );
    #endif
}

uint32_t sys_v_get_fw_length( void ){

    uint32_t length;

    #ifdef __SIM__
        length = 100;
    #else
        memcpy_P( &length, (void *)FW_INFO_ADDRESS + offsetof(fw_info_t, fw_length), sizeof(length) );
    #endif

    return length;
}

void sys_v_get_hw_info( hw_info_t *hw_info ){

    #ifdef ENABLE_FFS
    flash25_v_read( 0, hw_info, sizeof(hw_info_t) );
    #endif
}

// causes a watchdog timeout, which will cause a reset into the bootloader.
// this will request an immediate reboot from the loader.
void sys_reboot( void ){

    boot_data.boot_mode = BOOT_MODE_REBOOT;

	reboot();
}

// immediate reset into bootloader
void sys_reboot_to_loader( void ){

    boot_data.loader_command = LDR_CMD_SERIAL_BOOT;
    boot_data.boot_mode = BOOT_MODE_REBOOT;

	reboot();
}

// reboot with a load firmware command to the bootloader
void sys_v_load_fw( void ){

    boot_data.loader_command = LDR_CMD_LOAD_FW;

    sys_v_reboot_delay( SYS_MODE_NORMAL );
}

// reboot with a load recovery firmware command to the bootloader
void sys_v_load_recovery( void ){

    boot_data.loader_command = LDR_CMD_RECOVERY;

    sys_v_reboot_delay( SYS_MODE_NORMAL );
}

// start reboot delay thread
void sys_v_reboot_delay( sys_mode_t8 mode ){

    if( mode == SYS_MODE_SAFE ){

        // this looks weird.
        // the system module will check the boot mode when it initializes.
        // if it came up from a normal reboot, it expects to see reboot set.
        // if normal mode, it assumes an unexpected reboot, which causes it
        // to go in to safe mode.
        boot_data.boot_mode = BOOT_MODE_NORMAL;
    }
    else if( mode == SYS_MODE_FORMAT ){

        boot_data.boot_mode = BOOT_MODE_FORMAT;
    }
    else{

        boot_data.boot_mode = BOOT_MODE_REBOOT;
	}

    // the thread will perform a graceful reboot
    if( thread_t_create( THREAD_CAST(sys_reboot_thread),
                         PSTR("reboot"),
                         0,
                         0 ) < 0 ){

        // if the thread failed to create, just reboot right away so we don't get stuck.
        reboot();
    }
}

void reboot( void ){

	// make sure interrupts are disabled
	DISABLE_INTERRUPTS;

	// enable watchdog timer:
    wdg_v_enable( WATCHDOG_TIMEOUT_16MS, WATCHDOG_FLAGS_RESET );

	// infinite loop, wait for reset
	for(;;);
}

// runtime assertion handling.
// note all assertions are considered fatal, and will result in the system
// rebooting into the bootloader.  it will pass the assertion information
// to the bootloader so it can report the error.
#ifdef INCLUDE_ASSERTS
void assert(FLASH_STRING_T str_expr, FLASH_STRING_T file, int line){

#ifndef BOOTLOADER

    #ifdef ALLOW_ASSERT_DISABLE
    if( disable_assertions ){

        asserted = TRUE;

        return;
    }
    #endif

	// disable interrupts, as this is a fatal error
	DISABLE_INTERRUPTS;

    sys_v_disable_watchdog();

    #ifdef ENABLE_WCOM
    // get timer information
    uint32_t sym_count = rf_u32_read_symbol_counter();
    uint32_t sym_count_irq_status = rf_u8_get_sym_count_irq_status();
    uint32_t sym_count_irq_mask = rf_u8_get_sym_count_irq_mask();
    uint32_t sym_cmp_1 = rf_u32_get_sym_count_compare( 1 );
    uint32_t sym_cmp_2 = rf_u32_get_sym_count_compare( 2 );
    uint32_t sym_cmp_3 = rf_u32_get_sym_count_compare( 3 );
    #endif

    // create error log
    cfg_error_log_t error_log;
    memset( &error_log, 0, sizeof(error_log) );

    // get pointer
    void *ptr = error_log.log;

    // copy flash file name pointer to memory variable
    char filename[32];
    strlcpy_P( filename, file, sizeof(filename) );

    // get thread function pointer
    uint32_t thread_addr = thread_u32_get_current_addr();

    uint32_t irq_addr = osirq_u32_get_irq_addr();
    uint32_t last_irq_addr = osirq_u32_get_last_irq_addr();

    // write assert data
    ptr += sprintf_P( ptr,
                      PSTR("Assert File: %s Line: %u \r\nThread: 0x%lx IRQ: 0x%lx LastIRQ: 0x%lx\r\n"),
                      filename,
                      line,
                      thread_addr,
                      irq_addr,
                      last_irq_addr );

    // write sys error code
    ptr += sprintf_P( ptr, PSTR("Error: %u  Warnings: %lu\r\n"), sys_error, warnings );

    // write system time
    ptr += sprintf_P( ptr, PSTR("System Time: %lu \r\n"), tmr_u32_get_system_time_ms() );

    // write network time
    datetime_t datetime;
    datetime_v_now( &datetime );
    char buf[ISO8601_STRING_MIN_LEN];
    datetime_v_to_iso8601( buf, sizeof(buf), &datetime );
    ptr += sprintf_P( ptr, PSTR("SNTP Time: %s \r\n"), buf );

    // write memory data
    mem_rt_data_t rt_data;
    mem2_v_get_rt_data( &rt_data );

    ptr += sprintf_P(   ptr,
                        PSTR("Memory: Handles: %u Free: %u Used: %u Dirty: %u Data: %u Stack: %u\r\n"),
                        rt_data.handles_used,
                        rt_data.free_space,
                        rt_data.used_space,
                        rt_data.dirty_space,
                        rt_data.data_space,
                        mem2_u16_stack_count() );

    #ifdef ENABLE_MEMORY_DUMP
    // dump memory
    // this only works with a small number of handles.
    // try compiling with 32 or less.
    for( uint16_t i = 0; i < MAX_MEM_HANDLES; i++ ){

        mem_block_header_t header = mem2_h_get_header( i );

        ptr += sprintf_P( ptr,
                          PSTR("%3d:%4x\r\n"),
                          header.size,
                          #ifdef ENABLE_RECORD_CREATOR
                          header.creator_address
                          #else
                          0
                          #endif
                          );
    }
    #endif

    #ifdef ENABLE_WCOM
    // write timer info
    ptr += sprintf_P(
            ptr,
            PSTR("SymCount: %lu IRQS: %lu IRQM: %lu CMP1: %lu CMP2: %lu CMP3: %lu\r\n"),
            sym_count,
            sym_count_irq_status,
            sym_count_irq_mask,
            sym_cmp_1,
            sym_cmp_2,
            sym_cmp_3
            );
    #endif

    // write error log
    cfg_v_write_error_log( &error_log );


    // log final event and flush to file
    // this is dangerous, because if we assert from an IRQ
    // we will be calling file system and memory manager code
    // in an unsafe way.
    EVENT( EVENT_ID_SYS_ASSERT, 0 );
    event_v_flush();

    // dump threadinfo
    // dangerous, see above.
    thread_v_dump();


    // reboot
    reboot();
#endif
}
#endif

void sys_v_wdt_reset( void ){

	wdg_v_reset();
}

void sys_v_init_watchdog( void ){

    wdg_v_enable( WATCHDOG_TIMEOUT_2048MS, WATCHDOG_FLAGS_INTERRUPT );
}

void sys_v_disable_watchdog( void ){

    wdg_v_reset();
    wdg_v_disable();
}



PT_THREAD( sys_reboot_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // shutdown presence
    #ifdef ENABLE_PRESENCE
    presence_v_shutdown();
    #endif

    // notify boot mode
    kv_i8_publish( __KV__boot_mode );

    while( reboot_delay > 0 ){

	   TMR_WAIT( pt, 1000 );

       reboot_delay--;
    }

    // check for safe mode.  wcom is disabled in safe mode for now,
    // so a call to wcom will crash.
    if( sys_mode != SYS_MODE_SAFE ){

        #ifdef ENABLE_WCOM
        // shutdown network
        wcom_neighbors_v_shutdown();
        #endif
    }

    #ifdef ENABLE_USB
    usb_v_shutdown();
    #endif

    log_v_debug_P( PSTR("Ldr Cmd: %x Mode: %d"), boot_data.loader_command, boot_data.boot_mode );

    log_v_flush();

	TMR_WAIT( pt, 1000 );

	reboot();

PT_END( pt );
}


void sys_v_enable_interrupts( void ){

    interrupts_enabled = TRUE;
    ENABLE_INTERRUPTS;
}

void sys_v_disable_interrupts( void ){

    DISABLE_INTERRUPTS;
    interrupts_enabled = FALSE;
}


#ifdef EXPERIMENTAL_ATOMIC
static bool critical = FALSE;

void _sys_v_enter_critical( void ){

    DISABLE_INTERRUPTS;

    ASSERT( atomic_counter < 255 );

    #ifdef ATOMIC_TIMING

    if( critical ){

        return;
    }

    critical = TRUE;

    if( atomic_counter == 0 ){

        atomic_timestamp = tmr_u64_get_system_time_us();
    }
    #endif

    atomic_counter++;

    #ifdef ATOMIC_TIMING
    critical = FALSE;
    #endif
}

void _sys_v_exit_critical( FLASH_STRING_T file, int line ){

    #ifdef ATOMIC_TIMING
    if( critical ){

        return;
    }

    critical = TRUE;
    #endif

    ASSERT( atomic_counter != 0 );

    atomic_counter--;

    if( atomic_counter == 0 ){

        #ifdef ATOMIC_TIMING

        uint64_t elapsed = tmr_u64_get_system_time_us() - atomic_timestamp;

        if( elapsed > atomic_longest_time ){

            atomic_longest_time = elapsed;
            atomic_longest_file = file;
            atomic_longest_line = line;
        }

        critical = FALSE;

        #endif

        if( interrupts_enabled ){

            ENABLE_INTERRUPTS;
        }
    }
}

#endif


#ifdef ATOMIC_TIMING

void _sys_v_start_atomic_timestamp( void ){

    if( atomic_counter == 0 ){

        atomic_timestamp = tmr_u64_get_system_time_us();
    }

    atomic_counter++;
}

void _sys_v_end_atomic_timestamp( void ){

    atomic_counter--;

    if( atomic_counter == 0 ){

        uint64_t elapsed = tmr_u64_get_system_time_us() - atomic_timestamp;

        if( elapsed > atomic_longest_time ){

            atomic_longest_time = elapsed;
        }
    }
}

#endif
