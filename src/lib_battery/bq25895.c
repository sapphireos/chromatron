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

#include "i2c.h"

#include "bq25895.h"
#include "util.h"
#include "energy.h"

#include "flash_fs.h"
#include "hal_boards.h"

#include "mppt.h"
#include "battery.h"

#ifdef ENABLE_BATTERY

static uint8_t regs[BQ25895_N_REGS];

static uint16_t batt_volts;
static uint16_t vbus_volts;
static uint16_t sys_volts;
static uint16_t batt_charge_current;
static uint16_t batt_charge_power;
static uint16_t batt_max_charge_current;
static bool batt_charging;
// static bool vbus_connected;
static uint8_t batt_fault;
static uint8_t vbus_status;
static uint8_t charge_status;
static bool dump_regs;

static uint16_t boost_voltage;
static uint16_t vindpm;
static uint16_t solar_vindpm = 5800;
static uint16_t iindpm;

// true if MCU system power is sourced from the boost converter
static bool mcu_source_pmid;

// DEBUG
static uint16_t adc_time_min = 65535;
static uint16_t adc_time_max;
static uint32_t adc_good;
static uint32_t adc_fail;

static int8_t batt_temp = -127;
static int16_t batt_temp_state;

static int8_t batt_temp_raw;

// #ifdef ESP32
// static int8_t case_temp = -127;
// static int8_t ambient_temp = -127;
// static int16_t case_temp_state;
// static int16_t ambient_temp_state;
// #endif


KV_SECTION_OPT kv_meta_t bq25895_info_kv[] = {
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &batt_temp,                  0,  "batt_temp" },
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &batt_temp_raw,              0,  "batt_temp_raw" },
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &batt_charging,              0,  "batt_charging" },
    // { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &vbus_connected,             0,  "batt_external_power" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_volts,                 0,  "batt_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &vbus_volts,                 0,  "batt_vbus_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &sys_volts,                  0,  "batt_sys_volts" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &charge_status,              0,  "batt_charge_status" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_charge_current,        0,  "batt_charge_current" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_charge_power,          0,  "batt_charge_power" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &batt_fault,                 0,  "batt_fault" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &vbus_status,                0,  "batt_vbus_status" },
    
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &batt_max_charge_current,    0,  "batt_max_charge_current" },
    
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &boost_voltage,              0,  "batt_boost_voltage" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &vindpm,                     0,  "batt_vindpm" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &solar_vindpm,               0,  "batt_solar_vindpm" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &iindpm,                     0,  "batt_iindpm" },

    { CATBUS_TYPE_BOOL,    0, 0,                   &dump_regs,                  0,  "batt_dump_regs" },

    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &adc_time_min,               0,  "batt_adc_time_min" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &adc_time_max,               0,  "batt_adc_time_max" },

    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &adc_good,                   0,  "batt_adc_reads" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &adc_fail,                   0,  "batt_adc_fails" },

    // #ifdef ESP32
    // { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &case_temp,                  0,  "batt_case_temp" },
    // { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &ambient_temp,               0,  "batt_ambient_temp" },
    // #endif

};

#define VOLTS_FILTER    32

// #define VINDPM_WALL     0
// #define VINDPM_SOLAR    solar_vindpm


#define BQ25895_THERM_FILTER 32


// PT_THREAD( bat_control_thread( pt_t *pt, void *state ) );
PT_THREAD( bat_mon_thread( pt_t *pt, void *state ) );

int8_t bq25895_i8_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    // probe for battery charger
    if( bq25895_u8_get_device_id() != BQ25895_DEVICE_ID ){

        log_v_debug_P( PSTR("batt controller not found") );

        return -1;
    }

    log_v_debug_P( PSTR("BQ25895 detected") );

    kv_v_add_db_info( bq25895_info_kv, sizeof(bq25895_info_kv) );

    bq25895_v_read_all();

    thread_t_create( bat_mon_thread,
                     PSTR("bat_mon"),
                     0,
                     0 );

    return 0;
}

void bq25895_v_read_all( void ){

    i2c_v_mem_read( BQ25895_I2C_ADDR, 0, 1, regs, sizeof(regs), 0 );
}

static uint8_t read_cached_reg( uint8_t addr ){

    return regs[addr];
}

uint8_t bq25895_u8_read_reg( uint8_t addr ){

    uint8_t data = 0;
    
    i2c_v_mem_read( BQ25895_I2C_ADDR, addr, 1, &data, sizeof(data), 0 );

    // update cache
    regs[addr] = data;

    return data;
}

void bq25895_v_write_reg( uint8_t addr, uint8_t data ){

    uint8_t cmd[2];
    cmd[0] = addr;
    cmd[1] = data;

    i2c_v_write( BQ25895_I2C_ADDR, cmd, sizeof(cmd) );

    // update cache
    regs[addr] = data;
}

void bq25895_v_set_reg_bits( uint8_t addr, uint8_t mask ){

    uint8_t data = bq25895_u8_read_reg( addr );
    data |= mask;
    bq25895_v_write_reg( addr, data );
}

void bq25895_v_clr_reg_bits( uint8_t addr, uint8_t mask ){

    uint8_t data = bq25895_u8_read_reg( addr );
    data &= ~mask;
    bq25895_v_write_reg( addr, data );
}

void bq25895_v_reset( void ){

    bq25895_v_set_reg_bits( BQ25895_REG_RESET, BQ25895_BIT_RESET );
}

void bq25895_v_set_inlim_pin( bool enable ){

    if( enable ){

        bq25895_v_set_reg_bits( BQ25895_REG_INPUT_CURRENT, BQ25895_BIT_ENABLE_ILIM_PIN );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_INPUT_CURRENT, BQ25895_BIT_ENABLE_ILIM_PIN );
    }
}


void bq25895_v_set_hiz( bool enable ){

    if( enable ){

        bq25895_v_set_reg_bits( BQ25895_REG_INPUT_CURRENT, BQ25895_BIT_ENABLE_HIZ );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_INPUT_CURRENT, BQ25895_BIT_ENABLE_HIZ );
    }
}

