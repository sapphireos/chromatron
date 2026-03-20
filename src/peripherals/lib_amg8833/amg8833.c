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

/*

AMG8833 driver
Panasonic Grid-Eye 8x8 IR sensor



Resolution is 12 bits per pixel

*/

#include "sapphire.h"

#include "i2c.h"

#include "amg8833.h"
#include "logging.h"

#ifdef ESP32

static uint8_t device_addr;
static int16_t therm;

static int16_t raw_pixels[AMG8833_PIXEL_COUNT];
static int16_t delta_pixels[AMG8833_PIXEL_COUNT];
static int16_t background_pixels[AMG8833_PIXEL_COUNT];
static int16_t image_pixels[AMG8833_PIXEL_COUNT];
static int16_t detection_pixels[AMG8833_PIXEL_COUNT];

static uint8_t pixels256[256];

static uint8_t detection_threshold_trigger = 96;
static uint8_t detection_threshold_release = 64;

static int16_t detection_max;
static uint8_t detection_max_pixel;

// static int16_t filtered_pixels[AMG8833_PIXEL_COUNT];


// static int16_t therm_delta_pixels[AMG8833_PIXEL_COUNT];

// static uint8_t background_avg;
// static uint8_t delta_avg;
// static uint8_t therm_delta_avg;
// static uint16_t delta_total;
// static int16_t therm_delta_total;
// static int8_t min_therm_delta;
// static int8_t max_therm_delta;

#define BACKGROUND_FILTER_RATIO 4
#define BACKGROUND_TIMING_RATIO 16
#define PIXEL_FILTER_RATIO 128


KV_SECTION_OPT kv_meta_t amg_info_kv[] = {
    { CATBUS_TYPE_INT16,  0,                          KV_FLAGS_READ_ONLY,  &therm,             0,   "amg_temp" },

    { CATBUS_TYPE_INT16,  0,                          KV_FLAGS_READ_ONLY,  &detection_max,     0,   "amg_detection_max" },
    { CATBUS_TYPE_UINT8,  0,                          KV_FLAGS_READ_ONLY,  &detection_max_pixel,0,  "amg_detection_max_pixel" },


    { CATBUS_TYPE_UINT8,  0,                          0,      &detection_threshold_trigger,    0,   "amg_threshold_trigger" },
    { CATBUS_TYPE_UINT8,  0,                          0,      &detection_threshold_release,    0,   "amg_threshold_release" },

    // { CATBUS_TYPE_UINT8,  0,                          KV_FLAGS_READ_ONLY,  &background_avg,    0,   "amg_background_avg" },
    // { CATBUS_TYPE_UINT8,  0,                          KV_FLAGS_READ_ONLY,  &delta_avg,         0,   "amg_delta_avg" },
    // { CATBUS_TYPE_UINT8,  0,                          KV_FLAGS_READ_ONLY,  &therm_delta_avg,   0,   "amg_therm_delta_avg" },
    // { CATBUS_TYPE_UINT16, 0,                          KV_FLAGS_READ_ONLY,  &delta_total,       0,   "amg_delta_total" },
    // { CATBUS_TYPE_INT16,  0,                          KV_FLAGS_READ_ONLY,  &therm_delta_total, 0,   "amg_delta_therm_total" },
    { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  raw_pixels,         0,   "amg_pixels" },
    { CATBUS_TYPE_UINT8,  256 - 1,                    KV_FLAGS_READ_ONLY,  pixels256,          0,   "amg_pixels256" },
    // { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  filtered_pixels,    0,   "amg_filtered_pixels" },
    { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  background_pixels,  0,   "amg_background" },
    { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  delta_pixels,       0,   "amg_delta" },
    { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  image_pixels,       0,   "amg_image" },
    { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  detection_pixels,   0,   "amg_detection" },
    // { CATBUS_TYPE_INT16,  AMG8833_PIXEL_COUNT - 1,    KV_FLAGS_READ_ONLY,  therm_delta_pixels, 0,   "amg_therm_delta" },
    // { CATBUS_TYPE_INT8,   0,                          KV_FLAGS_READ_ONLY,  &min_therm_delta,   0,   "amg_min_therm_delta" },
    // { CATBUS_TYPE_INT8,   0,                          KV_FLAGS_READ_ONLY,  &max_therm_delta,   0,   "amg_max_therm_delta" },
};



