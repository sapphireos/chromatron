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

#include "sapphire.h"

#include "i2c.h"

#include "bq25895.h"
#include "util.h"
#include "energy.h"

static uint8_t batt_soc; // state of charge in percent
static uint8_t batt_soc_startup; // state of charge at power on
static uint16_t batt_volts;
static uint16_t vbus_volts;
static uint16_t sys_volts;
static uint16_t batt_charge_current;
static uint16_t batt_max_charge_current;
static bool batt_charging;
static bool vbus_connected;
static uint8_t batt_fault;
static uint8_t vbus_status;
static uint8_t charge_status;
static bool dump_regs;
static uint32_t capacity;
static int32_t remaining;
static int8_t therm;
static uint8_t batt_cells; // number of cells in system
static uint16_t boost_voltage;
static uint16_t vindpm;

KV_SECTION_META kv_meta_t bat_info_kv[] = {
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &batt_soc,                 0,  "batt_soc" },
    { SAPPHIRE_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &therm,                    0,  "batt_temp" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &batt_charging,            0,  "batt_charging" },
    { SAPPHIRE_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &vbus_connected,           0,  "batt_external_power" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_volts,               0,  "batt_volts" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &vbus_volts,               0,  "batt_vbus_volts" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &sys_volts,                0,  "batt_sys_volts" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &charge_status,            0,  "batt_charge_status" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &batt_charge_current,      0,  "batt_charge_current" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &batt_fault,               0,  "batt_fault" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &vbus_status,              0,  "batt_vbus_status" },
    { SAPPHIRE_TYPE_UINT32,  0, KV_FLAGS_PERSIST,    &capacity,                 0,  "batt_capacity" },
    { SAPPHIRE_TYPE_INT32,   0, KV_FLAGS_READ_ONLY,  &remaining,                0,  "batt_remaining" },
    { SAPPHIRE_TYPE_UINT8,   0, KV_FLAGS_PERSIST,    &batt_cells,               0,  "batt_cells" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &batt_max_charge_current,  0,  "batt_max_charge_current" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &boost_voltage,            0,  "batt_boost_voltage" },
    { SAPPHIRE_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &vindpm,                   0,  "batt_vindpm" },


    { SAPPHIRE_TYPE_BOOL,    0, 0,                   &dump_regs,            0,  "batt_dump_regs" },
};

static uint16_t soc_state;

#define SOC_MAX_VOLTS   ( BQ25895_FLOAT_VOLTAGE - 200 )
#define SOC_MIN_VOLTS   BQ25895_CUTOFF_VOLTAGE
#define SOC_FILTER      16

PT_THREAD( bat_control_thread( pt_t *pt, void *state ) );
PT_THREAD( bat_mon_thread( pt_t *pt, void *state ) );


static uint16_t _calc_batt_soc( uint16_t volts ){

    if( volts < SOC_MIN_VOLTS ){

        return 0;
    }

    if( volts > SOC_MAX_VOLTS ){

        return 10000;
    }

    return util_u16_linear_interp( volts, SOC_MIN_VOLTS, 0, SOC_MAX_VOLTS, 10000 );
}

static uint8_t calc_batt_soc( uint16_t volts ){

    uint16_t temp_soc = _calc_batt_soc( volts );

    soc_state = util_u16_ewma( temp_soc, soc_state, SOC_FILTER );

    return soc_state / 100;
}

int8_t bq25895_i8_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    // probe for battery charger
    if( bq25895_u8_get_device_id() != BQ25895_DEVICE_ID ){

        log_v_debug_P( PSTR("batt controller not found") );

        return -1;
    }

    thread_t_create( bat_mon_thread,
                     PSTR("bat_mon"),
                     0,
                     0 );

    thread_t_create( bat_control_thread,
                     PSTR("bat_control"),
                     0,
                     0 );

    return 0;
}