// current in is mA!
void bq25895_v_set_inlim( uint16_t current ){

    if( current > 3250 ){

        current = 3250;
    }
    else if( current < 100 ){

        current = 100;
    }

    uint8_t data = ( ( current - 100 ) * 63 ) / 3150;

    data &= BQ25895_MASK_INPUT_CURRENT_LIM;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_INPUT_CURRENT, BQ25895_MASK_INPUT_CURRENT_LIM );

    bq25895_v_set_reg_bits( BQ25895_REG_INPUT_CURRENT, data );
}

uint16_t bq25895_u16_get_inlim( void ){

    uint32_t data = read_cached_reg( BQ25895_REG_INPUT_CURRENT ) & BQ25895_MASK_INPUT_CURRENT_LIM;

    return ( ( data * 3150 ) / 63 ) + 100;
}

void bq25895_v_enable_adc_continuous( void ){

    bq25895_v_set_reg_bits( BQ25895_REG_ADC, BQ25895_BIT_ADC_CONV_RATE );
}

void bq25895_v_start_adc_oneshot( void ){

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_ADC );

    // check if ADC is already running:
    if( ( reg & BQ25895_BIT_ADC_CONV_START ) != 0 ){

        return;
    }

    reg &= ~BQ25895_BIT_ADC_CONV_RATE;
    reg |= BQ25895_BIT_ADC_CONV_START;

    bq25895_v_write_reg( BQ25895_REG_ADC, reg );
}

bool bq25895_b_adc_ready( void ){

    return ( bq25895_u8_read_reg( BQ25895_REG_ADC ) & BQ25895_BIT_ADC_CONV_START ) == 0;
}

static bool bq25895_b_adc_ready_cached( void ){

    return ( read_cached_reg( BQ25895_REG_ADC ) & BQ25895_BIT_ADC_CONV_START ) == 0;
}

void bq25895_v_set_boost_1500khz( void ){

    // default is 500k

    bq25895_v_clr_reg_bits( BQ25895_REG_BOOST_FREQ, BQ25895_BIT_BOOST_FREQ );
}

bool bq25895_b_is_boost_1500khz( void ){

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_BOOST_FREQ );

    return ( reg & BQ25895_BIT_BOOST_FREQ ) != 0;
}

static void set_boost_mode( bool enable ){

    if( enable ){

        bq25895_v_set_reg_bits( BQ25895_REG_BOOST_EN, BQ25895_BIT_BOOST_EN );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_BOOST_EN, BQ25895_BIT_BOOST_EN );
    }
}

void bq25895_v_set_boost_mode( bool enable ){

    // check if MCU power source is PMID and
    // disabling the boost converter is requested
    if( mcu_source_pmid && !enable ){

        // leave boost mode enabled in this case,
        // since turning it off would turn off the MCU power.

        return;
    }

    set_boost_mode( enable );
}

// forces input current limit detection
void bq25895_v_force_dpdm( void ){

    bq25895_v_set_reg_bits( BQ25895_REG_FORCE_DPDM, BQ25895_BIT_FORCE_DPDM );
}

void bq25895_v_set_minsys( uint8_t sysmin ){

    bq25895_v_clr_reg_bits( BQ25895_REG_MINSYS_EN, BQ25895_MASK_MINSYS ); 

    bq25895_v_set_reg_bits( BQ25895_REG_MINSYS_EN, ( sysmin << BQ25895_SHIFT_MINSYS ) & BQ25895_MASK_MINSYS );
}

// current in is mA!
void bq25895_v_set_fast_charge_current( uint16_t current ){

    if( current > 5056 ){

        current = 5056;
    }

    uint8_t data = ( (uint32_t)current * 127 ) / 8128;

    data &= BQ25895_MASK_FAST_CHARGE;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_FAST_CHARGE_CURRENT, BQ25895_MASK_FAST_CHARGE );

    bq25895_v_set_reg_bits( BQ25895_REG_FAST_CHARGE_CURRENT, data );
}

// current in is mA!
void bq25895_v_set_pre_charge_current( uint16_t current ){

    if( current > 1024 ){

        current = 1024;
    }

    uint8_t data = ( ( current - 64 ) * 15 ) / 960;
    data <<= BQ25895_SHIFT_PRE_CHARGE;

    data &= BQ25895_MASK_PRE_CHARGE;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_PRE_CHARGE_CURRENT, BQ25895_MASK_PRE_CHARGE );

    bq25895_v_set_reg_bits( BQ25895_REG_PRE_CHARGE_CURRENT, data );
}

// current in is mA!
void bq25895_v_set_termination_current( uint16_t current ){

    if( current > 1024 ){

        current = 1024;
    }

    uint8_t data = ( ( current - 64 ) * 15 ) / 960;

    data &= BQ25895_MASK_TERM;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_TERM_CURRENT, BQ25895_MASK_TERM );

    bq25895_v_set_reg_bits( BQ25895_REG_TERM_CURRENT, data );
}

// voltage is in mV!
void bq25895_v_set_charge_voltage( uint16_t volts ){

    // the charger itself can go up to 4608.
    // but we are limiting below that.

    if( volts > 4300 ){

        volts = 4300;
    }
    else if( volts < 3840 ){

        volts = 3840;
    }

    uint8_t data = ( ( volts - 3840 ) * 63 ) / 1008;
    data <<= BQ25895_SHIFT_CHARGE_VOLTS;

    data &= BQ25895_MASK_CHARGE_VOLTS;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_CHARGE_VOLTS, BQ25895_MASK_CHARGE_VOLTS );

    bq25895_v_set_reg_bits( BQ25895_REG_CHARGE_VOLTS, data );
}

void bq25895_v_set_batlowv( bool high ){

    /*
    
    Note from TI: https://e2e.ti.com/support/power-management-group/power-management/f/power-management-forum/710331/bq25895-batfet-turn-off-at-vbat_otg-about-2-85v-in-boost-mode

    They admit that BATLOWV will trigger turning off the BATFET, but this is not documented.  
    The datasheet just says it will turn off BOOST, but the BATFET stays on until another, 
    lower threshold.  But it actually does cut off at BATLOWV.  This is fine for us (better, actually).


    SO FAR:
    I cannot get this bit to actually clear.  It will clear if VBUS is plugged in (not helpful), but then
    resets to 1 when VBUS is unplugged.  Any attempt to clear the bit on battery power fails, it is forced to 1.

    */

    // high is 3.0V, low is 2.8V
    if( high ){

        bq25895_v_set_reg_bits( BQ25895_REG_CHARGE_VOLTS, BQ25895_BIT_BATLOWV );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_CHARGE_VOLTS, BQ25895_BIT_BATLOWV );
    }
}

