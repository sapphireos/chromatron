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


#include "cpu.h"

#include "system.h"
#include "target.h"
#include "flash_fs.h"
#include "hal_boards.h"

#include "hal_io.h"

#ifndef BOOTLOADER
static io_mode_t8 io_modes[IO_PIN_COUNT];
#endif

static const gpio_num_t gpios_v0_0[IO_PIN_COUNT] = {
    // feather pinout - need to change this
    GPIO_NUM_13, // IO_PIN_13_A12 
    GPIO_NUM_12, // IO_PIN_12_A11 
    GPIO_NUM_27, // IO_PIN_27_A10 
    GPIO_NUM_33, // IO_PIN_33_A9  
    GPIO_NUM_15, // IO_PIN_15_A8  
    GPIO_NUM_32, // IO_PIN_32_A7  
    GPIO_NUM_14, // IO_PIN_14_A6  
    GPIO_NUM_22, // IO_PIN_22_SCL 
    GPIO_NUM_23, // IO_PIN_23_SDA 
    GPIO_NUM_21, // IO_PIN_21     
    GPIO_NUM_17, // IO_PIN_17_TX  
    GPIO_NUM_16, // IO_PIN_16_RX  
    GPIO_NUM_19, // IO_PIN_19_MISO
    GPIO_NUM_18, // IO_PIN_18_MOSI
    GPIO_NUM_5,  // IO_PIN_5_SCK  
    GPIO_NUM_4,  // IO_PIN_4_A5   
    GPIO_NUM_36, // IO_PIN_36_A4  
    GPIO_NUM_39, // IO_PIN_39_A3  
    GPIO_NUM_34, // IO_PIN_34_A2  
    GPIO_NUM_25, // IO_PIN_25_A1  
    GPIO_NUM_26, // IO_PIN_26_A0  

    // chromatron32 v0.0
    GPIO_NUM_13, // IO_PIN_LED0
    GPIO_NUM_2,  // IO_PIN_LED1
    GPIO_NUM_15, // IO_PIN_LED2
};

static const gpio_num_t gpios_v0_1[IO_PIN_COUNT] = {
    // feather pinout - need to change this
    GPIO_NUM_13, // IO_PIN_13_A12 
    GPIO_NUM_12, // IO_PIN_12_A11 
    GPIO_NUM_27, // IO_PIN_27_A10 
    GPIO_NUM_33, // IO_PIN_33_A9  
    GPIO_NUM_15, // IO_PIN_15_A8  
    GPIO_NUM_32, // IO_PIN_32_A7  
    GPIO_NUM_14, // IO_PIN_14_A6  
    GPIO_NUM_22, // IO_PIN_22_SCL 
    GPIO_NUM_23, // IO_PIN_23_SDA 
    GPIO_NUM_21, // IO_PIN_21     
    GPIO_NUM_17, // IO_PIN_17_TX  
    GPIO_NUM_16, // IO_PIN_16_RX  
    GPIO_NUM_19, // IO_PIN_19_MISO
    GPIO_NUM_18, // IO_PIN_18_MOSI
    GPIO_NUM_5,  // IO_PIN_5_SCK  
    GPIO_NUM_4,  // IO_PIN_4_A5   
    GPIO_NUM_36, // IO_PIN_36_A4  
    GPIO_NUM_39, // IO_PIN_39_A3  
    GPIO_NUM_34, // IO_PIN_34_A2  
    GPIO_NUM_25, // IO_PIN_25_A1  
    GPIO_NUM_26, // IO_PIN_26_A0  

    // chromatron32 v0.1
    GPIO_NUM_15, // IO_PIN_LED0
    GPIO_NUM_4,  // IO_PIN_LED1
    GPIO_NUM_2, // IO_PIN_LED2
};


static const gpio_num_t gpios_wrover_kit[IO_PIN_COUNT] = {
    GPIO_NUM_13, // IO_PIN_13_A12 
    GPIO_NUM_12, // IO_PIN_12_A11 
    GPIO_NUM_27, // IO_PIN_27_A10 
    GPIO_NUM_33, // IO_PIN_33_A9  
    GPIO_NUM_15, // IO_PIN_15_A8  
    GPIO_NUM_32, // IO_PIN_32_A7  
    GPIO_NUM_14, // IO_PIN_14_A6  
    GPIO_NUM_22, // IO_PIN_22_SCL 
    GPIO_NUM_23, // IO_PIN_23_SDA 
    GPIO_NUM_21, // IO_PIN_21     
    GPIO_NUM_17, // IO_PIN_17_TX  
    GPIO_NUM_16, // IO_PIN_16_RX  
    GPIO_NUM_19, // IO_PIN_19_MISO
    GPIO_NUM_18, // IO_PIN_18_MOSI
    GPIO_NUM_5,  // IO_PIN_5_SCK  
    GPIO_NUM_4,  // IO_PIN_4_A5   
    GPIO_NUM_36, // IO_PIN_36_A4  
    GPIO_NUM_39, // IO_PIN_39_A3  
    GPIO_NUM_34, // IO_PIN_34_A2  
    GPIO_NUM_25, // IO_PIN_25_A1  
    GPIO_NUM_26, // IO_PIN_26_A0  

    GPIO_NUM_0, // IO_PIN_LED0
    GPIO_NUM_4, // IO_PIN_LED1
    GPIO_NUM_2, // IO_PIN_LED2
};