PT_THREAD( amg8833_sensor_thread( pt_t *pt, void *state ) );

void amg8833_v_reg_write( uint8_t addr, uint8_t data ){

    uint8_t cmd[2];
    cmd[0] = addr;
    cmd[1] = data;

    i2c_v_write( device_addr, cmd, sizeof(cmd) );
}

uint8_t amg8833_u8_reg_read( uint8_t addr ){

    uint8_t data = 0;

    i2c_v_write( device_addr, &addr, sizeof(addr) );
    i2c_v_read( device_addr, &data, sizeof(data) );

    return data;
}

void amg8833_v_init( void ){

    i2c_v_init( I2C_BAUD_400K );
        
    device_addr = AMG8833_I2C_ADDR_0;

    // attempt to set one of the registers, read back to check if device is present
    amg8833_v_reg_write( AMG8833_REG_INTHL, 0x43 );

    if( amg8833_u8_reg_read( AMG8833_REG_INTHL ) != 0x43 ){
        
        // try next address
        device_addr = AMG8833_I2C_ADDR_1;

        amg8833_v_reg_write( AMG8833_REG_INTHL, 0x43 );

        if( amg8833_u8_reg_read( AMG8833_REG_INTHL ) != 0x43 ){

            // log_v_debug_P( PSTR("AMG8833 not found") );

            return;
        }
    }

    log_v_debug_P( PSTR("AMG8833 detected") );

    kv_v_add_db_info( amg_info_kv, sizeof(amg_info_kv) );

    amg8833_v_reg_write( AMG8833_REG_INTHL, 0 );

    // sensor detected, start threads
    thread_t_create( amg8833_sensor_thread,
                     PSTR("amg8833_sensor"),
                     0,
                     0 );
}


void amg8833_v_reset( void ){

    amg8833_v_reg_write( AMG8833_REG_RST, AMG8833_RST_INIT_RESET );
}

void amg8833_v_set_mode( uint8_t mode ){

    amg8833_v_reg_write( AMG8833_REG_PCTL, mode );
}

void amg8833_v_set_rate( uint8_t rate ){
    
    amg8833_v_reg_write( AMG8833_REG_FPSC, rate );
}

int16_t amg8833_i16_read_thermistor( void ){

    uint8_t temp0 = 0xff;
    uint8_t temp1 = 0xff;

    temp0 = amg8833_u8_reg_read( AMG8833_REG_TTHL );
    temp1 = amg8833_u8_reg_read( AMG8833_REG_TTHH );

    uint16_t raw = ( temp1 << 8 ) | temp0;

    return ( raw * 100 ) / 16;
}

int16_t amg8833_i16_read_pixel( uint8_t index ){

    ASSERT( index < AMG8833_PIXEL_COUNT );

    uint8_t temp0 = 0xff;
    uint8_t temp1 = 0xff;

    uint16_t addr = AMG8833_REG_PIXEL_START + ( index * 2 );

    temp0 = amg8833_u8_reg_read( addr );
    temp1 = amg8833_u8_reg_read( addr + 1 );

    return ( ( ( temp1 << 8 ) | temp0 ) * 100 ) / 4;
}

void amg8833_v_read_pixels_raw( int16_t pixels[AMG8833_PIXEL_COUNT] ){

    uint8_t addr = AMG8833_REG_PIXEL_START;

    i2c_v_write( device_addr, &addr, sizeof(addr) );

    i2c_v_read( device_addr, (uint8_t *)pixels, sizeof(int16_t) * AMG8833_PIXEL_COUNT );
}

// void amg8833_v_read_pixels_raw_8bit( uint8_t pixels[AMG8833_PIXEL_COUNT] ){

//     int16_t raw[AMG8833_PIXEL_COUNT];
//     amg8833_v_read_pixels_raw( raw );

//     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){        

//         if( raw[i] < 0 ){

//             raw[i] = 0;
//         }
//         else if( raw[i] > 255 ){

//             raw[i] = 255;
//         }

//         pixels[i] = raw[i];
//     }   
// }

void amg8833_v_read_pixels( int16_t pixels[AMG8833_PIXEL_COUNT] ){
    
    amg8833_v_read_pixels_raw( pixels );

    for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        pixels[i] *= 16;
    }    
}

