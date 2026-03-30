
#include "sapphire.h"
#include "ui.h"
#include "lcd.h"
#include "pca9685.h"
#include "buttons.h"

#ifdef ESP32

int8_t _lcd_i8_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len ){

    if( op == KV_OP_SET ){

        if( hash == __KV__ui_line0 ){

        		lcd_v_write_P( (char *)data, 0, 0 );
        }
    }
    else if( op == KV_OP_GET ){

    	memset( data, 0, len );
    }

    return 0;
}

KV_SECTION_META kv_meta_t lcd_ui_enable_kv[] = {
    { CATBUS_TYPE_BOOL, 0, KV_FLAGS_PERSIST, 0, 0, "lcd_enable" },
};

KV_SECTION_OPT kv_meta_t lcd_ui_kv[] = {
    { CATBUS_TYPE_STRING32, 0, 0, 0, _lcd_i8_kv_handler, "lcd_line0" },
};

void set_backlight( uint16_t r, uint16_t g, uint16_t b, uint16_t fade );

PT_THREAD( ui_thread( pt_t *pt, void *state ) );


void ui_v_init( void ){

    if( !kv_b_get_boolean( __KV__lcd_enable ) ){

        return;
    }

    kv_v_add_db_info( lcd_ui_kv, sizeof(lcd_ui_kv) );

	// lcd_v_init( 20, 4 );
    lcd_v_init( 40, 4 );
    lcd_v_clear();       
   	
   	lcd_v_printf_P( 0, 0, PSTR("SapphireOS") );
   	lcd_v_printf_P( 0, 1, PSTR("UI init...") );
    lcd_v_printf_P( 0, 2, PSTR("Line 2") );
    lcd_v_printf_P( 21, 2, PSTR("Pos 21") );
    lcd_v_printf_P( 0, 3, PSTR("Line 3") );

    pca9685_v_init( PCA9685_I2C_ADDR_0 );
    pca9685_v_set_freq( 4 );

    // set_backlight( PCA9685_MAX_PWM, PCA9685_MAX_PWM, PCA9685_MAX_PWM, 1000 );
	set_backlight( 0, 0, 0, 1000 );

    // pca9685_v_set( 0, 0 );
    // pca9685_v_set( 2, 0 );
    // pca9685_v_set( 4, 0 );


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

    // pca9685_v_set( 0, 4095 ); 
    // pca9685_v_set( 2, 4095 ); 
    // pca9685_v_set( 4, 0 ); // full on blue

    thread_t_create( ui_thread,
                 PSTR("ui"),
                 0,
                 0 );

}


void set_backlight( uint16_t r, uint16_t g, uint16_t b, uint16_t fade ){

	if( r > PCA9685_MAX_PWM ){

		r = PCA9685_MAX_PWM;
	}

	if( g > PCA9685_MAX_PWM ){

		g = PCA9685_MAX_PWM;
	}

	if( b > PCA9685_MAX_PWM ){

		b = PCA9685_MAX_PWM;
	}

	pca9685_v_fade( 0, r, fade );
	pca9685_v_fade( 1, r, fade );

	pca9685_v_fade( 2, g, fade );
	pca9685_v_fade( 3, g, fade );

	pca9685_v_fade( 4, b, fade );
	pca9685_v_fade( 5, b, fade );
}


PT_THREAD( ui_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	TMR_WAIT( pt, 1000 );

    while(1){

        TMR_WAIT( pt, 100 );

        
    }
    
PT_END( pt );
}

#endif