void bq25895_v_enable_ship_mode( bool delay ){

    // enable delay
    if( delay ){

        bq25895_v_set_reg_bits( BQ25895_REG_SHIP_MODE, BQ25895_BIT_BATFET_DLY );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_SHIP_MODE, BQ25895_BIT_BATFET_DLY );
    }

    // enable BATFET shutoff
    bq25895_v_set_reg_bits( BQ25895_REG_SHIP_MODE, BQ25895_BIT_BATFET_DIS );
}

void bq25895_v_leave_ship_mode( void ){

    // disable BATFET shutoff
    bq25895_v_clr_reg_bits( BQ25895_REG_SHIP_MODE, BQ25895_BIT_BATFET_DIS );
}

void bq25895_v_set_system_reset( bool enable ){

    if( enable ){

        bq25895_v_set_reg_bits( BQ25895_REG_SHIP_MODE, BQ25895_BIT_BATFET_RST_EN );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_SHIP_MODE, BQ25895_BIT_BATFET_RST_EN );
    }
}

// voltage is in mV!
void bq25895_v_set_boost_voltage( uint16_t volts ){

    if( volts > BQ25895_MAX_BOOST_VOLTAGE ){

        volts = BQ25895_MAX_BOOST_VOLTAGE;
    }
    else if( volts < BQ25895_MIN_BOOST_VOLTAGE ){

        volts = BQ25895_MIN_BOOST_VOLTAGE;
    }

    uint8_t data = ( ( volts - 4550 ) * 15 ) / 960;
    data <<= BQ25895_SHIFT_BOOST_VOLTS;

    data &= BQ25895_MASK_BOOST_VOLTS;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_BOOST_VOLTS, BQ25895_MASK_BOOST_VOLTS );

    bq25895_v_set_reg_bits( BQ25895_REG_BOOST_VOLTS, data );
}

uint8_t bq25895_u8_get_vbus_status( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_VBUS_STATUS ) & BQ25895_MASK_VBUS_STATUS;
    data >>= BQ25895_SHIFT_VBUS_STATUS;

    return data;
}

bool bq25895_b_get_vbus_good( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_VBUS_GOOD );

    return ( data & BQ25895_BIT_VBUS_GOOD ) != 0;
}

uint8_t bq25895_u8_get_charge_status( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_CHARGE_STATUS ) & BQ25895_MASK_CHARGE_STATUS;
    data >>= BQ25895_SHIFT_CHARGE_STATUS;

    return data;
}

bool bq25895_b_is_charging( void ){

    uint8_t status = bq25895_u8_get_charge_status();

    if( ( status == BQ25895_CHARGE_STATUS_NOT_CHARGING ) ||
        ( status == BQ25895_CHARGE_STATUS_CHARGE_DONE ) ){

        return FALSE;
    }

    // if the batt controller still thinks it is charging, but it is down to 0 current,
    // we can just call it done.  sometimes it never quite gets to full.
    if( ( batt_volts > ( batt_u16_get_charge_voltage() - 100 ) ) &&
        ( batt_charge_current == 0 ) ){

        return FALSE;
    }

    return TRUE;
}

bool bq25895_b_power_good( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_POWER_GOOD ) & BQ25895_BIT_POWER_GOOD;

    return data != 0;
}

uint8_t bq25895_u8_get_faults( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_FAULT );

    return data;
}

uint16_t bq25895_u16_get_batt_voltage( void ){

    return batt_volts;
}

static uint16_t _bq25895_u16_get_batt_voltage( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_BATT_VOLTAGE ) & BQ25895_MASK_BATT_VOLTAGE;

    // check if battery is not connected (the min voltage is way below min. safe on a Li-ion,
    // so we assume there is no battery in this case).
    if( data == 0 ){

        // return 0, since that's effectively the battery voltage with no battery.
        return 0;
    }

    uint16_t mv = ( ( (uint32_t)data * ( 4840 - 2304 ) ) / 127 ) + 2304;

    return mv;
}

uint16_t bq25895_u16_get_vbus_voltage( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_VBUS_VOLTAGE ) & BQ25895_MASK_VBUS_VOLTAGE;

    // check if 0, if so, VBUS is most likely not connected, so we'll return a 0
    if( data == 0 ){

        return 0;
    }

    uint16_t mv = ( ( (uint32_t)data * ( 15300 - 2600 ) ) / 127 ) + 2600;

    return mv;
}

uint16_t bq25895_u16_get_sys_voltage( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_SYS_VOLTAGE ) & BQ25895_MASK_SYS_VOLTAGE;

    uint16_t mv = ( ( (uint32_t)data * ( 4848 - 2304 ) ) / 127 ) + 2304;

    return mv;
}

uint16_t bq25895_u16_get_charge_current( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_CHARGE_CURRENT ) & BQ25895_MASK_CHARGE_CURRENT;

    uint16_t mv = ( ( (uint32_t)data * 6350 ) / 127 );

    return mv;
}

static const int8_t temp_table[128] = {
    -18 , // 127
    -16 ,
    -15 ,
    -13 ,
    -11 ,
    -10 ,
    -9  ,
    -7  ,
    -6  ,
    -5  ,
    -4  ,
    -3  ,
    -2  ,
    -1  ,
    0   ,
    1   ,
    2   ,
    3   ,
    4   ,
    5   ,
    6   ,
    7   ,
    8   ,
    9   ,
    10  ,
    10  ,
    11  ,
    12  ,
    13  ,
    13  ,
    14  ,
    15  ,
    16  ,
    16  ,
    17  ,
    18  ,
    19  ,
    19  ,
    20  ,
    21  ,
    22  ,
    23  ,
    23  ,
    24  ,
    24  ,
    25  ,
    25  ,
    26  ,
    27  ,
    27  ,
    28  ,
    29  ,
    29  ,
    30  ,
    31  ,
    31  ,
    32  ,
    32  ,
    33  ,
    34  ,
    34  ,
    35  ,
    36  ,
    36  ,
    37  ,
    38  ,
    38  ,
    39  ,
    39  ,
    40  ,
    41  ,
    41  ,
    42  ,
    43  ,
    43  ,
    44  ,
    45  ,
    45  ,
    46  ,
    46  ,
    47  ,
    48  ,
    49  ,
    49  ,
    50  ,
    51  ,
    51  ,
    52  ,
    53  ,
    53  ,
    54  ,
    54  ,
    55  ,
    56  ,
    57  ,
    57  ,
    58  ,
    59  ,
    59  ,
    60  ,
    61  ,
    62  ,
    62  ,
    63  ,
    64  ,
    65  ,
    66  ,
    66  ,
    67  ,
    68  ,
    69  ,
    70  ,
    70  ,
    71  ,
    72  ,
    73  ,
    74  ,
    75  ,
    76  ,
    77  ,
    78  ,
    79  ,
    80  ,
    81  ,
    82  ,
    83  ,
    84  ,
    85  , // 0
};