static int16_t _amg8833_i16_ewma_single( int16_t new, int16_t old, uint8_t ratio ){

    return util_i16_ewma( new, old, ratio );

    // // check if filter is unchanging
    // if( filtered == old ){

    //     // adjust by minimum
    //     if( new > old ){

    //         filtered++;
    //     }
    //     else if( new < old ){

    //         filtered--;
    //     }
    // }   

    // return filtered;
}

// static void _amg8833_v_ewma( int16_t new[AMG8833_PIXEL_COUNT], int16_t old[AMG8833_PIXEL_COUNT], uint8_t ratio ){

//     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

//         int16_t temp = _amg8833_i16_ewma_single( new[i], old[i], ratio );

//         old[i] = temp;
//     }   
// }


PT_THREAD( amg8833_sensor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    static bool init;
    init = FALSE;

    static uint8_t frame;
    frame = 0;

    amg8833_v_reset();
    amg8833_v_set_mode( AMG8833_PCTL_MODE_NORMAL );
    amg8833_v_set_rate( AMG8833_FPSC_10FPS );

    log_v_debug_P( PSTR("AMG8833 detected at 0x%02x"), device_addr );

    TMR_WAIT( pt, 500 );

    // init arrays
    amg8833_v_read_pixels( raw_pixels );        
    memcpy( background_pixels, raw_pixels, sizeof(background_pixels) );


    while( 1 ){

        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() );
        thread_v_set_alarm( tmr_u32_get_system_time_ms() + 100 );

        therm = amg8833_i16_read_thermistor();
        int16_t normalized_therm = ( therm * 4 ) / 100;
        normalized_therm *= 16;

        amg8833_v_read_pixels( raw_pixels );
            
        // compute delta from thermistor
        for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

            int16_t delta = raw_pixels[i] - normalized_therm;

            delta_pixels[i] = delta;
        }

        if( !init ){

            memcpy( background_pixels, delta_pixels, sizeof(background_pixels) );

            init = TRUE;
        }
        else{

            // update background filter
            if( ( frame % BACKGROUND_TIMING_RATIO ) == 0 ){
                
                for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

                    // freeze background if detected
                    if( detection_pixels[i] == 0 ){

                        background_pixels[i] =_amg8833_i16_ewma_single( delta_pixels[i], background_pixels[i], BACKGROUND_FILTER_RATIO );
                    }
                }

                // _amg8833_v_ewma( delta_pixels, background_pixels, BACKGROUND_FILTER_RATIO );
            }
        }

        detection_max = -1000;

        for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

            int16_t delta = delta_pixels[i] - background_pixels[i];

            if( delta < 0 ){

                delta = 0;
            }

            image_pixels[i] = _amg8833_i16_ewma_single( delta, image_pixels[i], PIXEL_FILTER_RATIO );

            if( image_pixels[i] > detection_threshold_trigger ){

                // onset trigger
                if( image_pixels[i] > detection_pixels[i] ){

                    // detection_pixels[i] = image_pixels[i];    
                    detection_pixels[i] = 250;    
                }
                // decay
                else if( image_pixels[i] < detection_pixels[i] ){

                    detection_pixels[i]--;
                }
            }

            if( image_pixels[i] < detection_threshold_release ){

                detection_pixels[i] -= 8;
            }

            if( detection_pixels[i] < 0 ){

                detection_pixels[i] = 0;
            }

            if( detection_pixels[i] > detection_max ){

                detection_max = detection_pixels[i];

                if( detection_max > 0 ){

                    detection_max_pixel = i;
                }
            }
        }



        // _amg8833_v_ewma( raw_pixels, filtered_pixels, PIXEL_FILTER_RATIO );    

        // uint32_t temp_background = 0;
        // uint32_t temp_delta = 0;
        // int32_t temp_therm = 0;

        // min_therm_delta = 127;
        // max_therm_delta = -127;

        // // update calculated values
        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //     if( raw_pixels[i] > background_pixels[i] ){

        //         delta_pixels[i] = raw_pixels[i] - background_pixels[i];
        //     }
        //     else{

        //         delta_pixels[i] = 0;
        //     }

        //     if( delta_pixels[i] < min_therm_delta ){

        //         min_therm_delta = delta_pixels[i];
        //     }
        //     else if( delta_pixels[i] > max_therm_delta ){

        //         max_therm_delta = delta_pixels[i];
        //     }

        //     int16_t normalized_therm = ( therm * 4 ) / 100;

        //     int16_t temp = (int16_t)raw_pixels[i] - normalized_therm;

        //     if( temp > 127 ){

        //         temp = 127;
        //     }
        //     else if( temp < -127 ){

        //         temp = -127;
        //     }

        //     therm_delta_pixels[i] = temp;
            
        //     temp_background += background_pixels[i];
        //     temp_delta += delta_pixels[i];
        //     temp_therm += therm_delta_pixels[i];
        // }

        // background_avg  = temp_background / AMG8833_PIXEL_COUNT;
        // delta_avg       = temp_delta / AMG8833_PIXEL_COUNT;
        // therm_delta_avg = temp_therm / AMG8833_PIXEL_COUNT;

        // delta_total         = temp_delta;
        // therm_delta_total   = temp_therm;



        // 256 point upconversion        

        uint8_t x = 0;
        uint8_t y = 0;

        // upsample to 256 items
        for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

            uint8_t temp = ( raw_pixels[i] / 128 ) - 100;
            pixels256[( y * 16 ) + x]             = temp;
            pixels256[( y * 16 ) + x + 1]         = temp;
            pixels256[( ( y + 1 ) * 16 ) + x]     = temp;
            pixels256[( ( y + 1 ) * 16 ) + x + 1] = temp;

            x += 2;

            if( x >= 16 ){

                x = 0;
                y += 2;
            }
        }


        frame++;
    }

