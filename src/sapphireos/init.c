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


#include "system.h"
#include "cmd_usart.h"
#include "random.h"
#include "eeprom.h"
#include "config.h"
#include "adc.h"
#include "crc.h"
#include "threading.h"
#include "timers.h"
#include "fs.h"
#include "io.h"
#include "status_led.h"
#include "logging.h"
#include "datetime.h"
#include "keyvalue.h"
#include "catbus.h"
#include "kvdb.h"
#include "event_log.h"
#include "watchdog.h"
#include "dns.h"
#include "sntp.h"

#ifdef ENABLE_NETWORK
#include "netmsg.h"
#include "sockets.h"
#endif

#ifdef ENABLE_FFS
#include "flash25.h"
#include "flash_fs.h"
#endif

#ifdef ENABLE_POWER
#include "power.h"
#endif

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

#ifdef ENABLE_WIFI
#include "esp8266.h"
#endif

#ifdef ENABLE_TIME_SYNC
#include "timesync.h"
#endif

#include "init.h"


int8_t sapphire_i8_init( void ){

    // Must be called first!
	sys_v_init(); // init system controller

	// disable watchdog timer
    sys_v_disable_watchdog();

	// Must be called second!
	thread_v_init();

	// Must be called third!
	mem2_v_init();

	// init analog module
	adc_v_init();

    // initialize board IO
    io_v_init();

    // system check IO for safe mode
    sys_v_check_io_for_safe_mode();

    // turn off yellow LED (left on from loader)
    status_led_v_set( 0, STATUS_LED_YELLOW );

    // turn on green LED during init
    status_led_v_set( 1, STATUS_LED_GREEN );

    // if safe mode, turn on red led during init
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        status_led_v_set( 0, STATUS_LED_YELLOW );
        status_led_v_set( 1, STATUS_LED_RED );
    }

	// init CRC module
	crc_v_init();

    #ifdef ENABLE_USB
    usb_v_init();
    #endif

    // init serial port
    cmd_usart_v_init();

    #ifdef ENABLE_FFS
    // init flash driver
    flash25_v_init();

    // init flash file system
    ffs_v_init();
    #endif

    // init user file system
    fs_v_init();

    // check if safe mode
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        // init logging module
        log_v_init();
    }

    // init event log
    event_v_init();

    // init key value service
    kv_v_init();

    // init key value DB
    kvdb_v_init();

    // init EEPROM
    ee_v_init();

	// init config manager
	cfg_v_init();

    // check if we need to go into recovery mode
    sys_v_check_recovery_mode();    

    // if safe mode, turn on red led during init
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        status_led_v_set( 0, STATUS_LED_YELLOW );
        status_led_v_set( 1, STATUS_LED_RED );
    }


    #ifdef ENABLE_IP
    // init IP module
    ip_v_init();
    #endif

    #ifdef ENABLE_NETWORK
    // init sockets
    sock_v_init();

    // init netmsg
    netmsg_v_init();

    #endif

    // init random number generator
    rnd_v_init();

    // init timekeeping
    datetime_v_init();

    #ifdef ENABLE_POWER
    // init power module
    pwr_v_init();
    #endif

    // initialize status LED
    status_led_v_init();

    #ifdef ENABLE_WIFI
    wifi_v_init();
    #endif

    catbus_v_init();

    #ifdef ENABLE_TIME_SYNC
    time_v_init();
    sntp_v_init();
    #endif
    
    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        trace_printf( "SapphireOS SAFE MODE\r\n" );

        return -1;
    }
    else if( sys_u8_get_mode() == SYS_MODE_NO_APP ){
        return -2;
    }


    dns_v_init();

    trace_printf( "SapphireOS ready\r\n" );

    // return system OK
    return 0;
}


void sapphire_run( void ){

    // enable global interrupts
    sys_v_enable_interrupts();

    // init timers
	// do this just before starting the scheduler so that we won't miss the
	// first timer interrupt
	tmr_v_init();

    // enable watchdog timer
    sys_v_init_watchdog();

	// start the thread scheduler
	thread_start();
}