int8_t bq25895_i8_calc_temp( uint8_t ratio ){

    if( ratio > 127 ){

        ratio = 127;
    }

    // percent conversion from datasheet.
    // we don't need this, our table includes it.
    // float temp = 0.465 * data;
    // temp += 21.0;

    return temp_table[127 - ratio];
}


// // percent * 10
// // IE, the first value is 41.8%
// static const uint16_t temp_table2[] = {
// 418 , // -20C
// 415 ,
// 411 ,
// 407 ,
// 403 ,
// 399 ,
// 395 ,
// 390 ,
// 386 ,
// 381 ,
// 377 ,
// 372 ,
// 367 ,
// 363 ,
// 358 ,
// 353 ,
// 348 ,
// 343 ,
// 338 ,
// 333 ,
// 328 , // 0C
// 322 ,
// 317 ,
// 312 ,
// 307 ,
// 301 ,
// 296 ,
// 291 ,
// 285 ,
// 280 ,
// 275 ,
// 269 ,
// 264 ,
// 259 ,
// 254 ,
// 248 ,
// 243 ,
// 238 ,
// 233 ,
// 228 ,
// 223 ,
// 218 ,
// 213 ,
// 209 ,
// 204 ,
// 199 , // 25C
// 195 ,
// 190 ,
// 186 ,
// 181 ,
// 177 ,
// 173 ,
// 168 ,
// 164 ,
// 160 ,
// 156 ,
// 153 ,
// 149 ,
// 145 ,
// 141 ,
// 138 ,
// 134 ,
// 131 ,
// 128 ,
// 124 ,
// 121 ,
// 118 ,
// 115 ,
// 112 ,
// 109 ,
// 107 ,
// 104 ,
// 101 ,
// 99  ,
// 96  ,
// 94  ,
// 91  ,
// 89  ,
// 87  ,
// 84  ,
// 82  ,
// 80  ,
// 78  ,
// 76  ,
// 74  ,
// 72  ,
// 70  ,
// 69  ,
// 67  ,
// 65  ,
// 63  ,
// 62  ,
// 60  ,
// 59  ,
// 57  ,
// 56  ,
// 54  ,
// 53  ,
// 52  ,
// 50  ,
// 49  ,
// 48  ,
// 47  ,
// 46  ,
// 45  ,
// 43  , // 85C
// };

// // percent * 10, using table 2
// int8_t bq25895_i8_calc_temp2( uint16_t percent ){

//     for( uint8_t i = 0; i < cnt_of_array(temp_table2) - 1; i++ ){

//         if( ( percent <= temp_table2[i] ) && ( percent >= temp_table2[i + 1] ) ){

//             return (int16_t)i - 20;
//         }
//     }

//     return -20;
// }

int8_t bq25895_i8_get_therm( void ){

    uint8_t data = read_cached_reg( BQ25895_REG_THERM );

    return bq25895_i8_calc_temp( data );
}

int8_t bq25895_i8_get_temp( void ){

    return batt_temp;
}

// int8_t bq25895_i8_get_case_temp( void ){

//     #ifdef ESP32
//     return case_temp;
//     #else
//     return 0;
//     #endif
// }

// int8_t bq25895_i8_get_ambient_temp( void ){

//     #ifdef ESP32
//     return ambient_temp;
//     #else
//     return 0;
//     #endif
// }

uint16_t bq25895_u16_read_vbus( void ){

    return vbus_volts;
}

void bq25895_v_set_watchdog( uint8_t setting ){

    setting <<= BQ25895_SHIFT_WATCHDOG;
    setting &= BQ25895_MASK_WATCHDOG;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_WATCHDOG, BQ25895_MASK_WATCHDOG );

    bq25895_v_set_reg_bits( BQ25895_REG_WATCHDOG, setting );
}

void bq25895_v_kick_watchdog( void ){

    bq25895_v_set_reg_bits( BQ25895_REG_WATCHDOG_RST, BQ25895_BIT_WATCHDOG_RST );
}

void bq25895_v_set_charger( bool enable ){

    if( enable ){

        bq25895_v_set_reg_bits( BQ25895_REG_CHARGE_EN, BQ25895_BIT_CHARGE_EN );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_CHARGE_EN, BQ25895_BIT_CHARGE_EN );
    }
}

void bq25895_v_set_charge_timer( uint8_t setting ){

    setting <<= BQ25895_SHIFT_CHARGE_TIMER;
    setting &= BQ25895_MASK_CHARGE_TIMER;

    // clear bits
    bq25895_v_clr_reg_bits( BQ25895_REG_CHARGE_TIMER, BQ25895_MASK_CHARGE_TIMER );

    bq25895_v_set_reg_bits( BQ25895_REG_CHARGE_TIMER, setting );
}

uint8_t bq25895_u8_get_device_id( void ){

    return bq25895_u8_read_reg( BQ25895_REG_DEV_ID ) & BQ25895_MASK_DEV_ID;
}