PT_END( pt );
}


#if 0

#define PIXEL_FILTER         64

#define DETECT_THRESHOLD    10
#define MIN_DETECT_VALUE    64


// this function will not work correctly if pixels and output
// are the same array.
// void _amg8833_v_pixel_mass( const int16_t pixels[AMG8833_PIXEL_COUNT], int16_t output[AMG8833_PIXEL_COUNT] ){

//     int16_t (*pixels_2d)[8] = (int16_t (*)[8])pixels;
//     int16_t (*output_2d)[8] = (int16_t (*)[8])output;

//     for( int8_t y = 0; y < 8; y++ ){
//         for( int8_t x = 0; x < 8; x++ ){

//             int32_t a = 0;  
//             int32_t a2 = 0;        
//             int8_t xi, yi;

//             xi = x - 0;
//             yi = y - 0;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a = (int32_t)pixels_2d[xi][yi];
//                 }
//             }



//             xi = x - 1;
//             yi = y - 1;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x - 0;
//             yi = y - 1;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x + 1;
//             yi = y - 1;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x - 1;
//             yi = y - 0;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x + 1;
//             yi = y - 0;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x - 1;
//             yi = y + 1;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x - 0;
//             yi = y + 1;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }

//             xi = x + 1;
//             yi = y + 1;
//             if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

//                 if( pixels_2d[xi][yi] > a2 ){

//                     a2 = (int32_t)pixels_2d[xi][yi];
//                 }
//             }


//             a += a2;

//             output_2d[x][y] = a;
//         }
//     }
// }



int16_t _amg8833_i16_get_next_largest( const int8_t pixels[AMG8833_PIXEL_COUNT], uint8_t i, uint8_t *coord ){

    int8_t (*pixels_2d)[8] = (int8_t (*)[8])pixels;

    int8_t x = i / 8;
    int8_t y = i % 8;

    int16_t a = 0;  

    int8_t xi, yi;

    xi = x - 1;
    yi = y - 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x - 0;
    yi = y - 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x + 1;
    yi = y - 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x - 1;
    yi = y - 0;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x + 1;
    yi = y - 0;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x - 1;
    yi = y + 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x - 0;
    yi = y + 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }

    xi = x + 1;
    yi = y + 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] > a ){

            a = (int16_t)pixels_2d[xi][yi];
            *coord = xi * 8 + yi;
        }
    }
    
    return a;    
}

#define MARK 4

