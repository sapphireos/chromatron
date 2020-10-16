/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2020  Jeremy Billheimer
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
#include "election.h"
#include "services.h"
#endif

#include "flash25.h"
#include "flash_fs.h"

#ifdef ENABLE_POWER
#include "power.h"
#endif

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

#ifdef ENABLE_WIFI
#include "wifi.h"
#endif

#ifdef ENABLE_TIME_SYNC
#include "timesync.h"
#endif

#ifdef ENABLE_MSGFLOW
#include "msgflow.h"
#endif

#include "init.h"


int8_t sapphire_i8_init( void ){

    // Must be called first!
	sys_v_init(); // init system controller

	// disable watchdog timer
    #ifndef ESP8266
    sys_v_disable_watchdog();
    #endif

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

    // init serial port
    cmd_usart_v_init();

    // init flash driver
    flash25_v_init();

    // init flash file system
    ffs_v_init();

    // init user file system
    fs_v_init();

    // check if safe mode
    if( sys_u8_get_mode() != SYS_MODE_SAFE ){

        // init logging module
        log_v_init();
    }
trace_printf("event\n");
    // init event log
    event_v_init();
trace_printf("kv\n");
    // init key value service
    kv_v_init();
trace_printf("kvdb\n");
    // init key value DB
    kvdb_v_init();
trace_printf("ee\n");
    // init EEPROM
    ee_v_init();
trace_printf("cfg\n");
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
trace_printf("sock\n");
    #ifdef ENABLE_NETWORK
    // init sockets
    sock_v_init();

    // init netmsg
    netmsg_v_init();

    #endif
trace_printf("rnd\n");
    // init random number generator
    rnd_v_init();
trace_printf("date\n");
    // init timekeeping
    datetime_v_init();

    #ifdef ENABLE_POWER
    // init power module
    pwr_v_init();
    #endif
trace_printf("status\n");
    // initialize status LED
    status_led_v_init();

    #ifdef ENABLE_WIFI
trace_printf("wifi\n");
    wifi_v_init();
    #endif

trace_printf("catbus\n");
    catbus_v_init();

    #ifdef ENABLE_TIME_SYNC
trace_printf("time\n");
    time_v_init();
    sntp_v_init();
    #endif

    #ifdef ENABLE_ELECTION
    election_v_init();
    #endif

    #ifdef ENABLE_SERVICES
    services_v_init();
    #endif

    #ifdef ENABLE_MSGFLOW
    msgflow_v_init();
    #endif

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        trace_printf( "SapphireOS SAFE MODE\r\n" );

        return -1;
    }
    else if( sys_u8_get_mode() == SYS_MODE_NO_APP ){
        return -2;
    }

    #ifdef ENABLE_WIFI
    dns_v_init();
    #endif

    log_v_info_P( PSTR("Sapphire start") );
    trace_printf( "SapphireOS ready\r\n" );

    trace_printf( "HW Rev: %u\r\n", io_u8_get_board_rev() );

    // return system OK
    return 0;
}


void sapphire_run( void ){

    // enable global interrupts
    sys_v_enable_interrupts();

    #ifdef ENABLE_USB
    usb_v_init();
    #endif

    // init timers
	// do this just before starting the scheduler so that we won't miss the
	// first timer interrupt
	tmr_v_init();

    // enable watchdog timer
    sys_v_init_watchdog();

	// start the thread scheduler
	thread_start();
}