void bq25895_v_print_regs( void ){
    
    uint8_t data;

    log_v_debug_P( PSTR("BQ25895:") );
    
    data = bq25895_u8_read_reg( 0x00 );
    bool hiz            = ( data & BQ25895_BIT_ENABLE_HIZ ) != 0;
    bool en_ilim        = ( data & BQ25895_BIT_ENABLE_ILIM_PIN ) != 0;
    uint8_t iinlim      = data & BQ25895_MASK_INPUT_CURRENT_LIM;
    log_v_debug_P( PSTR("0x00 = 0x%02x | HIZ: %d EN_ILIM: %d IINLIM: %d"), data, hiz, en_ilim, iinlim );

    data = bq25895_u8_read_reg( 0x01 );
    log_v_debug_P( PSTR("0x01 = 0x%02x"), data );    

    data = bq25895_u8_read_reg( 0x02 );
    bool conv_start     = ( data & BQ25895_BIT_ADC_CONV_START ) != 0;
    bool conv_rate      = ( data & BQ25895_BIT_ADC_CONV_RATE ) != 0;
    bool boost_freq     = ( data & BQ25895_BIT_BOOST_FREQ ) != 0;
    bool ico_en         = ( data & BQ25895_BIT_ICO_EN ) != 0;
    bool hvdcp_en       = ( data & BQ25895_BIT_HVDCP_EN ) != 0;
    bool maxc_en        = ( data & BQ25895_BIT_MAXC_EN ) != 0;
    bool force_dpdm     = ( data & BQ25895_BIT_FORCE_DPDM ) != 0;
    bool auto_dpdm_en   = ( data & BQ25895_BIT_AUTO_DPDM ) != 0;
    log_v_debug_P( PSTR("0x02 = 0x%02x | CONV_START: %d CONV_RATE: %d BOOST_FREQ: %d ICO_EN: %d HVDCP_EN: %d MAXC_EN: %d FORCE_DPDM: %d AUTO_DPDM_EN: %d"), 
        data,
        conv_start,
        conv_rate,
        boost_freq,
        ico_en,
        hvdcp_en,
        maxc_en,
        force_dpdm,
        auto_dpdm_en );

    data = bq25895_u8_read_reg( 0x03 );
    bool otg_config     = ( data & BQ25895_BIT_BOOST_EN ) != 0;
    bool chg_config     = ( data & BQ25895_BIT_CHARGE_EN ) != 0;
    uint8_t sys_min     = ( data & BQ25895_MASK_MINSYS ) >> BQ25895_SHIFT_MINSYS;
    log_v_debug_P( PSTR("0x03 = 0x%02x | OTG_CONFIG: %d CHG_CONFIG: %d SYS_MIN: %d"), data, otg_config, chg_config, sys_min );

    data = bq25895_u8_read_reg( 0x04 );
    uint8_t ichg        = data & BQ25895_MASK_FAST_CHARGE;
    log_v_debug_P( PSTR("0x04 = 0x%02x | ICHG: %d"), data, ichg );

    data = bq25895_u8_read_reg( 0x05 );
    uint8_t iprechg     = ( data & BQ25895_MASK_PRE_CHARGE ) >> BQ25895_SHIFT_PRE_CHARGE;
    uint8_t iterm       = ( data & BQ25895_MASK_TERM );
    log_v_debug_P( PSTR("0x05 = 0x%02x | IPRECHG: %d ITERM: %d"), data, iprechg, iterm );

    data = bq25895_u8_read_reg( 0x06 );
    uint8_t vreg        = ( data & BQ25895_MASK_CHARGE_VOLTS ) >> BQ25895_SHIFT_CHARGE_VOLTS;
    bool batlowv        = ( data & BQ25895_BIT_BATLOWV ) != 0;
    bool vrechg         = ( data & BQ25895_BIT_VRECHG ) != 0;
    log_v_debug_P( PSTR("0x06 = 0x%02x | VREG: %d BATLOWV: %d VRECHG: %d"), data, vreg, batlowv, vrechg );

    data = bq25895_u8_read_reg( 0x07 );
    log_v_debug_P( PSTR("0x07 = 0x%02x"), data );    

    data = bq25895_u8_read_reg( 0x08 );
    log_v_debug_P( PSTR("0x08 = 0x%02x"), data );    

    data = bq25895_u8_read_reg( 0x09 );
    uint8_t batfet_dis  = ( data & BQ25895_BIT_BATFET_DIS ) != 0;
    uint8_t batfet_dly  = ( data & BQ25895_BIT_BATFET_DLY ) != 0;
    uint8_t batfet_rst_en  = ( data & BQ25895_BIT_BATFET_RST_EN ) != 0;
    log_v_debug_P( PSTR("0x09 = 0x%02x | BATFET_DIS: %d BATFET_DLY: %d BATFET_RST_EN: %d"), data, batfet_dis, batfet_dly, batfet_rst_en );

    data = bq25895_u8_read_reg( 0x0A );
    uint8_t boostv       = ( data & BQ25895_MASK_BOOST_VOLTS ) >> BQ25895_SHIFT_BOOST_VOLTS;
    log_v_debug_P( PSTR("0x0A = 0x%02x | BOOSTV: %d"), data, boostv );

    data = bq25895_u8_read_reg( 0x0B );
    uint8_t vbus_stat   = ( data & BQ25895_MASK_VBUS_STATUS ) >> BQ25895_SHIFT_VBUS_STATUS;
    uint8_t charge_stat = ( data & BQ25895_MASK_CHARGE_STATUS ) >> BQ25895_SHIFT_CHARGE_STATUS;
    bool pg_stat        = ( data & BQ25895_BIT_POWER_GOOD ) != 0;
    bool sdp_stat       = ( data & BQ25895_BIT_SDP_STAT ) != 0;
    bool vsys_stat      = ( data & BQ25895_BIT_VSYS_STAT ) != 0;
    log_v_debug_P( PSTR("0x0B = 0x%02x | VBUS_STAT: %d CHARGE_STAT: %d PG_STAT: %d SDP_STAT: %d VSYS_STAT: %d"), data, vbus_stat, charge_stat, pg_stat, sdp_stat, vsys_stat );    

    data = bq25895_u8_read_reg( 0x0C );
    bool watchdog_fault = ( data & BQ25895_BIT_WATCHDOG_FAULT ) != 0;
    bool boost_fault    = ( data & BQ25895_BIT_BOOST_FAULT ) != 0;
    uint8_t chrg_fault  = ( data & BQ25895_MASK_CHRG_FAULT ) >> BQ25895_SHIFT_CHRG_FAULT;
    bool bat_fault      = ( data & BQ25895_BIT_BAT_FAULT ) != 0;
    uint8_t ntc_fault   = ( data & BQ25895_MASK_NTC_FAULT ) >> BQ25895_SHIFT_NTC_FAULT;
    log_v_debug_P( PSTR("0x0C = 0x%02x | WATCHDOG_FAULT: %d BOOST_FAULT: %d CHRG_FAULT: %d BAT_FAULT: %d NTC_FAULT: %d"), data, watchdog_fault, boost_fault, chrg_fault, bat_fault, ntc_fault );

    data = bq25895_u8_read_reg( 0x0D );
    bool force_vindpm   = ( data & BQ25895_BIT_FORCE_VINDPM ) != 0;
    uint8_t vindpm_reg  = ( data & BQ25895_MASK_VINDPM );
    log_v_debug_P( PSTR("0x0D = 0x%02x | FORCE_VINDPM: %d VINDPM: %d"), data, force_vindpm, vindpm_reg );

    data = bq25895_u8_read_reg( 0x0E );
    bool therm_stat    = ( data & BQ25895_BIT_THERM_STAT ) != 0;
    uint8_t batv       = ( data & BQ25895_MASK_BATT_VOLTAGE );
    log_v_debug_P( PSTR("0x0E = 0x%02x | THERM_STAT: %d BATV: %d"), data, therm_stat, batv );

    data = bq25895_u8_read_reg( 0x0F );
    uint8_t sysv       = ( data & BQ25895_MASK_SYS_VOLTAGE );
    log_v_debug_P( PSTR("0x0F = 0x%02x | SYSV: %d"), data, sysv );

    data = bq25895_u8_read_reg( 0x10 );
    uint8_t tspct      = data;
    log_v_debug_P( PSTR("0x10 = 0x%02x | TSPCT: %d"), data, tspct );

    data = bq25895_u8_read_reg( 0x11 );
    bool vbus_gd       = ( data & BQ25895_BIT_VBUS_GOOD ) != 0;
    uint8_t vbusv      = ( data & BQ25895_MASK_VBUS_VOLTAGE );
    log_v_debug_P( PSTR("0x11 = 0x%02x | VBUS_GD: %d VBUSV: %d"), data, vbus_gd, vbusv );    

    data = bq25895_u8_read_reg( 0x12 );
    uint8_t ichgr      = data;
    log_v_debug_P( PSTR("0x12 = 0x%02x | ICHGR: %d"), data, ichgr );

    data = bq25895_u8_read_reg( 0x13 );
    bool vdpm_stat     = ( data & BQ25895_BIT_VINDPM ) != 0;
    bool idpm_stat     = ( data & BQ25895_BIT_IINDPM ) != 0;
    uint8_t idpm_lim   = ( data & BQ25895_MASK_IINDPM );
    log_v_debug_P( PSTR("0x13 = 0x%02x | VDPM_STAT: %d IDPM_STAT: %d IDPM_LIM: %d"), data, vdpm_stat, idpm_stat, idpm_lim );    

    data = bq25895_u8_read_reg( 0x14 );
    log_v_debug_P( PSTR("0x14 = 0x%02x"), data );    




    // for( uint8_t addr = 0; addr < 0x15; addr++ ){

    //     uint8_t data = bq25895_u8_read_reg( addr );

    //     log_v_debug_P( PSTR("0x%02x = 0x%02x"), addr, data );
    // }
}