uint8_t bq25895_u8_read_reg( uint8_t addr ){

    i2c_v_write( BQ25895_I2C_ADDR, &addr, sizeof(addr) );

    uint8_t data = 0;
    
    i2c_v_read( BQ25895_I2C_ADDR, &data, sizeof(data) );

    return data;
}

void bq25895_v_write_reg( uint8_t addr, uint8_t data ){

    uint8_t cmd[2];
    cmd[0] = addr;
    cmd[1] = data;

    i2c_v_write( BQ25895_I2C_ADDR, cmd, sizeof(cmd) );
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

    uint32_t data = bq25895_u8_read_reg( BQ25895_REG_INPUT_CURRENT ) & BQ25895_MASK_INPUT_CURRENT_LIM;

    return ( ( data * 3150 ) / 63 ) + 100;
}

void bq25895_v_enable_adc_continuous( void ){

    bq25895_v_set_reg_bits( BQ25895_REG_ADC, BQ25895_BIT_ADC_CONV_RATE );
}

void bq25895_v_start_adc_oneshot( void ){

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_ADC );
    reg &= ~BQ25895_BIT_ADC_CONV_RATE;
    reg |= BQ25895_BIT_ADC_CONV_START;

    bq25895_v_write_reg( BQ25895_REG_ADC, reg );
}

bool bq25895_b_adc_ready( void ){

    return ( bq25895_u8_read_reg( BQ25895_REG_ADC ) & BQ25895_BIT_ADC_CONV_START ) == 0;
}

void bq25895_v_set_boost_1500khz( void ){

    // default is 500k

    bq25895_v_clr_reg_bits( BQ25895_REG_BOOST_FREQ, BQ25895_BIT_BOOST_FREQ );
}

bool bq25895_b_is_boost_1500khz( void ){

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_BOOST_FREQ );

    return ( reg & BQ25895_BIT_BOOST_FREQ ) != 0;
}

void bq25895_v_set_boost_mode( bool enable ){

    if( enable ){

        bq25895_v_set_reg_bits( BQ25895_REG_BOOST_EN, BQ25895_BIT_BOOST_EN );
    }
    else{

        bq25895_v_clr_reg_bits( BQ25895_REG_BOOST_EN, BQ25895_BIT_BOOST_EN );
    }
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

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_VBUS_STATUS ) & BQ25895_MASK_VBUS_STATUS;
    data >>= BQ25895_SHIFT_VBUS_STATUS;

    return data;
}

bool bq25895_b_get_vbus_good( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_VBUS_GOOD );

    return ( data & BQ25895_BIT_VBUS_GOOD ) != 0;
}

uint8_t bq25895_u8_get_charge_status( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_CHARGE_STATUS ) & BQ25895_MASK_CHARGE_STATUS;
    data >>= BQ25895_SHIFT_CHARGE_STATUS;

    return data;
}

bool bq25895_b_power_good( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_POWER_GOOD ) & BQ25895_BIT_POWER_GOOD;

    return data != 0;
}

uint8_t bq25895_u8_get_faults( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_FAULT );

    return data;
}

uint16_t bq25895_u16_get_batt_voltage( void ){

    return batt_volts;
}

static uint16_t _bq25895_u16_get_batt_voltage( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_BATT_VOLTAGE ) & BQ25895_MASK_BATT_VOLTAGE;

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

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_VBUS_VOLTAGE ) & BQ25895_MASK_VBUS_VOLTAGE;

    // check if 0, if so, VBUS is most likely not connected, so we'll return a 0
    if( data == 0 ){

        return 0;
    }

    uint16_t mv = ( ( (uint32_t)data * ( 15300 - 2600 ) ) / 127 ) + 2600;

    return mv;
}

uint16_t bq25895_u16_get_sys_voltage( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_SYS_VOLTAGE ) & BQ25895_MASK_SYS_VOLTAGE;

    uint16_t mv = ( ( (uint32_t)data * ( 4848 - 2304 ) ) / 127 ) + 2304;

    return mv;
}

