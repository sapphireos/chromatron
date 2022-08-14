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

#if 0

/*

Framework for software tests

*/


// do not include in simulator builds, there is no reason to.
#ifndef __SIM__


#include "cpu.h"
#include "system.h"

#include "io.h"
#include "cmd_usart.h"
#include "config.h"

#include "CuTest.h"

#include "status_led.h"
#include "sw_test.h"

#ifdef ENABLE_USB
#include "usb_intf.h"
#endif

#include <stdio.h>


static int uart_putchar( char c, FILE *stream );
static FILE uart_stdout = FDEV_SETUP_STREAM( uart_putchar, NULL, _FDEV_SETUP_WRITE );

static int uart_putchar( char c, FILE *stream )
{
	if( c == '\n' ){

        cmd_usart_v_send_char( '\r' );
	}

	cmd_usart_v_send_char( c );

	return 0;
}

void sw_test_v_init( void ){

    status_led_v_set( 1, STATUS_LED_YELLOW );

    // sys_v_init();
    // sys_v_disable_watchdog();
    //
    //
    // // init serial port
    // cmd_usart_v_init();
    //
    //
    // // set all LEDs to outputs
    // io_v_set_mode( IO_PIN_LED0, IO_MODE_OUTPUT );
    // io_v_set_mode( IO_PIN_LED1, IO_MODE_OUTPUT );
    // io_v_set_mode( IO_PIN_LED2, IO_MODE_OUTPUT );
    //
    // // clear LEDs
    // io_v_digital_write( IO_PIN_LED_GREEN, LOW );
    // io_v_digital_write( IO_PIN_LED_YELLOW, LOW );
    // io_v_digital_write( IO_PIN_LED_RED, LOW );
    //
    // // note that we don't call io_v_init() because it will create a thread.
    // // we don't want any threads in the test mode since they may interfere with
    // // the tests.
    //
    // sei();
    //
    // // check sys mode
    // if( sys_u8_get_mode() == SYS_MODE_SAFE ){
    //
    //     printf_P( PSTR("\nASSERT\n") );
    //
    //     cfg_error_log_t error_log;
    //     cfg_v_read_error_log( &error_log );
    //
    //     printf_P( PSTR("%s"), error_log.log );
    //
    //     io_v_digital_write( IO_PIN_LED_RED, HIGH );
    //
    //     for(;;);
    // }

}

// void sw_test_v_start( PGM_P module_name ){

    // printf_P( PSTR("SapphireOS Test Framework\n") );
    // printf_P( PSTR("Testing module: ") );
    // printf_P( module_name );
    // printf_P( PSTR("\n") );
    //
    // // set yellow LED
    // io_v_digital_write( IO_PIN_LED_YELLOW, HIGH );
// }

void sw_test_finish( PGM_P module_name, CuSuite *suite ){

    sys_v_init();

    #ifdef ENABLE_USB
    sw_test_usb_v_init();
    #endif

    // init serial port
    cmd_usart_v_init();

    // set stdout
	stdout = &uart_stdout;

    status_led_v_set( 0, STATUS_LED_YELLOW );

    if( suite->failCount == 0 ){

        status_led_v_set( 1, STATUS_LED_GREEN );
    }
    else{

        status_led_v_set( 1, STATUS_LED_RED );
    }

    sei();

    while(1){

        if( cmd_usart_i16_get_char() >= 0 ){

            printf_P( PSTR("{\n") );
            printf_P( PSTR("    \"title\": \"SapphireOS Test Framework\",\n") );
            printf_P( PSTR("    \"module\": \"") );
            printf_P( module_name );
            printf_P( PSTR("\",\n") );

            printf_P( PSTR("    \"pattern\": \"") );

            for( int i = 0; i < suite->count; i++ ){

        		CuTest* testCase = suite->list[i];

                if( testCase->failed ){

                    printf_P( PSTR("F") );
                }
                else{

                    printf_P( PSTR(".") );
                }
        	}

        	printf_P( PSTR("\",\n") );

            printf_P( PSTR("    \"tests\": %d,\n"), suite->count );
            printf_P( PSTR("    \"passed\": %d,\n"), suite->count - suite->failCount );
            printf_P( PSTR("    \"failed\": %d,\n"), suite->failCount );

            if( suite->failCount > 0 ){

                printf_P( PSTR("    \"failures:\": [\n") );

                for( int i = 0; i < suite->count; i++ ){

                    CuTest* testCase = suite->list[i];

                    if( testCase->failed ){

                        printf("        \"%s: %s\",\n", testCase->name, testCase->message);
                    }
                }

                printf_P( PSTR("    ]\n") );
            }

            printf_P( PSTR("}\n") );
        }


        #ifdef ENABLE_USB
        sw_test_usb_v_process();
        #endif
    }
}

#endif
#endif