void bq25895_v_set_vindpm( int16_t mv ){

    uint16_t original_mv = mv;

    if( mv < BQ25895_MIN_VINDPM ){

        mv = BQ25895_MIN_VINDPM;
    }
    else if( mv > BQ25895_MAX_VINDPM ){

        mv = BQ25895_MAX_VINDPM;
    }

    mv -= 2600;
    mv /= 100;

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_VINDPM );

    reg &= ~BQ25895_MASK_VINDPM;

    reg |= mv;

    // if setting to 0, use relative mode
    if( original_mv == 0 ){

        // clear the force bit
        reg &= ~BQ25895_BIT_FORCE_VINDPM;            
    }
    else{

        // set the force bit
        reg |= BQ25895_BIT_FORCE_VINDPM;
    }

    bq25895_v_write_reg( BQ25895_REG_VINDPM, reg );
}

uint16_t bq25895_u16_get_iindpm( void ){

    uint16_t data = read_cached_reg( BQ25895_REG_IINDPM ) & BQ25895_MASK_IINDPM;

    return 50 * data + 100;
}

bool bq25895_b_get_vindpm( void ){
    
    uint8_t reg = read_cached_reg( BQ25895_REG_IINDPM );

    return ( reg & BQ25895_BIT_VINDPM ) != 0;
}

bool bq25895_b_get_iindpm( void ){
    
    uint8_t reg = read_cached_reg( BQ25895_REG_IINDPM );

    return ( reg & BQ25895_BIT_IINDPM ) != 0;
}

static void init_boost_converter( void ){

    log_v_debug_P( PSTR("Init boost converter") );

    // boost frequency can only be changed when OTG boost is turned off.
    bq25895_v_set_boost_mode( FALSE );
    bq25895_v_set_boost_1500khz();
    bq25895_v_set_boost_mode( TRUE );

    // set boost voltage
    if( boost_voltage == 0 ){

        boost_voltage = 5100;
    }
    else if( boost_voltage > BQ25895_MAX_BOOST_VOLTAGE ){

        boost_voltage = BQ25895_MAX_BOOST_VOLTAGE;
    }
    else if( boost_voltage < BQ25895_MIN_BOOST_VOLTAGE ){

        boost_voltage = BQ25895_MIN_BOOST_VOLTAGE;
    }

    bq25895_v_set_boost_voltage( boost_voltage );
}

