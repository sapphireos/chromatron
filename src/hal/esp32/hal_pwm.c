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


#include "system.h"

#include "pwm.h"
#include "hal_pwm.h"

#include "sapphire.h"

#include "driver/ledc.h"
#include "driver/timer.h"


static void esp32_timer_init( int group, int timer, int timer_interval_msec );


static ledc_channel_config_t ledc_channel[HAL_PWM_MAX_CHANNELS];

void pwm_v_init( void ){

    esp32_timer_init( TIMER_GROUP_0, TIMER_0, 250 );

    for( uint8_t i = 0; i < cnt_of_array(ledc_channel); i++ ){

        ledc_channel[i].gpio_num = -1;
    }


    // leaving this example code here for future reference:
    

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


#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

static bool IRAM_ATTR timer_isr_callback(void *args)
{
    // BaseType_t high_task_awoken = pdFALSE;
    // example_timer_info_t *info = (example_timer_info_t *) args;

    // uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(info->timer_group, info->timer_idx);

    /* Prepare basic event data that will be then sent back to task */
    // example_timer_event_t evt = {
    //     .info.timer_group = info->timer_group,
    //     .info.timer_idx = info->timer_idx,
    //     .info.auto_reload = info->auto_reload,
    //     .info.alarm_interval = info->alarm_interval,
    //     .timer_counter_value = timer_counter_value
    // };

    // if (!info->auto_reload) {
    //     timer_counter_value += info->alarm_interval * TIMER_SCALE;
    //     timer_group_set_alarm_value_in_isr(info->timer_group, info->timer_idx, timer_counter_value);
    // }

    /* Now just send the event data back to the main program task */
    // xQueueSendFromISR(s_timer_queue, &evt, &high_task_awoken);

    // return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR

    return FALSE;
}

static void esp32_timer_init( int group, int timer, int timer_interval_msec )
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = TRUE,
    }; // default clock source is APB
    timer_init(group, timer, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(group, timer, 0);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(group, timer, timer_interval_msec * ( TIMER_SCALE / 1000 ));
    timer_enable_intr(group, timer);

    // example_timer_info_t *timer_info = calloc(1, sizeof(example_timer_info_t));
    // timer_info->timer_group = group;
    // timer_info->timer_idx = timer;
    // timer_info->auto_reload = auto_reload;
    // timer_info->alarm_interval = timer_interval_sec;
    // timer_isr_callback_add(group, timer, timer_group_isr_callback, timer_info, 0);
    timer_isr_callback_add(group, timer, timer_isr_callback, 0, 0);

    timer_start(group, timer);
}