uint16_t bq25895_u16_get_charge_current( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_CHARGE_CURRENT ) & BQ25895_MASK_CHARGE_CURRENT;

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

int8_t bq25895_i8_get_therm( void ){

    uint8_t data = bq25895_u8_read_reg( BQ25895_REG_THERM );

    // percent conversion from datasheet.
    // we don't need this, our table includes it.
    // float temp = 0.465 * data;
    // temp += 21.0;

    return temp_table[127 - data];
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

uint8_t bq25895_u8_get_soc( void ){

    return batt_soc;
}

uint8_t bq25895_u8_get_device_id( void ){

    return bq25895_u8_read_reg( BQ25895_REG_DEV_ID ) & BQ25895_MASK_DEV_ID;
}

void bq25895_v_print_regs( void ){

    log_v_debug_P( PSTR("BQ25895:") );

    for( uint8_t addr = 0; addr < 0x15; addr++ ){

        uint8_t data = bq25895_u8_read_reg( addr );

        log_v_debug_P( PSTR("0x%02x = 0x%02x"), addr, data );
    }
}

void bq25895_v_set_vindpm( int16_t mv ){

    if( mv < 3900 ){

        mv = 3900;
    }
    else if( mv > 15300 ){

        mv = 15300;
    }

    mv -= 2600;
    mv /= 100;

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_VINDPM );

    // set the force bit
    reg |= BQ25895_BIT_FORCE_VINDPM;

    reg &= ~BQ25895_MASK_VINDPM;

    // make sure force bit is set before we update the setting
    bq25895_v_write_reg( BQ25895_REG_VINDPM, reg );

    reg |= mv;

    bq25895_v_write_reg( BQ25895_REG_VINDPM, reg );
}

void bq25895_v_set_iindpm( int16_t ma ){

    if( ma < 100 ){

        ma = 100;
    }
    else if( ma > 3250 ){

        ma = 3250;
    }

    ma *= 32;

    ma /= ( ( 3250 * 32 ) / 63 ) - 1;

    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_IINDPM );

    reg &= ~BQ25895_MASK_IINDPM;
    // bq25895_v_write_reg( BQ25895_REG_IINDPM, reg );

    reg |= ma;

    bq25895_v_write_reg( BQ25895_REG_IINDPM, reg );
}

bool bq25895_b_get_vindpm( void ){
    
    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_IINDPM );

    return ( reg & BQ25895_BIT_VINDPM ) != 0;
}

bool bq25895_b_get_iindpm( void ){
    
    uint8_t reg = bq25895_u8_read_reg( BQ25895_REG_IINDPM );

    return ( reg & BQ25895_BIT_IINDPM ) != 0;
}


void init_boost_converter( void ){

    // turn off charger
    bq25895_v_set_charger( FALSE );

    // boost frequency can only be changed when OTG boost is turned off.
    bq25895_v_set_boost_mode( FALSE );
    bq25895_v_set_boost_1500khz();
    bq25895_v_set_boost_mode( TRUE );

    // set boost voltage
    if( boost_voltage == 0 ){

        boost_voltage = 4700;
    }
    else if( boost_voltage > BQ25895_MAX_BOOST_VOLTAGE ){

        boost_voltage = BQ25895_MAX_BOOST_VOLTAGE;
    }
    else if( boost_voltage < BQ25895_MIN_BOOST_VOLTAGE ){

        boost_voltage = BQ25895_MIN_BOOST_VOLTAGE;
    }

    bq25895_v_set_boost_voltage( boost_voltage );
}