static void init_charger( void ){

    log_v_debug_P( PSTR("Init charger") );

    // enable charger and make sure HIZ is disabled
    bq25895_v_set_hiz( FALSE );
    bq25895_v_set_charger( TRUE );

    bq25895_v_set_minsys( BQ25895_SYSMIN_3_0V );
    bq25895_v_set_watchdog( BQ25895_WATCHDOG_OFF );

    // charge config for NCR18650B

    // bq25895_v_set_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );

    // turn off ICO
    bq25895_v_clr_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );   

    bq25895_v_set_inlim( 3250 );
    bq25895_v_set_pre_charge_current( 160 );
    
    // default to 0.5C rate:
    uint32_t fast_charge_current = batt_u16_get_nameplate_capacity() / 2;

    if( fast_charge_current > BQ25895_MAX_FAST_CHARGE_CURRENT ){

        fast_charge_current = BQ25895_MAX_FAST_CHARGE_CURRENT;
    }

    // set default max current to the cell count setting
    if( batt_max_charge_current == 0 ){

        batt_max_charge_current = fast_charge_current;
    }
    // apply maximum charge current, if the allowable cell current would exceed it
    else if( fast_charge_current > batt_max_charge_current ){

        fast_charge_current = batt_max_charge_current;
    }

    bq25895_v_set_fast_charge_current( fast_charge_current );

    bq25895_v_set_termination_current( BQ25895_TERM_CURRENT );

    bq25895_v_set_charge_voltage( batt_u16_get_charge_voltage() );

    // bq25895_v_set_batlowv( FALSE ); // set BATLOWV to 2.8V

    // disable ILIM pin
    bq25895_v_set_inlim_pin( FALSE );

    // bq25895_v_set_iindpm( 3000 );

    // turn off high voltage DCP, and maxcharge
    bq25895_v_clr_reg_bits( BQ25895_REG_MAXC_EN,   BQ25895_BIT_MAXC_EN );
    bq25895_v_clr_reg_bits( BQ25895_REG_HVDCP_EN,  BQ25895_BIT_HVDCP_EN );

    // turn on auto dpdm
    // bq25895_v_set_reg_bits( BQ25895_REG_AUTO_DPDM, BQ25895_BIT_AUTO_DPDM );

    // turn OFF auto dpdm
    bq25895_v_clr_reg_bits( BQ25895_REG_AUTO_DPDM, BQ25895_BIT_AUTO_DPDM );

    // turn off ICO
    // bq25895_v_clr_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );   

    bq25895_v_set_vindpm( 0 );
}

// top level API to enable the charger
void bq25895_v_enable_charger( void ){

    init_charger();

    bq25895_v_set_charger( TRUE );

    if( mcu_source_pmid ){
            
        // re-init boost
        init_boost_converter();
    }

}

// top level API to disable the charger
void bq25895_v_disable_charger( void ){

    bq25895_v_set_charger( FALSE );

}


static bool is_charging( void ){

    return ( charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE ) ||
           ( charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE );

}


static bool read_adc( void ){

    batt_fault = bq25895_u8_get_faults();
    vbus_status = bq25895_u8_get_vbus_status();
    vbus_volts = bq25895_u16_get_vbus_voltage();

    uint16_t temp_batt_volts = _bq25895_u16_get_batt_voltage();

    if( temp_batt_volts == 0 ){

        return FALSE;
    }

    if( batt_volts == 0 ){

        batt_volts = temp_batt_volts;
    }

    batt_charge_current = bq25895_u16_get_charge_current();
    charge_status = bq25895_u8_get_charge_status();
    batt_charging = is_charging();

    if( batt_volts != 0 ){

        batt_volts = util_u16_ewma( temp_batt_volts, batt_volts, VOLTS_FILTER );
    }
    

    sys_volts = bq25895_u16_get_sys_voltage();
    iindpm = bq25895_u16_get_iindpm();

    int8_t temp = bq25895_i8_get_therm();

    batt_temp_raw = temp;

    if( batt_temp != -127 ){

        batt_temp_state = util_i16_ewma( temp * 256, batt_temp_state, BQ25895_THERM_FILTER );
        batt_temp = batt_temp_state / 256;
    }
    else{

        batt_temp_state = temp * 256;
        batt_temp = temp;
    }
    

    batt_charge_power = ( (uint32_t)batt_charge_current * (uint32_t)batt_volts ) / 1000;

    return TRUE;
}


// static bool is_recharge_threshold( void ){

//     return batt_volts <= ( batt_u16_get_charge_voltage() - 50 );
// }

// static bool is_vbus_volts_ok( void ){

//     if( vbus_volts < 4200 ){

//         return FALSE;
//     }

//     return TRUE;
// }

// static bool is_vbus_good( void ){

//     if( !bq25895_b_get_vbus_good() ){

//         return FALSE;
//     }

//     return TRUE;
// }

