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

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FS.h>
#include "comm_intf.h"
#include "wifi.h"
#include "irq_line.h"
#include "version.h"

extern "C"{
    #include "random.h"
    #include "vm_runner.h"
    #include "user_interface.h"
    #include "crc.h"
    #include "wifi_cmd.h"
    #include "kvdb.h"
    #include "memory.h"
}


static uint8_t mem_heap[8192];


void setup(){

    Serial.begin(115200);
    Serial.print( "SOS_ESP " );
    delay(10);

    Serial.write(0);
    Serial.print("v");
    Serial.print(VERSION_MAJOR);
    Serial.print(".");
    Serial.print(VERSION_MINOR);
    Serial.write(0);
    delay(10);

    String rst = ESP.getResetReason();
    Serial.println(rst);
    delay(10);

    String info = ESP.getResetInfo();
    Serial.println(info);
    delay(10);

    Serial.print( "Free heap:" );
    Serial.println( ESP.getFreeHeap() );

    delay(10);

    Serial.end();

    system_update_cpu_freq( SYS_CPU_160MHZ );

    rnd_v_init();

    irqline_v_init();

    mem2_v_init( mem_heap, sizeof(mem_heap) );

    kvdb_v_init();

    wifi_v_init();

    intf_v_init();

    vm_v_init();

    //SPIFFS.begin();

    // uint32_t array[64];
    // for( uint32_t i = 0; i < 64; i++ ){

    //     array[i] = i + 1;
    // }

    // uint32_t array2[16];
    // for( uint32_t i = 0; i < 16; i++ ){

    //     array2[i] = i + 3;
    // }

    // kvdb_i8_add( __KV__test_array, CATBUS_TYPE_UINT32, 64, array, sizeof(array), 0 );
    // kvdb_i8_add( __KV__test_data,  CATBUS_TYPE_UINT32, 1, 0, 0, 0 );
    // kvdb_i8_add( __KV__test_meow,  CATBUS_TYPE_UINT32, 16, array2, sizeof(array2), 0 );

    // intf_v_send_kv( __KV__test_array );
    // intf_v_send_kv( __KV__test_data );
    // intf_v_send_kv( __KV__test_meow );


    wifi_set_sleep_type( MODEM_SLEEP_T );
}

uint32_t elapsed_time( uint32_t start_time ){

    uint32_t end_time = micros();

    uint32_t elapsed;

    if( end_time >= start_time ){
        
        elapsed = end_time - start_time;
    }
    else{
        
        elapsed = UINT32_MAX - ( start_time - end_time );
    }

    if( elapsed > UINT16_MAX ){

        elapsed = UINT16_MAX;
    }

    return elapsed;
}

void loop(){

    uint32_t start, elapsed;

    process_stats_t *stats;
    intf_v_get_proc_stats( &stats );

    irqline_v_process();

    start = micros();
    intf_v_process();
    stats->intf_run_time = elapsed_time( start );

    start = micros();
    wifi_v_process();
    stats->wifi_run_time = elapsed_time( start );

    start = micros();
    vm_v_process();
    stats->vm_run_time = elapsed_time( start );

    start = micros();
    mem2_v_collect_garbage();
    stats->mem_run_time = elapsed_time( start );

    if( stats->intf_run_time > stats->intf_max_time ){

        stats->intf_max_time = stats->intf_run_time;
    }

    if( stats->wifi_run_time > stats->wifi_max_time ){

        stats->wifi_max_time = stats->wifi_run_time;
    }

    if( stats->vm_run_time > stats->vm_max_time ){

        stats->vm_max_time = stats->vm_run_time;
    }

    if( stats->mem_run_time > stats->mem_max_time ){

        stats->mem_max_time = stats->mem_run_time;
    }
}