void _amg8833_v_mark_neighbors( const uint8_t pixels[AMG8833_PIXEL_COUNT], uint8_t i ){

    uint8_t (*pixels_2d)[8] = (uint8_t (*)[8])pixels;

    int8_t x = i / 8;
    int8_t y = i % 8;

    int8_t xi, yi;

    // xi = x - 1;
    // yi = y - 1;
    // if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

    //     if( pixels_2d[xi][yi] < MARK ){

    //         pixels_2d[xi][yi] = MARK;
    //     }
    // }

    xi = x - 0;
    yi = y - 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] < MARK ){

            pixels_2d[xi][yi] = MARK;
        }
    }

    // xi = x + 1;
    // yi = y - 1;
    // if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

    //     if( pixels_2d[xi][yi] < MARK ){

    //         pixels_2d[xi][yi] = MARK;
    //     }
    // }

    xi = x - 1;
    yi = y - 0;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] < MARK ){

            pixels_2d[xi][yi] = MARK;
        }
    }

    xi = x + 1;
    yi = y - 0;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] < MARK ){

            pixels_2d[xi][yi] = MARK;
        }
    }

    // xi = x - 1;
    // yi = y + 1;
    // if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

    //     if( pixels_2d[xi][yi] < MARK ){

    //         pixels_2d[xi][yi] = MARK;
    //     }
    // }

    xi = x - 0;
    yi = y + 1;
    if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

        if( pixels_2d[xi][yi] < MARK ){

            pixels_2d[xi][yi] = MARK;
        }
    }

    // xi = x + 1;
    // yi = y + 1;
    // if( ( xi < 8 ) && ( yi < 8 ) && ( xi >= 0 ) && ( yi >= 0 ) ){

    //     if( pixels_2d[xi][yi] < MARK ){

    //         pixels_2d[xi][yi] = MARK;
    //     }
    // }
}

