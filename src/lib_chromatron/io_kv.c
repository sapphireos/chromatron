/*
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
 */


#include "sapphire.h"
#include "io_kv.h"
#include "io_intf.h"


static uint8_t analog_mode;


static uint8_t hash_to_pin( catbus_hash_t32 hash ){

    switch( hash ){
        case __KV__io_cfg_gpio_0:
            return IO_PIN_0_GPIO;
            break;

        case __KV__io_cfg_xck_1:
            return IO_PIN_1_XCK;
            break;

        case __KV__io_cfg_txd_2:
            return IO_PIN_2_TXD;
            break;

        case __KV__io_cfg_rxd_3:
            return IO_PIN_3_RXD;
            break;

        case __KV__io_cfg_adc0_4:
            return IO_PIN_4_ADC0;
            break;

        case __KV__io_cfg_adc1_5:
            return IO_PIN_5_ADC1;
            break;

        case __KV__io_cfg_dac0_6:
            return IO_PIN_6_DAC0;
            break;

        case __KV__io_cfg_dac1_7:
            return IO_PIN_7_DAC1;
            break;

        case __KV__io_val_gpio_0:
            return IO_PIN_0_GPIO;
            break;

        case __KV__io_val_xck_1:
            return IO_PIN_1_XCK;
            break;

        case __KV__io_val_txd_2:
            return IO_PIN_2_TXD;
            break;

        case __KV__io_val_rxd_3:
            return IO_PIN_3_RXD;
            break;

        case __KV__io_val_adc0_4:
            return IO_PIN_4_ADC0;
            break;

        case __KV__io_val_adc1_5:
            return IO_PIN_5_ADC1;
            break;

        case __KV__io_val_dac0_6:
            return IO_PIN_6_DAC0;
            break;

        case __KV__io_val_dac1_7:
            return IO_PIN_7_DAC1;
            break;

        default:
            return 0;
    }
}

static int8_t _iokv_i8_cfg_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        io_mode_t8 mode = io_u8_get_mode( hash_to_pin( hash ) );        

        uint8_t pin_mode = 0;

        // check analog mode
        if( hash == __KV__io_cfg_adc0_4 ){

            if( analog_mode & 0x01 ){

                pin_mode = IO_CFG_ANALOG;
            }
        }
        else if( hash == __KV__io_cfg_adc1_5 ){

            if( analog_mode & 0x02 ){

                pin_mode = IO_CFG_ANALOG;
            }
        }
        else if( hash == __KV__io_cfg_dac0_6 ){

            if( analog_mode & 0x04 ){

                pin_mode = IO_CFG_ANALOG;
            }
        }
        else if( hash == __KV__io_cfg_dac1_7 ){

            if( analog_mode & 0x08 ){

                pin_mode = IO_CFG_ANALOG;
            }
        }
        
        if( pin_mode != IO_CFG_ANALOG ){

            if( mode == IO_MODE_INPUT ){

                pin_mode = IO_CFG_INPUT;
            }
            else if( mode == IO_CFG_INPUT_PULLUP ){

                pin_mode = IO_CFG_INPUT_PULLUP;
            }
            else if( mode == IO_CFG_INPUT_PULLDOWN ){

                pin_mode = IO_CFG_INPUT_PULLDOWN;
            }
            else if( mode == IO_CFG_OUTPUT ){

                pin_mode = IO_CFG_OUTPUT;
            }
            else if( mode == IO_CFG_OUTPUT_OPEN_DRAIN ){

                pin_mode = IO_CFG_OUTPUT_OPEN_DRAIN;
            }
        }

        *( (uint8_t *)data ) = pin_mode;

        return 0;
    }
    else if( op == KV_OP_SET ){

        uint8_t pin_mode = *( (uint8_t *)data );
        uint8_t mode = 0;

        // clear analog setting
        if( hash == __KV__io_cfg_adc0_4 ){

            analog_mode &= ~0x01;
        }
        else if( hash == __KV__io_cfg_adc1_5 ){

            analog_mode &= ~0x02;
        }
        else if( hash == __KV__io_cfg_dac0_6 ){

            analog_mode &= ~0x04;
        }
        else if( hash == __KV__io_cfg_dac1_7 ){

            analog_mode &= ~0x08;
        }

        if( pin_mode == IO_CFG_INPUT ){

            mode = IO_MODE_INPUT;
        }
        else if( pin_mode == IO_CFG_INPUT_PULLUP ){

            mode = IO_MODE_INPUT_PULLUP;
        }
        else if( pin_mode == IO_CFG_INPUT_PULLDOWN ){

            mode = IO_MODE_INPUT_PULLDOWN;
        }
        else if( pin_mode == IO_CFG_OUTPUT ){

            mode = IO_MODE_OUTPUT;
        }
        else if( pin_mode == IO_CFG_OUTPUT_OPEN_DRAIN ){

            mode = IO_MODE_OUTPUT_OPEN_DRAIN;
        }
        else if( pin_mode == IO_CFG_ANALOG ){

            if( hash == __KV__io_cfg_adc0_4 ){

                analog_mode |= 0x01;
            }
            else if( hash == __KV__io_cfg_adc1_5 ){

                analog_mode |= 0x02;
            }
            else if( hash == __KV__io_cfg_dac0_6 ){

                analog_mode |= 0x04;
            }
            else if( hash == __KV__io_cfg_dac1_7 ){

                analog_mode |= 0x08;
            }
            else{

                // invalid channel
                return 0;
            }

            mode = IO_MODE_INPUT;
        }

        io_v_set_mode( hash_to_pin( hash ), mode );

        return 0;
    }

    return -1;
}


