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

PT_THREAD( io_thread( pt_t *pt, void *state ) );


// static uint8_t hash_to_pin( catbus_hash_t32 hash ){

//     switch( hash ){
//         case __KV__io_cfg_gpio_0:
//             return IO_PIN_0_GPIO;
//             break;

//         case __KV__io_cfg_xck_1:
//             return IO_PIN_1_XCK;
//             break;

//         case __KV__io_cfg_txd_2:
//             return IO_PIN_2_TXD;
//             break;

//         case __KV__io_cfg_rxd_3:
//             return IO_PIN_3_RXD;
//             break;

//         case __KV__io_cfg_adc0_4:
//             return IO_PIN_4_ADC0;
//             break;

//         case __KV__io_cfg_adc1_5:
//             return IO_PIN_5_ADC1;
//             break;

//         case __KV__io_cfg_dac0_6:
//             return IO_PIN_6_DAC0;
//             break;

//         case __KV__io_cfg_dac1_7:
//             return IO_PIN_7_DAC1;
//             break;

//         case __KV__io_val_gpio_0:
//             return IO_PIN_0_GPIO;
//             break;

//         case __KV__io_val_xck_1:
//             return IO_PIN_1_XCK;
//             break;

//         case __KV__io_val_txd_2:
//             return IO_PIN_2_TXD;
//             break;

//         case __KV__io_val_rxd_3:
//             return IO_PIN_3_RXD;
//             break;

//         case __KV__io_val_adc0_4:
//             return IO_PIN_4_ADC0;
//             break;

//         case __KV__io_val_adc1_5:
//             return IO_PIN_5_ADC1;
//             break;

//         case __KV__io_val_dac0_6:
//             return IO_PIN_6_DAC0;
//             break;

//         case __KV__io_val_dac1_7:
//             return IO_PIN_7_DAC1;
//             break;

//         default:
//             return 0;
//     }
// }

static int8_t _iokv_i8_cfg_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){
    
    if( op == KV_OP_GET ){

        return 0;
    }
    else if( op == KV_OP_SET ){


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

        return 0;
    }
    else if( op == KV_OP_SET ){


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


void iokv_v_init( void ){

    thread_t_create( io_thread,
                     PSTR("io"),
                     0,
                     0 );
}

PT_THREAD( io_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    while(1){

        TMR_WAIT( pt, 100 );
    }

PT_END( pt );
}