#if 0
PT_THREAD( bat_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    init_charger();

    vbus_connected = FALSE;
    vindpm = VINDPM_WALL; // wall power

    // wait until we see the battery
    THREAD_WAIT_WHILE( pt, adc_good == 0 );

    while(1){

        vbus_connected = FALSE;

        // init charger
        init_charger();

        TMR_WAIT( pt, 20 );

      /*

        Charger notes:

        If not using USB, need to short USB data lines to enable DCP mode (1.5A and beyond).
        Otherwise may get limited to 500 mA by DPDM.

        OR turn AUTO_DPDM off.  Remember force DPDM will run even if auto is off, so bypass
        that as well if you don't want DPDM at all.


        MORE NOTES!!!!!!!!!!!!!!!!!!!

        REGN is turned off in HIZ, if there is no input on VBUS, and boost mode is disabled.
        This means the batt temp sensor will read incorrectly in those conditions!
        Also note there is a 200ms power on delay for REGN.

        */


        // set MINSYS to 3.0V for ADC accuracy.  VBAT must be greater than MINSYS.
        // NOTE set MINSYS to 3.0V for ADC accuracy when on battery power.
        // the chip won't actually regulate UP to MINSYS anyway so on battery it doesn't
        // matter, we will basically just get whatever the battery is on SYS.
        // when charging, we can set it to 3.5V (or 3.7V) so we are above 3.3V SYS.

        // NOTE below 10 C (check thermistor!), reduce charging to 0.25C.
        // don't forget max otherwise is 0.5C.  Need a config option for this.

        // NOTE reduce battery charge.  Charge to 4.0 or 4.1V and discharge to 
        // 3.0 to 3.2V to increase cycle life.        


        // NOTE:
        // VINDPM needs to be set to 0 for wall power!

        // NOTE:
        // if the charger is disabled and there is a source on VBUS, the Q4 BATFET will
        // be *disabled*.  If VBUS cannot support the load on SYS, the system will brownout.
        // if the charger is enabled, Q4 is still turned on, so the battery can supplement SYS
        // even if VBUS is weak.

        // NOTE:
        // HIZ will be reset to disabled each time VBUS comes up.
        
        
        THREAD_WAIT_WHILE( pt, !is_vbus_volts_ok() );

        if( !is_vbus_volts_ok() ){

            continue;
        }

        // check VBUS good

        TMR_WAIT( pt, 100 );

        if( !is_vbus_good() ){

            continue;
        }
        

        log_v_debug_P( PSTR("VBUS OK") );

        vbus_connected = TRUE;

        if( mcu_source_pmid ){
            
            // re-init boost
            init_boost_converter();
        }

        // check if we need to charge
        THREAD_WAIT_WHILE( pt, !is_recharge_threshold() &&
                                is_vbus_good() );

        if( !is_vbus_good() ){

            continue;
        }
        

        // // check vbus
        if( vbus_volts < 5500 ){

            // set for wall power:
            vindpm = VINDPM_WALL;
            bq25895_v_set_vindpm( vindpm );

            TMR_WAIT( pt, 1000 );

            // check faults
            if( batt_fault != 0 ){

                log_v_debug_P( PSTR("faults detected") );

                continue;
            }

            // are we charging at least a bit?
            if( batt_charge_current >= 100 ){

                log_v_debug_P( PSTR("Charging on wall power") );

                THREAD_WAIT_WHILE( pt, is_vbus_good() );

                // unplugged, reset loop
                log_v_debug_P( PSTR("VBUS disconnected") );

                continue;
            }
        }

        // not enough current, or VBUS is too high to be a wall charger.  try solar mode.

        vindpm = VINDPM_SOLAR;
        bq25895_v_set_vindpm( vindpm );

        TMR_WAIT( pt, 1000 );

        // check faults
        if( batt_fault != 0 ){

            log_v_debug_P( PSTR("faults detected: 0x%2x"), batt_fault );

            continue;
        }

        // check if charging
        // we don't check for current, it may be very low with solar
        if( is_charging() ){

            log_v_debug_P( PSTR("Charging on solar power") );

            THREAD_WAIT_WHILE( pt, is_vbus_good() );

            log_v_debug_P( PSTR("VBUS disconnected") );

            continue;
        }


        // charger isn't working?

        log_v_debug_P( PSTR("Not charging - reset control loop") );

        TMR_WAIT( pt, 10000 );
    }

PT_END( pt );
}
#endif


// #if defined(ESP32)

// PT_THREAD( bat_aux_temp_thread( pt_t *pt, void *state ) )
// {
// PT_BEGIN( pt );

//     if( ffs_u8_read_board_type() != BOARD_TYPE_ELITE ){

//         THREAD_EXIT( pt );
//     }

//     io_v_set_mode( ELITE_CASE_ADC_IO, IO_MODE_INPUT );      
//     io_v_set_mode( ELITE_AMBIENT_ADC_IO, IO_MODE_INPUT );      

//     while(1){

//         THREAD_WAIT_WHILE( pt, sys_volts == 0 ); // avoid divide by zero error

//         uint32_t case_adc = adc_u16_read_mv( ELITE_CASE_ADC_IO );
//         uint32_t ambient_adc = adc_u16_read_mv( ELITE_AMBIENT_ADC_IO );

//         int8_t temp = bq25895_i8_calc_temp2( ( case_adc * 1000 ) / sys_volts );

//         if( case_temp != -127 ){

//             case_temp_state = util_i16_ewma( temp * 256, case_temp_state, BQ25895_THERM_FILTER );    
//             case_temp = case_temp_state / 256;
//         }
//         else{

//             case_temp_state = temp * 256;
//             case_temp = temp;
//         }
        
//         temp = bq25895_i8_calc_temp2( ( ambient_adc * 1000 ) / sys_volts );

//         if( ambient_temp != -127 ){

//             ambient_temp_state = util_i16_ewma( temp * 256, ambient_temp_state, BQ25895_THERM_FILTER );    
//             ambient_temp = ambient_temp_state / 256;
//         }
//         else{

//             ambient_temp_state = temp * 256;
//             ambient_temp = temp;
//         }

//         TMR_WAIT( pt, 1000 );
//     }

// PT_END( pt );
// }

// #endif



PT_THREAD( bat_mon_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    init_charger();
    batt_v_disable_charge();

    if( ( ffs_u8_read_board_type() == BOARD_TYPE_UNKNOWN ) ||
        ( ffs_u8_read_board_type() == BOARD_TYPE_UNSET ) ){

        log_v_debug_P( PSTR("MCU power source is PMID BOOST") );

        mcu_source_pmid = TRUE;

        // we will not init the boost converter in this instance, because that will cut MCU power.
        // we have to wait until we have VBUS available.
    }
    else{

        init_boost_converter();
    }

    // thread_t_create( bat_control_thread,
    //                  PSTR("bat_control"),
    //                  0,
    //                  0 );

    // #if defined(ESP32)

    // thread_t_create( bat_aux_temp_thread,
    //                  PSTR("bat_aux_temp"),
    //                  0,
    //                  0 );

    // #endif

    mppt_v_reset();

    TMR_WAIT( pt, 50 );

    while(1){

        static uint32_t start_time;

        bq25895_v_start_adc_oneshot();
        start_time = tmr_u32_get_system_time_ms();

        thread_v_set_alarm( tmr_u32_get_system_time_ms() + 2000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !bq25895_b_adc_ready() );

        // read all registers
        bq25895_v_read_all();


        if( bq25895_b_adc_ready_cached() && read_adc() ){

            // ADC success

            uint16_t elapsed = tmr_u32_elapsed_time_ms( start_time );

            if( elapsed < adc_time_min ){

                adc_time_min = elapsed;
            }
            
            if( elapsed > adc_time_max ){

                adc_time_max = elapsed;
            }

            adc_good++;


            // run MPPT
            mppt_v_run( batt_charge_current );
        }
        else{

            adc_fail++;

            TMR_WAIT( pt, 200 );

            continue;
        }

        if( dump_regs ){

            dump_regs = FALSE;

            bq25895_v_print_regs();
        }

        TMR_WAIT( pt, 500 );
    }

PT_END( pt );
}

#endif