static int8_t _iokv_i8_val_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        uint16_t io_data = 0;

        // check analog mode
        if( ( hash == __KV__io_val_adc0_4 ) && ( analog_mode & 0x01 ) ){

            io_data = adc_u16_read_mv( ANALOG_CHANNEL_ADC0 );
        }
        else if( ( hash == __KV__io_val_adc1_5 ) && ( analog_mode & 0x02 ) ){

            io_data = adc_u16_read_mv( ANALOG_CHANNEL_ADC1 );
        }
        else if( ( hash == __KV__io_val_dac0_6 ) && ( analog_mode & 0x04 ) ){

            io_data = adc_u16_read_mv( ANALOG_CHANNEL_DAC0 );
        }
        else if( ( hash == __KV__io_val_dac1_7 ) && ( analog_mode & 0x08 ) ){

            io_data = adc_u16_read_mv( ANALOG_CHANNEL_DAC1 );
        }
        else{

            io_data = io_b_digital_read( hash_to_pin( hash ) );
        }

        *( (uint16_t *)data ) = io_data;

        return 0;
    }
    else if( op == KV_OP_SET ){

        // check analog mode
        if( ( hash == __KV__io_cfg_adc0_4 ) && ( analog_mode & 0x01 ) ){

            return 0;
        }
        else if( ( hash == __KV__io_cfg_adc1_5 ) && ( analog_mode & 0x02 ) ){

            return 0;
        }
        else if( ( hash == __KV__io_cfg_dac0_6 ) && ( analog_mode & 0x04 ) ){

            return 0;
        }
        else if( ( hash == __KV__io_cfg_dac1_7 ) && ( analog_mode & 0x08 ) ){

            return 0;
        }
        else{

            io_v_digital_write( hash_to_pin( hash ), *( (uint16_t *)data ) );
        }

        return 0;
    }

    return -1;
}

KV_SECTION_META kv_meta_t iokv_cfg[] = {
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_gpio_0" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_xck_1" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_txd_2" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_rxd_3" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_adc0_4" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_adc1_5" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_dac0_6" },
    { SAPPHIRE_TYPE_UINT8,   0, 0, 0,  _iokv_i8_cfg_handler, "io_cfg_dac1_7" },
};

KV_SECTION_META kv_meta_t iokv_val[] = {
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_gpio_0" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_xck_1" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_txd_2" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_rxd_3" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_adc0_4" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_adc1_5" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_dac0_6" },
    { SAPPHIRE_TYPE_UINT16,  0, 0, 0,  _iokv_i8_val_handler, "io_val_dac1_7" },
};



// PT_THREAD( io_thread( pt_t *pt, void *state ) );

void iokv_v_init( void ){

    // thread_t_create( io_thread,
    //                  PSTR("io"),
    //                  0,
    //                  0 );
}

// PT_THREAD( io_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

//     while(1){

//         TMR_WAIT( pt, 100 );
//     }

// PT_END( pt );
// }








