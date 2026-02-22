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

#include "sapphire.h"

#include "pca9685.h"

static uint8_t i2c_addr;

static uint16_t target_pwm[PCA9685_N_CHANNELS];
static uint16_t current_pwm[PCA9685_N_CHANNELS];
static int16_t pwm_step[PCA9685_N_CHANNELS];

#define PWM_FADER_RATE 20


PT_THREAD( pwm_fader_thread( pt_t *pt, void *state ) );



static void _pca9685_v_write_reg( uint8_t reg_addr, uint8_t data ){

	i2c_v_write_reg8( i2c_addr, reg_addr, data );
}

static uint8_t _pca9685_u8_read_reg( uint8_t reg_addr ){

	return i2c_u8_read_reg8( i2c_addr, reg_addr );
}


static uint32_t prescale_to_freq( uint32_t prescale ){

    prescale += 1;
    prescale *= 4096;

    return PCA9685_OSC_FREQ / prescale;
}

void pca9685_v_set_freq( uint8_t prescale ){

    if( prescale < PCA9685_MIN_PRESCALE ){

        prescale = PCA9685_MIN_PRESCALE;
    }

    uint32_t freq = prescale_to_freq( prescale );

    log_v_debug_P( PSTR("PCA9685 freq set to %lu"), freq );

    _pca9685_v_write_reg( PCA9685_REG_PRESCALE, prescale );
}

uint32_t pca9685_u32_get_freq( void ){

    uint32_t prescale = _pca9685_u8_read_reg( PCA9685_REG_PRESCALE );

    return prescale_to_freq( prescale );
}

static void _pca9685_v_set( uint8_t channel, uint16_t value ){

    ASSERT( channel < PCA9685_N_CHANNELS );

    uint8_t reg_addr = ( channel * 4 ) + PCA9685_REG_LED0_ON_L;

    // turn on at beginning of cycle
    _pca9685_v_write_reg( reg_addr, 0 );
    _pca9685_v_write_reg( reg_addr + 1, 0 );

    // turn off at value cycles in
    _pca9685_v_write_reg( reg_addr + 2, value & 0xff );
    _pca9685_v_write_reg( reg_addr + 3, value >> 8 );
}

void pca9685_v_set( uint8_t channel, uint16_t value ){

    _pca9685_v_set( channel, value );

    target_pwm[channel] = value;
    current_pwm[channel] = value;
}

void pca9685_v_fade( uint8_t channel, uint16_t value, uint16_t fade_time ){

    ASSERT( channel < PCA9685_N_CHANNELS );

    target_pwm[channel] = value;
    
    uint16_t fade_steps = fade_time / PWM_FADER_RATE;

    if( fade_steps <= 1 ){

        fade_steps = 2;
    }

    int32_t diff = (int32_t)target_pwm[channel] - (int32_t)current_pwm[channel];
    int32_t step = diff / fade_steps;

    if( step > PCA9685_MAX_PWM ){

        step = PCA9685_MAX_PWM;   
    }
    else if( step < -1 * PCA9685_MAX_PWM ){

        step = -1 * PCA9685_MAX_PWM;   
    }
    else if( step == 0 ){

        if( diff >= 0 ){

            step = 1;
        }
        else{

            step = -1;
        }   
    }

    pwm_step[channel] = step;
}

void pca9685_v_init( uint8_t addr ){

	i2c_v_init( I2C_BAUD_400K );

    #ifdef ESP8266
    i2c_v_set_pins( IO_PIN_6_DAC0, IO_PIN_7_DAC1 );
    #endif

    i2c_addr = addr;

    ASSERT( ( addr == PCA9685_I2C_ADDR_0 ) || ( addr == PCA9685_I2C_ADDR_1 ) );

    // set AI bit in MODE1, set all others to 0 to bring it out of sleep
    // and enable internal oscillator
    _pca9685_v_write_reg( PCA9685_REG_MODE1, PCA9685_MODE1_BIT_AI );

    // MODE2 sets output driver format (totem-pole/open-drain)
    // and inversion settings
    
    log_v_debug_P( PSTR("PCA9685 found at 0x%02x"), i2c_addr );

    thread_t_create( pwm_fader_thread,
                     PSTR("pwm_fader"),
                     0,
                     0 );
}


PT_THREAD( pwm_fader_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, PWM_FADER_RATE );

        for( uint8_t i = 0; i < PCA9685_N_CHANNELS; i++ ){

            if( current_pwm[i] != target_pwm[i] ){

                int32_t diff = (int32_t)current_pwm[i] - (int32_t)target_pwm[i];

                int32_t new_pwm;

                // check if this is the last fade step
                if( abs32( diff ) < abs16( pwm_step[i] ) ){

                    new_pwm = target_pwm[i];
                }
                else{

                    new_pwm = (int32_t)current_pwm[i] + pwm_step[i];

                    if( new_pwm > PCA9685_MAX_PWM ){

                        new_pwm = PCA9685_MAX_PWM;
                    }
                    else if( new_pwm < 0 ){

                        new_pwm = 0;
                    }
                }

                current_pwm[i] = new_pwm;

                _pca9685_v_set( i, current_pwm[i] );
            }
        }
    }
    
PT_END( pt );
}