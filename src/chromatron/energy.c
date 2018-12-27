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

// #include "sapphire.h"

// #include "logging.h"

// #include "esp8266.h"
// #include "graphics.h"
// #include "pixel.h"

// #include "energy.h"


// #define MICROAMPS_CPU           10000
// #define MICROAMPS_WIFI          10000
// #define MICROAMPS_GFX           10000

// // pixel calibrations for a single pixel at full power
// #define MICROAMPS_RED_PIX       10000
// #define MICROAMPS_GREEN_PIX     10000
// #define MICROAMPS_BLUE_PIX      10000

// static uint64_t energy_cpu;
// static uint64_t energy_wifi;
// static uint64_t energy_gfx;
// static uint64_t energy_pix_red;
// static uint64_t energy_pix_green;
// static uint64_t energy_pix_blue;
// static uint64_t energy_total;

// KV_SECTION_META kv_meta_t energy_info_kv[] = {
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_cpu,        0,    "energy_cpu" },
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_wifi,       0,    "energy_wifi" },
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_gfx,        0,    "energy_gfx" },
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_pix_red,    0,    "energy_pix_red" },
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_pix_green,  0,    "energy_pix_green" },
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_pix_blue,   0,    "energy_pix_blue" },
//     { SAPPHIRE_TYPE_UINT64,   0, 0,  &energy_total,      0,    "energy_total" },
// };


// PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) );


// void energy_v_init( void ){

//     thread_t_create( energy_monitor_thread,
//                      PSTR("energy_monitor"),
//                      0,
//                      0 );

// }

// uint64_t energy_u64_get_total( void ){

//     return energy_total;
// }

// PT_THREAD( energy_monitor_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

//     while( 1 ){

//         TMR_WAIT( pt, 50 );

//         energy_cpu += MICROAMPS_CPU;
        

//         if( gfx_b_running() ){
            
//             energy_gfx += MICROAMPS_GFX;
//         }

//         if( wifi_b_running() ){

//             energy_wifi += MICROAMPS_WIFI;    
//         }

//         uint16_t total_r, total_g, total_b;
//         pixel_v_get_rgb_totals( &total_r, &total_g, &total_b );

//         uint32_t current_r, current_g, current_b;

//         current_r = ( total_r * MICROAMPS_RED_PIX ) / 256;
//         current_g = ( total_g * MICROAMPS_GREEN_PIX ) / 256;
//         current_b = ( total_b * MICROAMPS_BLUE_PIX ) / 256;

//         energy_pix_red += current_r;
//         energy_pix_green += current_g;
//         energy_pix_blue += current_b;

//         energy_total = 0;
//         energy_total += energy_cpu;
//         energy_total += energy_wifi;
//         energy_total += energy_gfx;
//         energy_total += energy_pix_red;
//         energy_total += energy_pix_green;
//         energy_total += energy_pix_blue;
//     }    

// PT_END( pt );
// }
