/*
// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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

#include "pwm.h"
#include "hal_pwm.h"

#include "sapphire.h"

#include "driver/ledc.h"

static ledc_channel_config_t ledc_channel[HAL_PWM_MAX_CHANNELS];

// static uint16_t pwm;
// static uint16_t adc;

// int8_t _pwm_kv_handler(
//     kv_op_t8 op,
//     catbus_hash_t32 hash,
//     void *data,
//     uint16_t len )
// {
    
//     if( op == KV_OP_SET ){

//         ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, pwm);
//         ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);
//     }

//     return 0;
// }

// KV_SECTION_META kv_meta_t pwm_info_kv[] = {
//     { CATBUS_TYPE_UINT16,  0, 0,    &pwm,              _pwm_kv_handler,        "pwm" },
//     { CATBUS_TYPE_UINT16,  0, 0,    &adc,              0,        "pwm_adc" },
// };



// PT_THREAD( adc_pwm_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );
        
//     while(1){

//         TMR_WAIT( pt, 100 );

//         adc = adc_u16_read_raw( IO_PIN_17_TX );

//         adc = 4095 - adc;

//         adc = ( 100 * adc ) / 4095;
        
//         pwm = 600 + adc * 4;

//         if( pwm < 700 ){

//             pwm = 0;
//         }

//         ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, pwm);
//         ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);

//     }
    
// PT_END( pt );
// }

// #define PWM_ENABLED

void pwm_v_init( void ){

    for( uint8_t i = 0; i < cnt_of_array(ledc_channel); i++ ){

        ledc_channel[i].gpio_num = -1;
    }


    #pragma message "PWM ready for testing!"




    // #pragma message "PWM is not done!"

    // #ifdef PWM_ENABLED

    // #pragma message "PWM test enabled"

    // thread_t_create( adc_pwm_thread,
    //              PSTR("adc_pwm"),
    //              0,
    //              0 );


    // io_v_set_mode( IO_PIN_17_TX, IO_MODE_INPUT );    
    // io_v_set_mode( IO_PIN_34_A2, IO_MODE_INPUT );    

    // #define MOTOR_IO_0  IO_PIN_16_RX
    // // #define MOTOR_IO_1  IO_PIN_17_TX

    // io_v_set_mode( MOTOR_IO_0, IO_MODE_OUTPUT );
    // // io_v_set_mode( MOTOR_IO_1, IO_MODE_OUTPUT );

    // io_v_digital_write( MOTOR_IO_0, 0 );
    // // io_v_digital_write( MOTOR_IO_1, 0 );


    // ledc_timer_config_t ledc_timer = {
    //     .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
    //     .freq_hz = 20000,                      // frequency of PWM signal
    //     .speed_mode = LEDC_HIGH_SPEED_MODE,           // timer mode
    //     .timer_num = LEDC_TIMER_0            // timer index
    // };
    // // Set configuration of timer0 for high speed channels
    // ledc_timer_config(&ledc_timer);

    // // PWM on GPIO 16

    // /*
    //  * Prepare individual configuration
    //  * for each channel of LED Controller
    //  * by selecting:
    //  * - controller's channel number
    //  * - output duty cycle, set initially to 0
    //  * - GPIO number where LED is connected to
    //  * - speed mode, either high or low
    //  * - timer servicing selected channel
    //  *   Note: if different channels use one timer,
    //  *         then frequency and bit_num of these channels
    //  *         will be the same
    //  */
    

    // // Set LED Controller with previously prepared configuration
    // for( uint32_t ch = 0; ch < cnt_of_array(ledc_channel); ch++ ){

    //     ledc_channel_config(&ledc_channel[ch]);
    // }


    // ledc_set_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel, 0);
    // ledc_update_duty(ledc_channel[0].speed_mode, ledc_channel[0].channel);

    // #endif
}

int8_t get_channel( uint8_t gpio ){

    // convert GPIO to ESP numbering
    gpio = hal_io_i32_get_gpio_num( gpio );

    for( uint8_t i = 0; i < cnt_of_array(ledc_channel); i++ ){

        if( ledc_channel[i].gpio_num == gpio ){

            return i;
        }
    }

    return -1;
}

void pwm_v_init_channel( uint8_t channel, uint16_t freq ){

    int8_t led_ch = get_channel( channel );

    ledc_channel_config_t *config = 0;

    if( led_ch < 0 ){

        for( uint8_t i = 0; i < cnt_of_array(ledc_channel); i++ ){

            if( ledc_channel[i].gpio_num == -1 ){

                config = &ledc_channel[i];
                led_ch = i;

                break;
            }
        }
    }
    else{

        config = &ledc_channel[led_ch];
    }

    if( config == 0 ){

        log_v_warn_P( PSTR("exceeded maximum PWM channels") );

        return;
    }

    if( freq == 0 ){

        freq = 20000;
    }

    config->channel     = LEDC_CHANNEL_0 + led_ch;
    config->duty        = 0;
    config->gpio_num    = hal_io_i32_get_gpio_num( channel );
    config->speed_mode  = LEDC_HIGH_SPEED_MODE;
    config->hpoint      = 0;
    config->timer_sel   = LEDC_TIMER_0 + led_ch;


    ledc_timer_config_t ledc_timer = {
        .duty_resolution    = LEDC_TIMER_10_BIT,    // resolution of PWM duty
        .freq_hz            = freq,                 // frequency of PWM signal
        .speed_mode         = LEDC_HIGH_SPEED_MODE, // timer mode
        .timer_num          = config->timer_sel,    // timer index
        .clk_cfg            = LEDC_AUTO_CLK
    };

    ledc_timer_config( &ledc_timer );

    io_v_set_mode( channel, IO_MODE_OUTPUT );
    io_v_digital_write( channel, 0 );

    ledc_channel_config( config );
        
    pwm_v_write( channel, 0 );   
}   

void pwm_v_write( uint8_t channel, uint16_t value ){

    int8_t led_ch = get_channel( channel );

    if( led_ch < 0 ){

        return;
    }

    ledc_set_duty( ledc_channel[led_ch].speed_mode, ledc_channel[led_ch].channel, value );
    ledc_update_duty( ledc_channel[led_ch].speed_mode, ledc_channel[led_ch].channel) ;
}