void init_charger( void ){

    // turn off charger
    bq25895_v_set_charger( FALSE );

    bq25895_v_set_watchdog( BQ25895_WATCHDOG_OFF );

    // charge config for NCR18650B

    bq25895_v_set_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );

    bq25895_v_set_inlim( 3250 );
    bq25895_v_set_pre_charge_current( 160 );
    
    uint8_t n_cells = batt_cells;

    if( n_cells < 1 ){

        n_cells = 1;
    }

    uint32_t fast_charge_current = 1625 * n_cells;

    if( fast_charge_current > 5000 ){

        fast_charge_current = 5000;
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

    bq25895_v_set_termination_current( 65 );
    bq25895_v_set_charge_voltage( BQ25895_FLOAT_VOLTAGE );

    // disable ILIM pin
    bq25895_v_set_inlim_pin( FALSE );

    bq25895_v_set_iindpm( 3000 );

    // turn off high voltage DCP, and maxcharge
    bq25895_v_clr_reg_bits( BQ25895_REG_MAXC_EN,   BQ25895_BIT_MAXC_EN );
    bq25895_v_clr_reg_bits( BQ25895_REG_HVDCP_EN,  BQ25895_BIT_HVDCP_EN );

    // turn on auto dpdm
    // bq25895_v_set_reg_bits( BQ25895_REG_AUTO_DPDM, BQ25895_BIT_AUTO_DPDM );

    // turn OFF auto dpdm
    bq25895_v_clr_reg_bits( BQ25895_REG_AUTO_DPDM, BQ25895_BIT_AUTO_DPDM );

    // turn off ICO
    // bq25895_v_clr_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );   
}


static bool enable_solar = TRUE;
#define SOLAR_MIN_VBUS 4200

bool vbus_ok( void ){

    // check fault register
    if( bq25895_u8_get_faults() != 0 ){

        return FALSE;
    }

    bq25895_v_start_adc_oneshot();
    // note, current conversion may not be ready,
    // however, this function is used in polling loops so we should
    // have a measurement within the last 1 second
    
    uint16_t vbus = bq25895_u16_get_vbus_voltage();

    return vbus >= SOLAR_MIN_VBUS;
}


PT_THREAD( bat_solar_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    static uint16_t ichg_max;
    static uint16_t vbus_oc;
    static uint16_t vindpm_mpp;

    while(1){

        ichg_max = 0;
        vbus_oc = 0;

        init_charger();

        // bq25895_v_enable_adc_continuous();

        // see note in the wall power thread about sysmin and ADC
        bq25895_v_set_minsys( BQ25895_SYSMIN_3_0V );

        THREAD_WAIT_WHILE( pt, !vbus_ok() );

        bq25895_v_set_minsys( BQ25895_SYSMIN_3_7V );

        do{
            // disable boost converter
            init_boost_converter();
            init_charger();

            bq25895_v_set_hiz( TRUE );
            bq25895_v_start_adc_oneshot();      
            THREAD_WAIT_WHILE( pt, !bq25895_b_adc_ready() );
            // NOTE the boost converter is not operating when in HIZ mode!
            // PMID powered systems will lose power if there is not enough power available on VBUS!

            // get open circuit VBUS
            // note if we power from PMID, we won't have a true open circuit reading on VBUS
            // since we will be drawing current from it!
            vbus_oc = bq25895_u16_get_vbus_voltage();

            // disable HIZ
            bq25895_v_set_hiz( FALSE );      

            vindpm = vbus_oc * 0.65;

            // set minimum vindpm
            if( vindpm < SOLAR_MIN_VBUS ){

                vindpm = SOLAR_MIN_VBUS;
            }

            bq25895_v_set_vindpm( vindpm );

            bq25895_v_set_charger( TRUE );

            log_v_debug_P( PSTR("MPPT: search VBUS_OC: %d"), vbus_oc );

            // search loop:
            while( vindpm < ( vbus_oc * 0.9 ) ){

                TMR_WAIT( pt, 100 );

                bq25895_v_start_adc_oneshot();
                THREAD_WAIT_WHILE( pt, !bq25895_b_adc_ready() );

                // get charge current
                uint16_t ichg = bq25895_u16_get_charge_current();

                if( ichg > ichg_max ){

                    ichg_max = ichg;
                    vindpm_mpp = vindpm;
                }

                log_v_debug_P( PSTR("vindpm: %d ichg: %d vbus: %d"), vindpm, ichg, bq25895_u16_get_vbus_voltage() );

                vindpm += 100;
                bq25895_v_set_vindpm( vindpm );
            }

            // tracking:
            vindpm = vindpm_mpp;
            bq25895_v_set_vindpm( vindpm );

            log_v_debug_P( PSTR("MPPT: tracking: vindpm: %d ichg: %d vbus: %d good: %d"), 
                vindpm, bq25895_u16_get_charge_current(), bq25895_u16_get_vbus_voltage(), bq25895_b_get_vbus_good() );

            // bq25895_v_enable_adc_continuous();

            thread_v_set_alarm( tmr_u32_get_system_time_ms() + 30000 );
            THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && vbus_ok() );

        } while( vbus_ok() );


        log_v_debug_P( PSTR("MPPT VBUS invalid") );
        

        TMR_WAIT( pt, 100 );
    }

