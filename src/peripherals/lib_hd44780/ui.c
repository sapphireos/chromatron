
#include "sapphire.h"
#include "ui.h"
#include "lcd.h"
#include "pca9685.h"
#include "buttons.h"

#ifdef ESP32

PT_THREAD( ui_thread( pt_t *pt, void *state ) );


void ui_v_init( void ){

	lcd_v_init( 20, 4 );
    lcd_v_clear();       
    lcd_v_printf_P( 0, 0, PSTR("JEREMY ROCKS        ") );
    
    pca9685_v_init( PCA9685_I2C_ADDR_0 );
    pca9685_v_set_freq( 4 );

    // all off
    // pca9685_v_set( 0, 4095 );
    // pca9685_v_set( 2, 4095 );
    // pca9685_v_set( 4, 4095 );

    // pca9685_v_set( 0, 0 ); // full on red
    // pca9685_v_set( 2, 4095 );
    // pca9685_v_set( 4, 4095 );

    // pca9685_v_set( 0, 4095 ); 
    // pca9685_v_set( 2, 0 ); // full on green
    // pca9685_v_set( 4, 4095 );

    pca9685_v_set( 0, 4095 ); 
    pca9685_v_set( 2, 4095 ); 
    pca9685_v_set( 4, 0 ); // full on blue

    thread_t_create( ui_thread,
                 PSTR("ui"),
                 0,
                 0 );

}



PT_THREAD( ui_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    while(1){

        TMR_WAIT( pt, 100 );

        
    }
    
PT_END( pt );
}

#endif