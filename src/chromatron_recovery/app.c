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

#include "sapphire.h"
#include "flash25.h"
#include "hal_status_led.h"
#include "esp8266.h"
#include "pwm.h"

#include "app.h"



#define PIXEL_EN_PORT           PORTA
#define PIXEL_EN_PIN            7



static bool io_pins[8];
static bool io_pwm[4];
static bool io_pix_enable;
static uint16_t adc_bg;
static uint16_t adc_temp;

KV_SECTION_META kv_meta_t io_control_kv[] = {
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[0],         0,   "io_pin_0" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[1],         0,   "io_pin_1" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[2],         0,   "io_pin_2" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[3],         0,   "io_pin_3" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[4],         0,   "io_pin_4" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[5],         0,   "io_pin_5" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[6],         0,   "io_pin_6" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pins[7],         0,   "io_pin_7" },

    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pwm[0],          0,   "io_pwm_1" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pwm[1],          0,   "io_pwm_2" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pwm[2],          0,   "io_pwm_3" },
    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pwm[3],          0,   "io_pwm_4" },

    { SAPPHIRE_TYPE_BOOL,    0, 0, &io_pix_enable,      0,   "io_pix_enable" },

    { SAPPHIRE_TYPE_UINT16,  0, 0, &adc_bg,             0,   "adc_bg" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, &adc_temp,           0,   "adc_temp" },
};



// compute CRC of external partition
static uint16_t _app_u16_recovery_crc( void ){

    uint16_t crc = crc_u16_start();
    uint32_t length;

    flash25_v_read( FW_LENGTH_ADDRESS + FLASH_FS_FIRMWARE_1_PARTITION_START, &length, sizeof(length) );

    // verify length is valid
    if( length > FLASH_FS_FIRMWARE_1_PARTITION_SIZE ){

        return 0xffff;
    }

    uint32_t address = FLASH_FS_FIRMWARE_1_PARTITION_START;

    while( length > 0 ){

        uint8_t buf[64];
        uint16_t copy_len = 64;

        if( (uint32_t)copy_len > length ){

            copy_len = length;
        }

        if( copy_len > length ){

            copy_len = length;
        }

        flash25_v_read( address, buf, copy_len );

        address += copy_len;
        length -= copy_len;

        crc = crc_u16_partial_block( crc, buf, copy_len );

        // reset watchdog timer
        sys_v_wdt_reset();
    }

    crc = crc_u16_finish( crc );

    return crc;
}

// we don't use the FFS_FW version of the internal CRC check, because it accounts for the CRC
// at the end of the image, so a valid CRC will return 0. 
// this makes it impossible to tell if the firmware has changed.
// so, we implement our own that doesn't include the CRC at the end.
static uint16_t _app_u16_internal_crc( void ){

    uint16_t crc = crc_u16_start();

    uint32_t length = sys_v_get_fw_length();

    for( uint32_t i = 0; i < length; i++ ){

        crc = crc_u16_byte( crc, pgm_read_byte_far( i + FLASH_START ) );

        // reset watchdog timer
        sys_v_wdt_reset();
    }

    return crc_u16_finish( crc );    
}


static void _app_v_erase_recovery_partition( void ){

    for( uint16_t i = 0; i < FLASH_FS_FIRMWARE_1_N_BLOCKS; i++ ){

        // enable writes
        flash25_v_write_enable();

        // erase current block
        flash25_v_erase_4k( ( (uint32_t)i * (uint32_t)FLASH_FS_ERASE_BLOCK_SIZE ) + FLASH_FS_FIRMWARE_1_PARTITION_START );

        // wait for erase to complete
        SAFE_BUSY_WAIT( flash25_b_busy() );

        sys_v_wdt_reset();
    }
}