static const gpio_num_t gpios_elite[IO_PIN_COUNT] = {
    GPIO_NUM_13, // IO_PIN_13_A12 
    GPIO_NUM_12, // IO_PIN_12_A11 
    GPIO_NUM_27, // IO_PIN_27_A10 
    GPIO_NUM_33, // IO_PIN_33_A9  
    GPIO_NUM_15, // IO_PIN_15_A8  
    GPIO_NUM_32, // IO_PIN_32_A7  
    GPIO_NUM_14, // IO_PIN_14_A6  
    GPIO_NUM_22, // IO_PIN_22_SCL 
    GPIO_NUM_23, // IO_PIN_23_SDA 
    GPIO_NUM_21, // IO_PIN_21     
    GPIO_NUM_17, // IO_PIN_17_TX  
    GPIO_NUM_16, // IO_PIN_16_RX  
    GPIO_NUM_19, // IO_PIN_19_MISO
    GPIO_NUM_18, // IO_PIN_18_MOSI
    GPIO_NUM_5,  // IO_PIN_5_SCK  
    GPIO_NUM_4,  // IO_PIN_4_A5   
    GPIO_NUM_36, // IO_PIN_36_A4  
    GPIO_NUM_39, // IO_PIN_39_A3  
    GPIO_NUM_34, // IO_PIN_34_A2  
    GPIO_NUM_25, // IO_PIN_25_A1  
    GPIO_NUM_26, // IO_PIN_26_A0  

    GPIO_NUM_0,  // IO_PIN_LED0
    GPIO_NUM_15, // IO_PIN_LED1
    GPIO_NUM_2,  // IO_PIN_LED2
};

static const gpio_num_t *gpios = gpios_v0_1;

int32_t hal_io_i32_get_gpio_num( uint8_t pin ){

    #ifndef BOOTLOADER
    ASSERT( pin < IO_PIN_COUNT );

    return gpios[pin];

    #else

    return -1;

    #endif
}

bool hal_io_b_is_board_type_known( void ){

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_UNKNOWN ){

        return FALSE;
    }
    else if( board >= BOARD_TYPE_COUNT ){

        return FALSE;
    }

    return TRUE;
}

void io_v_init( void ){

    // check board type
    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_CHROMATRON32_v0_0 ){

        gpios = gpios_v0_0;
    }
    else if( board == BOARD_TYPE_CHROMATRON32_v0_1 ){

        gpios = gpios_v0_1;
    }
    else if( board == BOARD_TYPE_WROVER_KIT ){

        gpios = gpios_wrover_kit;
    }
    else if( board == BOARD_TYPE_ELITE ){

        gpios = gpios_elite;
    }
    else{

        trace_printf("Unknown board type, setting default IO map.\r\n");
    }
}

uint8_t io_u8_get_board_rev( void ){

    return 0;
}


void io_v_set_mode( uint8_t pin, io_mode_t8 mode ){

    #ifndef BOOTLOADER

    ASSERT( pin < IO_PIN_COUNT );

    io_modes[pin] = mode;

    gpio_num_t gpio = gpios[pin];
	
    // need to reset pin first to initialize it.
    gpio_reset_pin( gpio );

    if( mode == IO_MODE_INPUT ){

        gpio_set_direction( gpio, GPIO_MODE_INPUT );
        gpio_set_pull_mode( gpio, GPIO_FLOATING );
    }
    else if( mode == IO_MODE_INPUT_PULLUP ){

        gpio_set_direction( gpio, GPIO_MODE_INPUT );
        gpio_set_pull_mode( gpio, GPIO_PULLUP_ONLY );
    }
    else if( mode == IO_MODE_INPUT_PULLDOWN ){

        gpio_set_direction( gpio, GPIO_MODE_INPUT );
        gpio_set_pull_mode( gpio, GPIO_PULLDOWN_ONLY );
    }
    else if( mode == IO_MODE_OUTPUT ){

        gpio_set_direction( gpio, GPIO_MODE_OUTPUT );   
    }
    else if( mode == IO_MODE_OUTPUT_OPEN_DRAIN ){

        gpio_set_direction( gpio, GPIO_MODE_OUTPUT_OD );   
    }
    else{

        ASSERT( FALSE );
    }

    #endif
}


io_mode_t8 io_u8_get_mode( uint8_t pin ){
	
    #ifndef BOOTLOADER
    ASSERT( pin < IO_PIN_COUNT );
    
    return io_modes[pin];	

    #else
    return 0;
    #endif
}

void io_v_digital_write( uint8_t pin, bool state ){

    gpio_num_t gpio = gpios[pin];

    #ifndef BOOTLOADER
    ASSERT( pin < IO_PIN_COUNT );
    
    gpio_set_level( gpio, state );	
    #else

    gpio_pad_select_gpio( gpio );
    GPIO_OUTPUT_SET( gpio, state );

    #endif
}

bool io_b_digital_read( uint8_t pin ){
    
    #ifndef BOOTLOADER
    ASSERT( pin < IO_PIN_COUNT );

    gpio_num_t gpio = gpios[pin];
    
    return gpio_get_level( gpio );
    #else
    return FALSE;
    #endif
}

bool io_b_button_down( void ){

    return FALSE;
}

void io_v_disable_jtag( void ){

}

void io_v_enable_interrupt(
    uint8_t int_number,
    io_int_handler_t handler,
    io_int_mode_t8 mode )
{

}

void io_v_disable_interrupt( uint8_t int_number )
{

}