#endif

        // // compute current array average
        // uint16_t sum = 0;
        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //      sum += raw_pixels[i];
        // }
        // pixel_avg = sum / cnt_of_array(raw_pixels);
        // // pixel_avg = cnt_of_array(raw_pixels);
        // // pixel_avg = sum;

        // if( !init ){

        //     init = TRUE;

        //     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //         int16_t temp = raw_pixels[i];

        //         if( temp > 127 ){

        //             temp = 127;
        //         }

        //         fast_filtered[i] = temp;
        //         ref1[i] = temp;
        //         ref2[i] = temp;
        //     }
        // }

        // // run fast filter
        // _amg8833_v_ewma( raw_pixels, fast_filtered, PIXEL_FILTER );

        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //     int16_t temp = (int16_t)fast_filtered[i] - (int16_t)ref1[i];

        //     if( temp > 127 ){

        //         temp = 127;
        //     }

        //     pixel_delta[i] = temp;
        // }

        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //     // drain detection
        //     if( detection[i] > 0 ){

        //         detection[i]--;
        //     }
        // }

        // // compute noise
        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //     if( ( frame % 8 ) == 0 ){

        //         if( noise[i] > 0 ){

        //             noise[i]--;
        //         } 
        //     }

        //     int16_t diff = (int16_t)raw_pixels[i] - (int16_t)ref2[i];

        //     if( diff < 0 ){

        //         diff *= -1;
        //     }

        //     diff *= 16;

        //     if( diff > 255 ){

        //         diff = 255;
        //     }

        //     noise[i] = util_i16_ewma( diff, noise[i], 16 );
        // }


        // // compute delta over pixel average
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // //     int16_t temp = (int16_t)raw_pixels[i] - (int16_t)pixel_avg;

        // //     if( temp > 127 ){

        // //         temp = 127;
        // //     }
        // //     else if( temp < -127 ){
                
        // //         temp = -127;
        // //     }

        // //     pixel_delta[i] = temp;
        // // }

        // // enhance gain of pixels by summing each pixel with it's
        // // highest neighbor, going in order from highest pixel to lowest
        // int8_t temp_pixels[AMG8833_PIXEL_COUNT];
        // memcpy( temp_pixels, pixel_delta, sizeof(temp_pixels) );

        // bool found = FALSE;
        // do{

        //     found = FALSE;

        //     int8_t temp_max = 0;
        //     uint8_t max_i = 0;
        //     // get local maxima of deltas
        //     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //         if( temp_pixels[i] > temp_max ){

        //             temp_max = temp_pixels[i];
        //             max_i = i;

        //             found = TRUE;
        //         }
        //     }

        //     uint8_t coord = max_i;
        //     int16_t a = temp_pixels[max_i] + _amg8833_i16_get_next_largest( temp_pixels, max_i, &coord );


        //     temp_pixels[coord] = 0;
        //     // remove this pixel from consideration
        //     temp_pixels[max_i] = 0;

        //     // limit range
        //     if( a > 127 ){

        //         a = 127;
        //     }
        //     else if( a < -127 ){
                
        //         a = -127;
        //     }

        //     // assign updated value
        //     pixel_delta[max_i] = a;
 
        // } while( found );

            
        // // run detection from reference 1
        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //     // check if we meet threshold
        //     if( pixel_delta[i] > DETECT_THRESHOLD ){

        //         int16_t temp = pixel_delta[i] * 16;

        //         if( temp > 255 ){

        //             temp = 255;
        //         }

        //         if( detection[i] < DETECT_THRESHOLD ){

        //             detection[i] = MIN_DETECT_VALUE;                    
        //         }
        //         else if( detection[i] < temp ){

        //             uint16_t temp2 = detection[i] + 4;

        //             if( temp2 > 255 ){

        //                 temp2 = 255;
        //             }

        //             detection[i] = temp2;
        //         }


        //         // if( temp > detection[i] ){
                    
        //             // detection[i] = temp;
        //         // }
        //     }
        // }

        // // mark neighbor pixels
        // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //     if( detection[i] > MARK ){

        //         _amg8833_v_mark_neighbors( detection, i );                
        //     }
        // }

        // if( ( frame % 8 ) == 0 ){
            
        //     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //         if( detection[i] == 0 ){

        //             // run reference 1
        //             if( fast_filtered[i] < ref1[i] ){

        //                 if( ref1[i] > -127 ){
                            
        //                     ref1[i]--;
        //                 }
        //             }
        //         }
        //     }
        // }


        // if( ( frame % 8 ) == 0 ){
            
        //     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        //         // // run reference 2
        //         // if( fast_filtered[i] > ref2[i] ){

        //         //     if( ref2[i] < 127 ){
                        
        //         //         ref2[i]++;
        //         //     }
        //         // }
        //         // else if( fast_filtered[i] < ref2[i] ){

        //         //     if( ref2[i] > -127 ){
                        
        //         //         ref2[i]--;
        //         //     }
        //         // }

        //         if( detection[i] == 0 ){

        //             // run reference 1
        //             if( fast_filtered[i] > ref1[i] ){

        //                 if( ref1[i] < 127 ){
                            
        //                     ref1[i]++;
        //                 }
        //             }
        //             // else if( fast_filtered[i] < ref1[i] ){

        //             //     if( ref1[i] > -127 ){
                            
        //             //         ref1[i]--;
        //             //     }
        //             // }

        //             // // update reference 1 on pixels not in detection from ref 2
        //             // if( ref2[i] > ref1[i] ){

        //             //     if( ref1[i] < 127 ){   

        //             //         ref1[i]++;
        //             //     }
        //             // }
        //             // else if( ref2[i] < ref1[i] ){

        //             //     if( ref1[i] > -127 ){

        //             //         ref1[i]--;
        //             //     }
        //             // }
        //         }
        //     }
        // }

        


        // // catbus_i8_publish( __KV__amg_pixels );
        // // catbus_i8_publish( __KV__amg_pixel_avg );
        // // catbus_i8_publish( __KV__amg_pixel_delta );
        // // catbus_i8_publish( __KV__amg_detection );
        // // catbus_i8_publish( __KV__amg_filtered );
        // // catbus_i8_publish( __KV__amg_ref1 );
        // // catbus_i8_publish( __KV__amg_ref2 );
        // // catbus_i8_publish( __KV__amg_noise );








        // frame++;






        // // // process sensor data


        // // // run ewma filter
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // //     filtered[i] = _amg8833_i16_ewma_single( raw_pixels[i], filtered[i], PIXEL_FILTER );
        // // }

        // // // compute current array average
        // // uint32_t sum = 0;
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // //     sum += filtered[i];
        // // }

        // // pixel_avg = sum / cnt_of_array(filtered);

        // // // compute delta above background average, 
        // // // then compute pixel "mass"
        // // int16_t pixel_avg_delta[AMG8833_PIXEL_COUNT];
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){        

        // //     pixel_avg_delta[i] = filtered[i] - pixel_avg;
        // // }

        // // // compute delta above background temperature
        // // // then compute pixel "mass"
        // // int16_t pixel_therm_delta[AMG8833_PIXEL_COUNT];
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){        

        // //     pixel_therm_delta[i] = filtered[i] - therm;
        // // }

        // // // // apply thresholds
        // // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // // //     if( ( ( pixel_avg_delta[i] > 100 ) && ( pixel_therm_delta[i] > 50 ) ) ||
        // // //         ( pixel_avg_delta[i] > 250 ) ){

        // // //     }
        // // //     else{

        // // //         pixel_avg_delta[i] = 0;                    
        // // //     }
        // // // }

        // // bool found = FALSE;
        // // do{

        // //     found = FALSE;

        // //     int16_t temp_max = 0;
        // //     int16_t max_i = 0;
        // //     // get local maxima of deltas
        // //     for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // //         if( pixel_avg_delta[i] > temp_max ){

        // //             temp_max = pixel_avg_delta[i];
        // //             max_i = i;

        // //             found = TRUE;
        // //         }
        // //     }

        // //     int16_t a = pixel_avg_delta[max_i] + _amg8833_i16_get_next_largest( pixel_avg_delta, max_i );

        // //     if( ( a > detection[max_i] ) && ( a > 400 ) ){
            
        // //         detection[max_i] = a;
        // //     }

        // //     pixel_avg_delta[max_i] = 0;

        // // } while( found );

        // // // compute detection scores
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // //     detection[i] -= 4;

        // //     if( detection[i] < 0 ){

        // //         detection[i] = 0;
        // //     }
        // // }

        // // catbus_i8_publish( __KV__amg_detection );






        // // 256 point upconversion        

        // // uint8_t x = 0;
        // // uint8_t y = 0;
        // // uint8_t data256[256];

        // // // upsample to 256 items
        // // for( uint8_t i = 0; i < AMG8833_PIXEL_COUNT; i++ ){

        // //     uint8_t temp = ( raw_pixels[i] / 128 ) - 100;
        // //     data256[( y * 16 ) + x]             = temp;
        // //     data256[( y * 16 ) + x + 1]         = temp;
        // //     data256[( ( y + 1 ) * 16 ) + x]     = temp;
        // //     data256[( ( y + 1 ) * 16 ) + x + 1] = temp;

        // //     // kvdb_i8_array_set( __KV__amg_data256, CATBUS_TYPE_UINT8, ( y * 16 ) + x,             &temp, sizeof(temp) );
        // //     // kvdb_i8_array_set( __KV__amg_data256, CATBUS_TYPE_UINT8, ( y * 16 ) + x + 1,         &temp, sizeof(temp) );
        // //     // kvdb_i8_array_set( __KV__amg_data256, CATBUS_TYPE_UINT8, ( ( y + 1 ) * 16 ) + x,     &temp, sizeof(temp) );
        // //     // kvdb_i8_array_set( __KV__amg_data256, CATBUS_TYPE_UINT8, ( ( y + 1 ) * 16 ) + x + 1, &temp, sizeof(temp) );

        // //     x += 2;

        // //     if( x >= 16 ){

        // //         x = 0;
        // //         y += 2;
        // //     }
        // // }

        // // kvdb_i8_set( __KV__amg_data256, CATBUS_TYPE_UINT8, data256, sizeof(data256) );

        // // catbus_i8_publish( __KV__amg_data256 );

/*

Thoughts on target tracking:

Using output of filter layers

For first target:

Find pixel with highest value.  Then, take its immediate neighbors and calculate the 
center of mass.  That yields our exact target position, and the total sum can be the
mass of the target.

For the second target (and beyond):

Set previous target pixel and its immediate neighbors to 0.  Then run the algorithm
again.

Do the same for however many targets we want.


Center of mass:

M = m1 + m2 + m3 ...

COMx = (m1*x1 + m2*x2 + m3*x3 ...) / M
COMy = (m1*y1 + m2*y2 + m3*y3 ...) / M

That's our center of mass x and y coordinate, and the mass itself.

If we use 8 bit numbers for coords, that gives us 32 steps per pixel, which is more
than enough.

With precise coordinates, we can now set zone points along with distances, creating
a zone circle.

*/



#endif
