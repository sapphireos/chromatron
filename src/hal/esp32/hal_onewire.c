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

#include "sapphire.h"
#include "onewire.h"
#include "hal_onewire.h"

#include "driver/rmt.h"


#define TX_CHANNEL RMT_CHANNEL_0
#define RX_CHANNEL RMT_CHANNEL_1

static rmt_config_t rmt_tx_config;
static rmt_config_t rmt_rx_config;

void hal_onewire_v_init( uint8_t gpio ){

    gpio_num_t gpio_num = hal_io_i32_get_gpio_num( gpio );


    rmt_tx_config.rmt_mode = RMT_MODE_TX;                
    rmt_tx_config.channel = TX_CHANNEL;                  
    rmt_tx_config.gpio_num = gpio_num;                       
    rmt_tx_config.clk_div = 80;                          
    rmt_tx_config.mem_block_num = 8;                     
    rmt_tx_config.flags = 0;                             
                    
    rmt_tx_config.tx_config.carrier_freq_hz = 0;                
    rmt_tx_config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH; 
    rmt_tx_config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;        
    rmt_tx_config.tx_config.carrier_duty_percent = 0;              
    rmt_tx_config.tx_config.carrier_en = false;                     
    rmt_tx_config.tx_config.loop_en = false;                        
    rmt_tx_config.tx_config.idle_output_en = true;                  

    rmt_config(&rmt_tx_config);
    rmt_driver_install(TX_CHANNEL, 0, 0);


    // attach GPIO to previous pin
    if (gpio_num < 32)
    {
        GPIO.enable_w1ts = (0x1 << gpio_num);
    }
    else
    {
        GPIO.enable1_w1ts.data = (0x1 << (gpio_num - 32));
    }

    // attach RMT channels to new gpio pin
    // ATTENTION: set pin for rx first since gpio_output_disable() will
    //            remove rmt output signal in matrix!
    rmt_set_pin(RX_CHANNEL, RMT_MODE_RX, gpio_num);
    rmt_set_pin(TX_CHANNEL, RMT_MODE_TX, gpio_num);

    // force pin direction to input to enable path to RX channel
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[gpio_num]);

    // enable open drain
    GPIO.pin[gpio_num].pad_driver = 1;




     // Enable open drain
    // GPIO.pin[gpio_num].pad_driver = 1;

    // hal_onewire_v_write_bit( 0 );
    // hal_onewire_v_write_bit( 0 );
    // hal_onewire_v_write_bit( 0 );
    // hal_onewire_v_write_bit( 1 );
    // hal_onewire_v_write_bit( 1 );
    // hal_onewire_v_write_bit( 1 );

    // rmt_driver_uninstall(TX_CHANNEL);
}

void hal_onewire_v_reset( void ){

    // delay_g();
    // drive_low();
    // delay_h();
    // release();
    // delay_i();
    // bool bit = sample();
    // delay_j();


    rmt_item32_t items[1] = {0};

    items[0].duration0 = ONEWIRE_DELAY_H;
    items[0].level0 = 0;
    items[0].duration1 = ONEWIRE_DELAY_I;
    items[0].level1 = 1;

    rmt_write_items(TX_CHANNEL, items, 1, true);

    // read bit

    _delay_us( ONEWIRE_DELAY_J );

    
}

void hal_onewire_v_write_bit( uint8_t bit ){

    rmt_item32_t items[1] = {0};

    if( bit == 0 ){

        items[0].duration0 = ONEWIRE_DELAY_C;
        items[0].level0 = 0;
        items[0].duration1 = ONEWIRE_DELAY_D;
        items[0].level1 = 1;
    }
    else{ // 1

        items[0].duration0 = ONEWIRE_DELAY_A;
        items[0].level0 = 0;
        items[0].duration1 = ONEWIRE_DELAY_B;
        items[0].level1 = 1;
    }

    rmt_write_items(TX_CHANNEL, items, 1, true);
}

uint8_t hal_onewire_u8_read_bit( void ){

    return 0;
}

