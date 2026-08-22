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

#include "mpu9250.h"
// #include "fix16.h"

static uint8_t i2c_addr;

static int16_t accel_x;
static int16_t accel_y;
static int16_t accel_z;
// static fix16_t f16_accel_x;
// static fix16_t f16_accel_y;
// static fix16_t f16_accel_z;

static int16_t gyro_x;
static int16_t gyro_y;
static int16_t gyro_z;

KV_SECTION_OPT kv_meta_t mpu9250_info_kv[] = {
    { CATBUS_TYPE_INT16,   0, KV_FLAGS_READ_ONLY,  &accel_x,             0,   "accel_x" },
    { CATBUS_TYPE_INT16,   0, KV_FLAGS_READ_ONLY,  &accel_y,             0,   "accel_y" },
    { CATBUS_TYPE_INT16,   0, KV_FLAGS_READ_ONLY,  &accel_z,             0,   "accel_z" },

    // { CATBUS_TYPE_FIXED16, 0, KV_FLAGS_READ_ONLY,  &f16_accel_x,         0,   "accel_x_f16" },
    // { CATBUS_TYPE_FIXED16, 0, KV_FLAGS_READ_ONLY,  &f16_accel_y,         0,   "accel_y_f16" },
    // { CATBUS_TYPE_FIXED16, 0, KV_FLAGS_READ_ONLY,  &f16_accel_z,         0,   "accel_z_f16" },

	{ CATBUS_TYPE_INT16,   0, KV_FLAGS_READ_ONLY,  &gyro_x,             0,   "gyro_x" },
    { CATBUS_TYPE_INT16,   0, KV_FLAGS_READ_ONLY,  &gyro_y,             0,   "gyro_y" },
    { CATBUS_TYPE_INT16,   0, KV_FLAGS_READ_ONLY,  &gyro_z,             0,   "gyro_z" },

};


static void _mpu9250_v_write_reg( uint8_t reg_addr, uint8_t data ){

	i2c_v_write_reg8( i2c_addr, reg_addr, data );
}

static uint8_t _mpu9250_u8_read_reg( uint8_t reg_addr ){

	return i2c_u8_read_reg8( i2c_addr, reg_addr );
}


static void _mpu9250_v_read_regs( uint8_t reg_addr, uint8_t *data, uint8_t len ){

	return i2c_v_mem_read( i2c_addr, reg_addr, sizeof(uint8_t), data, len, 1 );
}

PT_THREAD( mpu9250_sensor_thread( pt_t *pt, void *state ) );



/*

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

The AMG8833 and MPU9250 use the same 2 I2C addresses!!!

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

*/

void mpu9250_v_init( void ){

	i2c_v_init( I2C_BAUD_400K );
    // i2c_v_set_pins( IO_PIN_6_DAC0, IO_PIN_7_DAC1 );

    i2c_addr = MPU9250_I2C_ADDR_0;

    // check connectivity
    if( _mpu9250_u8_read_reg( MPU9250_REG_WHO_AM_I ) != MPU9250_VAL_WHO_AM_I ){

    	i2c_addr = MPU9250_I2C_ADDR_1;

    	if( _mpu9250_u8_read_reg( MPU9250_REG_WHO_AM_I ) != MPU9250_VAL_WHO_AM_I ){

    		i2c_addr = 0;

    		// device not present
    		return;
    	}
    }

    kv_v_add_db_info( mpu9250_info_kv, sizeof(mpu9250_info_kv) );
    
    log_v_debug_P( PSTR("MPU9250 found at 0x%02x"), i2c_addr );

    mpu9250_v_set_accel_scale( MPU9250_ACCEL_SCALE_2G );

	// sensor detected, start threads
    thread_t_create( mpu9250_sensor_thread,
                     PSTR("mpu9250_sensor"),
                     0,
                     0 );    
}

void mpu9250_v_set_accel_scale( uint8_t scale ){

	if( scale > MPU9250_ACCEL_SCALE_16G ){

		scale = MPU9250_ACCEL_SCALE_16G;
	}

	uint8_t temp = _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_CONFIG );
	temp &= ~MPU9250_BIT_ACCEL_SCALE_MASK;

	temp |= ( scale << MPU9250_BIT_ACCEL_SCALE_SHIFT );

	_mpu9250_v_write_reg( MPU9250_REG_ACCEL_CONFIG, temp );
}

int16_t mpu9250_i16_read_accel_x( void ){

	int16_t temp = _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_XOUT_H ) << 8;
	temp |= _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_XOUT_L );

	return temp;
}

int16_t mpu9250_i16_read_accel_y( void ){

	int16_t temp = _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_YOUT_H ) << 8;
	temp |= _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_YOUT_L );

	return temp;
}

int16_t mpu9250_i16_read_accel_z( void ){

	int16_t temp = _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_ZOUT_H ) << 8;
	temp |= _mpu9250_u8_read_reg( MPU9250_REG_ACCEL_ZOUT_L );

	return temp;
}


int16_t mpu9250_i16_read_gyro_x( void ){

	int16_t temp = _mpu9250_u8_read_reg( MPU9250_REG_GYRO_XOUT_H ) << 8;
	temp |= _mpu9250_u8_read_reg( MPU9250_REG_GYRO_XOUT_L );

	return temp;
}

int16_t mpu9250_i16_read_gyro_y( void ){

	int16_t temp = _mpu9250_u8_read_reg( MPU9250_REG_GYRO_YOUT_H ) << 8;
	temp |= _mpu9250_u8_read_reg( MPU9250_REG_GYRO_YOUT_L );

	return temp;
}

int16_t mpu9250_i16_read_gyro_z( void ){

	int16_t temp = _mpu9250_u8_read_reg( MPU9250_REG_GYRO_ZOUT_H ) << 8;
	temp |= _mpu9250_u8_read_reg( MPU9250_REG_GYRO_ZOUT_L );

	return temp;
}


PT_THREAD( mpu9250_sensor_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

	while( 1 ){

		TMR_WAIT( pt, 20 );

		uint8_t regs[14];
		_mpu9250_v_read_regs( MPU9250_REG_ACCEL_XOUT_H, regs, sizeof(regs) );

		accel_x = ( regs[0] << 8 ) + regs[1];
		accel_y = ( regs[2] << 8 ) + regs[3];
		accel_z = ( regs[4] << 8 ) + regs[5];

		gyro_x = ( regs[8] << 8 ) + regs[9];
		gyro_y = ( regs[10] << 8 ) + regs[11];
		gyro_z = ( regs[12] << 8 ) + regs[13];


		// accel_x = mpu9250_i16_read_accel_x();
		// accel_y = mpu9250_i16_read_accel_y();
		// accel_z = mpu9250_i16_read_accel_z();

		// gyro_x = mpu9250_i16_read_gyro_x();
		// gyro_y = mpu9250_i16_read_gyro_y();
		// gyro_z = mpu9250_i16_read_gyro_z();

		// f16_accel_x = (int32_t)accel_x << 2;
		// f16_accel_y = (int32_t)accel_y << 2;
		// f16_accel_z = (int32_t)accel_z << 2;

		// catbus_i8_publish( __KV__accel_x );
		// catbus_i8_publish( __KV__accel_y );
		// catbus_i8_publish( __KV__accel_z );
		// catbus_i8_publish( __KV__accel_x_f16 );
		// catbus_i8_publish( __KV__accel_y_f16 );
		// catbus_i8_publish( __KV__accel_z_f16 );
	}	

PT_END( pt );
}





