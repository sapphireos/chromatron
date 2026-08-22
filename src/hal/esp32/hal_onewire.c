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
    rmt_tx_config.mem_block_num = 1;                     
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


    rmt_rx_config.rmt_mode = RMT_MODE_RX;                
    rmt_rx_config.channel = RX_CHANNEL;                  
    rmt_rx_config.gpio_num = gpio_num;                       
    rmt_rx_config.clk_div = 80;                          
    rmt_rx_config.mem_block_num = 1;                     
    rmt_rx_config.flags = 0;                             
    rmt_rx_config.rx_config.filter_en = true;
    rmt_rx_config.rx_config.filter_ticks_thresh = 30;
    rmt_rx_config.rx_config.idle_threshold = ONEWIRE_IDLE;
    
    rmt_config(&rmt_rx_config);
    rmt_driver_install(RX_CHANNEL, 512, 0);


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
    // set pin for rx first since gpio_output_disable() will
    //            remove rmt output signal in matrix!
    rmt_set_gpio(RX_CHANNEL, RMT_MODE_RX, gpio_num, false);
    rmt_set_gpio(TX_CHANNEL, RMT_MODE_TX, gpio_num, false);

    // force pin direction to input to enable path to RX channel
    PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[gpio_num]);

    // enable open drain
    GPIO.pin[gpio_num].pad_driver = 1;
}

static void flush_rx( void ){

    RingbufHandle_t ringbuf;

    rmt_get_ringbuf_handle( RX_CHANNEL, &ringbuf );
    uint32_t rx_size = 0;

    while( 1 ){

        void *ptr = xRingbufferReceive( ringbuf, &rx_size, 0 );

        if( ptr == 0 ){

            break;
        }

        vRingbufferReturnItem( ringbuf, ptr );
    }
}

void hal_onewire_v_deinit( void ){

    flush_rx();

    rmt_driver_uninstall(TX_CHANNEL);
    rmt_driver_uninstall(RX_CHANNEL);
}

bool hal_onewire_b_reset( void ){

    flush_rx();

    bool presence = FALSE;

    rmt_set_rx_idle_thresh(RX_CHANNEL, ONEWIRE_DELAY_H + ONEWIRE_DELAY_I);


    rmt_item32_t items[1] = {0};

    items[0].duration0 = ONEWIRE_DELAY_H;
    items[0].level0 = 0;
    items[0].duration1 = 0;
    items[0].level1 = 1;

    rmt_rx_start(RX_CHANNEL, true);

    rmt_write_items(TX_CHANNEL, items, 1, true);

    RingbufHandle_t ringbuf;

    rmt_get_ringbuf_handle(RX_CHANNEL, &ringbuf);
    uint32_t rx_size = 0;
    rmt_item32_t *rx_items = (rmt_item32_t *)xRingbufferReceive(ringbuf, &rx_size, 20 / portTICK_PERIOD_MS);

    if( rx_items != 0 ){

        uint32_t rx_count = rx_size / sizeof(rmt_item32_t);

        // for (int i = 0; i < count; i++)
        // {
        //     log_v_debug_P(PSTR("level0: %d, duration %d"), rx_items[i].level0, rx_items[i].duration0);
        //     log_v_debug_P(PSTR("level1: %d, duration %d"), rx_items[i].level1, rx_items[i].duration1);
        // }

        // should have received more than 1 item:
        if( rx_count > 1 ){

            // verify first item is the reset signal
            if( ( rx_items[0].level0 == 0 ) && ( rx_items[0].duration0 > ( ONEWIRE_DELAY_H - 10 ) ) &&
                ( rx_items[0].level1 == 1 ) && ( rx_items[0].duration1 > 0 ) ){

                // verify presence signal in second item
                if( ( rx_items[1].level0 == 0 ) && ( rx_items[1].duration0 > 20 ) ){

                    presence = TRUE;
                }
            }
        }
    }

    rmt_rx_stop(RX_CHANNEL);
    
    return presence;
}

void hal_onewire_v_write_byte( uint8_t byte ){

    rmt_item32_t items[1] = {0};

    for( uint8_t i = 0; i < 8; i++ ){

        if( byte & 0x01 ){

            items[0].duration0 = ONEWIRE_DELAY_A;
            items[0].level0 = 0;
            items[0].duration1 = ONEWIRE_DELAY_B;
            items[0].level1 = 1;
        }
        else{

            items[0].duration0 = ONEWIRE_DELAY_C;
            items[0].level0 = 0;
            items[0].duration1 = ONEWIRE_DELAY_D;
            items[0].level1 = 1;
        }

        rmt_write_items(TX_CHANNEL, items, 1, true);

        byte >>= 1;
    }
}

uint8_t hal_onewire_u8_read_byte( void ){

    uint8_t byte = 0;

    RingbufHandle_t ringbuf;
    rmt_get_ringbuf_handle(RX_CHANNEL, &ringbuf);
    

    flush_rx();

    rmt_item32_t items[9] = {0};

    for( uint8_t i = 0; i < 8; i++ ){    

        // set up read slot
        items[i].duration0 = ONEWIRE_DELAY_A;
        items[i].level0 = 0;
        items[i].duration1 = ONEWIRE_DELAY_E + ONEWIRE_DELAY_F;
        items[i].level1 = 1;
    }

    // end idle condition
    items[8].duration0 = 0;
    items[8].level0 = 1;
    items[8].duration1 = 0;
    items[8].level1 = 1;


    rmt_rx_start(RX_CHANNEL, true);

    rmt_write_items(TX_CHANNEL, items, cnt_of_array(items), true);    

    uint32_t rx_size = 0;
    rmt_item32_t *rx_items = (rmt_item32_t *)xRingbufferReceive(ringbuf, &rx_size, 20 / portTICK_PERIOD_MS);

    if( rx_items != 0 ){

        uint32_t rx_count = rx_size / sizeof(rmt_item32_t);

        // log_v_debug_P( PSTR("rx: %d"), rx_count );

        // for (int j = 0; j < rx_count; j++)
        // {
        //     log_v_debug_P(PSTR("level0: %d, duration %d"), rx_items[j].level0, rx_items[j].duration0);
        //     log_v_debug_P(PSTR("level1: %d, duration %d"), rx_items[j].level1, rx_items[j].duration1);
        // }
        
        if( rx_count >= 8 ){

            for( uint8_t i = 0; i < 8; i++ ){    

                byte >>= 1;

                // verify presence signal in second item
                if( ( rx_items[i].level0 == 0 ) && ( rx_items[i].duration0 > ONEWIRE_DELAY_E ) ){

                    // this is a zero
                }
                else if( rx_items[i].level0 == 0 ){

                    // this is a one
                    byte |= 0x80;
                }
            }
        }
    }

    rmt_rx_stop(RX_CHANNEL);


    return byte;
}