PT_THREAD( io_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  
        
    while(1){

        TMR_WAIT( pt, 50 );

        adc_bg = adc_u16_read_mv( ADC_CHANNEL_BG );
        adc_temp = adc_u16_read_mv( ADC_CHANNEL_TEMP );

        for( uint8_t i = 0; i < cnt_of_array(io_pins); i++ ){

            if( io_pins[i] ){

                io_v_set_mode( i, IO_MODE_OUTPUT );
                io_v_digital_write( i, 1 );
            }
            else{

                io_v_set_mode( i, IO_MODE_INPUT_PULLDOWN );
                io_v_digital_write( i, 0 );   
            }
        }


        for( uint8_t i = 0; i < cnt_of_array(io_pwm); i++ ){

            if( io_pwm[i] ){

                pwm_v_write( i, 65535 );
            }
            else{

                pwm_v_write( i, 0 );
            }
        }

        if( io_pix_enable ){

            PIXEL_EN_PORT.OUTSET = ( 1 << PIXEL_EN_PIN );
        }
        else{

            PIXEL_EN_PORT.OUTCLR = ( 1 << PIXEL_EN_PIN );
        }
    }

PT_END( pt );	
}

PT_THREAD( led_thread( pt_t *pt, void *state ) )
{           
PT_BEGIN( pt );  
    
    // while(1){
    
    //     status_led_v_disable();
            
    //     status_led_v_set( 1, STATUS_LED_WHITE );

    //     TMR_WAIT( pt, 2000 );

    //     status_led_v_enable();

    //     TMR_WAIT( pt, 5000 );
    // }

    status_led_v_disable();
    
    while(1){

        if( wifi_b_connected() ){

            status_led_v_set( 1, STATUS_LED_BLUE );

            TMR_WAIT( pt, 500 );

            status_led_v_set( 1, STATUS_LED_WHITE );

            TMR_WAIT( pt, 500 );    
        }
        else{

            status_led_v_set( 1, STATUS_LED_GREEN );

            TMR_WAIT( pt, 500 );

            status_led_v_set( 1, STATUS_LED_WHITE );

            TMR_WAIT( pt, 500 );    
        }
    }

PT_END( pt );   
}


void app_v_init( void ){

    // disable link system
    catbus_v_set_options( CATBUS_OPTION_LINK_DISABLE );

    // check if external flash is present and accessible
    if( flash25_u32_capacity() == 0 ){

        return;
    }   

    uint32_t sys_fw_length = sys_v_get_fw_length() + sizeof(uint16_t); // adjust for CRC

    
    // check recovery section CRC
    if( _app_u16_recovery_crc() != _app_u16_internal_crc() ){

        log_v_info_P( PSTR("Recovery partition CRC mismatch") );   

        _app_v_erase_recovery_partition();

        // copy firmware to partition
        for( uint32_t i = 0; i < sys_fw_length; i++ ){

            sys_v_wdt_reset();

            // read byte from internal flash
            uint8_t temp = pgm_read_byte_far( i + FLASH_START );

            // write to external flash
            flash25_v_write_byte( i + (uint32_t)FLASH_FS_FIRMWARE_1_PARTITION_START, temp );
        }
        
        // recheck CRC
        if( _app_u16_recovery_crc() != _app_u16_internal_crc() ){

            log_v_error_P( PSTR("Failed to store firmware to recovery partition") );
        }
        else{

            log_v_info_P( PSTR("Recovery partition stored to flash") );   
        }
    }
    else{

        log_v_info_P( PSTR("Recovery partition CRC OK") );   
    }

    // enable PWM, we'll need that
    pwm_v_init();

    // set up pixel enable
    PIXEL_EN_PORT.OUTCLR = ( 1 << PIXEL_EN_PIN );
    PIXEL_EN_PORT.DIRSET = ( 1 << PIXEL_EN_PIN );

    PIXEL_EN_PORT.OUTCLR = ( 1 << PIXEL_EN_PIN );


    thread_t_create( io_thread,
                     PSTR("io"),
                     0,
                     0 );

    thread_t_create( led_thread,
                     PSTR("led"),
                     0,
                     0 );
}