PT_END( pt );
}



PT_THREAD( bat_control_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    bq25895_v_set_watchdog( BQ25895_WATCHDOG_OFF );
    // // bq25895_v_enable_adc_continuous();
    // bq25895_v_set_charger( FALSE );

    // bq25895_v_start_adc_oneshot();

    
    // // set min sys
    // // on battery only mode, the battery voltage must be above MINSYS for the ADC
    // // to read correctly.
    // // since MINSYS can only regulate the SYS voltage when plugged in to a power source,
    // // it otherwise isn't very important for our designs other than this ADC consideration.
    // bq25895_v_set_minsys( BQ25895_SYSMIN_3_0V );

    // // turn off charger
    // bq25895_v_set_charger( FALSE );

    // if( !bq25895_b_is_boost_1500khz() ){

    //     // boost controller is set to default

    //     log_v_debug_P( PSTR("Controller not configured - waiting for VBUS") );

    //     // we can only change this with VBUS plugged in, since designs powered by PMID
    //     // will briefly lose power while the boost switches.
    //     THREAD_WAIT_WHILE( pt, !bq25895_b_get_vbus_good() );

    //     log_v_debug_P( PSTR("VBUS OK - resetting config") );
        
    //     // reset settings
    //     bq25895_v_reset();
    // }

    if( enable_solar ){

        log_v_debug_P( PSTR("Solar MPPT enabled, switching control schemes") );

        thread_t_create( bat_solar_thread,
                     PSTR("bat_solar"),
                     0,
                     0 );   

        THREAD_EXIT( pt );
    }


    // wall power algorithm follows from here

    while(1){

      /*

        Charger notes:

        If not using USB, need to short USB data lines to enable DCP mode (1.5A and beyond).
        Otherwise may get limited to 500 mA by DPDM.

        OR turn AUTO_DPDM off.  Remember force DPDM will run even if auto is off, so bypass
        that as well if you don't want DPDM at all.

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


        if( bq25895_b_get_vbus_good() && !vbus_connected ){

            // log_v_debug_P( PSTR("VBUS OK - resetting config") );
            log_v_debug_P( PSTR("VBUS OK") );

            // re-init charger and boost
            init_boost_converter();
            init_charger();

            // re-enable charging
            bq25895_v_set_charger( TRUE );

            vbus_connected = TRUE;

            // set min sys
            // on battery only mode, the battery voltage must be above MINSYS for the ADC
            // to read correctly.
            // since MINSYS can only regulate the SYS voltage when plugged in to a power source,
            // it otherwise isn't very important for our designs other than this ADC consideration.
            bq25895_v_set_minsys( BQ25895_SYSMIN_3_7V );

        }
        else if( vbus_connected ){ 

            // vbus disconnected, but we were previously connected

            if( !bq25895_b_get_vbus_good() ){
                
                // on really crappy USB sources, we might get spurious disconnects.
                // delay, and then check again before resetting.
                TMR_WAIT( pt, 3000 );
            }

            if( !bq25895_b_get_vbus_good() ){

                vbus_connected = FALSE;
                log_v_debug_P( PSTR("VBUS disconnected") );

                // set min sys to 3.0V for ADC accuracy
                bq25895_v_set_minsys( BQ25895_SYSMIN_3_0V );
            }
        }
    
        TMR_WAIT( pt, 1000 );
    }

PT_END( pt );
}


PT_THREAD( bat_mon_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // init battery SOC state
    while( batt_volts == 0 ){
        // loop until we get a good batt reading
        bq25895_v_start_adc_oneshot();
        THREAD_WAIT_WHILE( pt, !bq25895_b_adc_ready() );

        batt_volts = _bq25895_u16_get_batt_voltage();
        soc_state = _calc_batt_soc( batt_volts );
        batt_soc = calc_batt_soc( batt_volts );
        batt_soc_startup = batt_soc;
    }

    if( capacity != 0 ){

        // set baseline energy remaining based on SOC
        remaining = ( capacity * batt_soc ) / 100;
    }
    
    TMR_WAIT( pt, 500 );


    while(1){

        bq25895_v_start_adc_oneshot();
        THREAD_WAIT_WHILE( pt, !bq25895_b_adc_ready() );

        // update status values
        uint16_t temp_batt_volts = _bq25895_u16_get_batt_voltage();
        if( temp_batt_volts != 0 ){

            batt_volts = temp_batt_volts;

            charge_status = bq25895_u8_get_charge_status();
            vbus_volts = bq25895_u16_get_vbus_voltage();
            sys_volts = bq25895_u16_get_sys_voltage();
            batt_charge_current = bq25895_u16_get_charge_current();
            batt_fault = bq25895_u8_get_faults();
            vbus_status = bq25895_u8_get_vbus_status();
            therm = bq25895_i8_get_therm();
        }

        static uint8_t counter;

        if( ( charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE) ||
            ( charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE) ){

            batt_charging = TRUE;

            if( ( counter % 60 ) == 0 ){

                log_v_debug_P( PSTR("batt: %d curr: %d"),
                    batt_volts,
                    batt_charge_current );

                if( batt_fault != 0 ){

                    log_v_debug_P( PSTR("flt:%x"), batt_fault );
                }
            }
        }
        else{ // DISCHARGE

            batt_charging = FALSE;

            if( ( counter % 60 ) == 0 ){

                log_v_debug_P( PSTR("batt: %d soc: %d"),
                    batt_volts, batt_soc );

                if( batt_fault != 0 ){

                    log_v_debug_P( PSTR("flt:%x"), batt_fault );
                }
            }
        }



        counter++;


        if( dump_regs ){

            dump_regs = FALSE;

            bq25895_v_print_regs();
        }

    
        // update state of charge
        // this is a simple linear fit
        uint8_t new_batt_soc = calc_batt_soc( batt_volts );

        // check if battery just ran down to 0,
        // AND we've started from a full charge
        // this will auto-calibrate the battery capacity.
        if( ( batt_soc > 0 ) && 
            ( new_batt_soc == 0 ) &&
            ( batt_soc_startup >= 95 ) ){ // above 95% is close enough to full charge

            // save energy usage as capacity
            capacity = energy_u32_get_total();

            kv_i8_persist( __KV__batt_capacity );

            log_v_info_P( PSTR("Battery capacity calibrated to %u"), capacity );

            batt_soc_startup = 0; // this prevents this from running again until the device has been restarted with a full charge
        }

        batt_soc = new_batt_soc;

        if( capacity != 0 ){

            remaining = (int32_t)capacity - (int32_t)energy_u32_get_total();    
        }
        
        TMR_WAIT( pt, 1000 );
    }

PT_END( pt );
}
