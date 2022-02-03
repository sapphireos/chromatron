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

#include "sapphire.h"

#if 0

#include "motor.h"

static uint8_t motor_state;
static uint8_t prev_dir;
static uint16_t target_position;
static volatile uint16_t position;

static int16_t diff;

KV_SECTION_META kv_meta_t motor_info_kv[] = {
    { CATBUS_TYPE_UINT8,  0, 0,    &motor_state,              0,        "motor_state" },
    { CATBUS_TYPE_UINT16, 0, 0,    &target_position,          0,        "motor_target" },
    { CATBUS_TYPE_UINT16, 0, 0,    (uint16_t *)&position,     0,        "motor_position" },

    { CATBUS_TYPE_INT32,  0, 0,    &diff,                     0,        "motor_diff" },
};

#define MOTOR_IO_0 	IO_PIN_16_RX
#define MOTOR_IO_1 	IO_PIN_17_TX

#define MOTOR_IRQ	GPIO_NUM_27


#define MOTOR_STATE_OFF     0
#define MOTOR_STATE_UP      1 // the actual directions don't matter
#define MOTOR_STATE_DOWN    2


#define COUNTS_PER_CYCLE    171


static void IRAM_ATTR motor_irq_handler(void* arg){

    if( prev_dir == MOTOR_STATE_UP ){

        position++;    

        if( position >= COUNTS_PER_CYCLE ){

           position = 0;
        }
    }
    else{

        position--;

        if( position >= COUNTS_PER_CYCLE ){

            position = COUNTS_PER_CYCLE - 1;
        }
    }
	
    if( position == target_position ){

        motor_state = MOTOR_STATE_OFF;

        io_v_digital_write( MOTOR_IO_0, 0 );
        io_v_digital_write( MOTOR_IO_1, 0 );
    }
}


// gear train:
// motor encoder: 8 counts per turn
// motor output: 6 -> 24
// drive: 12 -> 64
// reduction: 4 * 5.3333333
// total reduction: 1:21.3333333
// counts per cycle: approx 171

PT_THREAD( motor_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

	gpio_install_isr_service( 0 );
	gpio_isr_handler_add( MOTOR_IRQ, motor_irq_handler, 0 );
		
	gpio_set_direction( MOTOR_IRQ, GPIO_MODE_INPUT );
	gpio_set_pull_mode( MOTOR_IRQ, GPIO_PULLUP_ONLY );
	gpio_set_intr_type( MOTOR_IRQ, GPIO_INTR_ANYEDGE );
	gpio_intr_enable( MOTOR_IRQ );	



	io_v_set_mode( MOTOR_IO_0, IO_MODE_OUTPUT );
	io_v_set_mode( MOTOR_IO_1, IO_MODE_OUTPUT );

    io_v_digital_write( MOTOR_IO_0, 0 );
    io_v_digital_write( MOTOR_IO_1, 0 );

    TMR_WAIT( pt, 200 );

    position = 0;


	while(1){

        if( target_position >= ( COUNTS_PER_CYCLE  - 1 ) ){

            target_position %= COUNTS_PER_CYCLE;
        }
		
        diff = (int32_t)target_position - (int32_t)position;
        // diff = util_i8_compare_sequence_u16( target_position, position );

        int16_t abs_diff = abs16( diff );

        // compute shortest distance by direction
        if( abs_diff > COUNTS_PER_CYCLE / 2 ){

            if( diff > 0 ){

                diff -= COUNTS_PER_CYCLE;
            }
            else if( diff < 0 ){

                diff += COUNTS_PER_CYCLE;
            }
        }

        if( abs_diff < 4 ){

        // }
        // else if( diff == 0 ){

            motor_state = MOTOR_STATE_OFF;
        }
        else if( diff > 0 ){

            motor_state = MOTOR_STATE_UP;
        }
        else if( diff < 0 ){

            motor_state = MOTOR_STATE_DOWN;
        }

        if( motor_state == MOTOR_STATE_OFF ){

            io_v_digital_write( MOTOR_IO_0, 0 );
            io_v_digital_write( MOTOR_IO_1, 0 );
        }
        else if( motor_state == MOTOR_STATE_UP ){

            prev_dir = MOTOR_STATE_UP;

            io_v_digital_write( MOTOR_IO_0, 1 );
            io_v_digital_write( MOTOR_IO_1, 0 );
        }
        else if( motor_state == MOTOR_STATE_DOWN ){

            prev_dir = MOTOR_STATE_DOWN;

            io_v_digital_write( MOTOR_IO_0, 0 );
            io_v_digital_write( MOTOR_IO_1, 1 );
        }
        else{

            motor_state = MOTOR_STATE_OFF;
        }


        TMR_WAIT( pt, 100 );
	}

PT_END( pt );	
}


void motor_v_init( void ){

    thread_t_create( motor_thread,
                     PSTR("motor"),
                     0,
                     0 );
}

#else


void motor_v_init( void ){

}

#endif