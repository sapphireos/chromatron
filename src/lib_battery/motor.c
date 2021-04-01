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

#include "motor.h"

static bool drive0;
static bool drive1;

static uint16_t position;

KV_SECTION_META kv_meta_t motor_info_kv[] = {
    { SAPPHIRE_TYPE_BOOL,   0, 0,    &drive0, 0,          "motor_drive_0" },
    { SAPPHIRE_TYPE_BOOL,   0, 0,    &drive1, 0,          "motor_drive_1" },
    { SAPPHIRE_TYPE_UINT16, 0, 0,    &position, 0,        "motor_position" },
};

#define MOTOR_IO_0 	IO_PIN_16_RX
#define MOTOR_IO_1 	IO_PIN_17_TX

#define MOTOR_IRQ	GPIO_NUM_27

static void IRAM_ATTR motor_irq_handler(void* arg){

	position++;
}



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

	while(1){

		TMR_WAIT( pt, 100 );

		io_v_digital_write( MOTOR_IO_0, 0 );
		io_v_digital_write( MOTOR_IO_1, 0 );

		if( drive0 ){

			io_v_digital_write( MOTOR_IO_0, 1 );
		}

		if( drive1 ){

			io_v_digital_write( MOTOR_IO_1, 1 );
		}
	}


PT_END( pt );	
}


void motor_v_init( void ){

    thread_t_create( motor_thread,
                     PSTR("motor"),
                     0,
                     0 );
}

