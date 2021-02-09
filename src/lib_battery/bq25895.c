/*
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
 */

#include "sapphire.h"

#include "i2c.h"

#include "bq25895.h"
#include "util.h"

static uint8_t batt_soc; // state of charge in percent
static uint8_t batt_soc_startup; // state of charge at power on
static uint16_t batt_volts;
static uint16_t vbus_volts;
static uint16_t sys_volts;
static uint16_t batt_charge_current;
static bool batt_charging;
static bool vbus_connected;
static uint8_t batt_fault;
static uint8_t vbus_status;
static uint8_t charge_status;
static bool dump_regs;


KV_SECTION_META kv_meta_t bat_info_kv[] = {
    { SAPPHIRE_TYPE_UINT8,   0, 0,                   &batt_soc,             0,  "batt_soc" },
    { SAPPHIRE_TYPE_BOOL,    0, 0,                   &batt_charging,        0,  "batt_charging" },
    { SAPPHIRE_TYPE_BOOL,    0, 0,                   &vbus_connected,       0,  "batt_external_power" },
    { SAPPHIRE_TYPE_UINT16,  0, 0,                   &batt_volts,           0,  "batt_volts" },
    { SAPPHIRE_TYPE_UINT16,  0, 0,                   &vbus_volts,           0,  "batt_vbus_volts" },
    { SAPPHIRE_TYPE_UINT16,  0, 0,                   &sys_volts,            0,  "batt_sys_volts" },
    { SAPPHIRE_TYPE_UINT8,   0, 0,                   &charge_status,        0,  "batt_charge_status" },
    { SAPPHIRE_TYPE_UINT16,  0, 0,                   &batt_charge_current,  0,  "batt_charge_current" },
    { SAPPHIRE_TYPE_UINT8,   0, 0,                   &batt_fault,           0,  "batt_fault" },
    { SAPPHIRE_TYPE_UINT8,   0, 0,                   &vbus_status,          0,  "batt_vbus_status" },
    { SAPPHIRE_TYPE_UINT64,  0, KV_FLAGS_PERSIST,    0,                     0,  "batt_capacity" },

    { SAPPHIRE_TYPE_BOOL,    0, 0,                   &dump_regs,            0,  "batt_dump_regs" },
};

#define SOC_MAX_VOLTS   4150
#define SOC_MIN_VOLTS   3200


PT_THREAD( bat_mon_thread( pt_t *pt, void *state ) );


static uint8_t calc_batt_soc( uint16_t volts ){

    if( volts < SOC_MIN_VOLTS ){

        return 0;
    }

    if( volts > SOC_MAX_VOLTS ){

        return 100;
    }

    return util_u16_linear_interp( volts, SOC_MIN_VOLTS, 0, SOC_MAX_VOLTS, 100 );
}

int8_t bq25895_i8_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    // probe for battery charger
    if( bq25895_u8_get_device_id() != BQ25895_DEVICE_ID ){

        log_v_debug_P( PSTR("batt controller not found") );

        return -1;
    }

    batt_volts = bq25895_u16_get_batt_voltage();
    batt_soc = calc_batt_soc( batt_volts );
    batt_soc_startup = batt_soc;

    thread_t_create( bat_mon_thread,
                     PSTR("bat_mon"),
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

void bq25895_v_set_boost_1500khz( void ){

    // default is 500k

    bq25895_v_clr_reg_bits( BQ25895_REG_BOOST_FREQ, BQ25895_BIT_BOOST_FREQ );
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

    if( volts > 5510 ){

        volts = 5510;
    }
    else if( volts < 4550 ){

        volts = 4550;
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


PT_THREAD( bat_mon_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    TMR_WAIT( pt, 500 );

    // reconfigure

    // bq25895_v_enable_ship_mode();
    // bq25895_v_leave_ship_mode();

    // bq25895_v_force_dpdm();

    // bq25895_v_print_regs();

    bq25895_v_set_watchdog( BQ25895_WATCHDOG_OFF );
    bq25895_v_enable_adc_continuous();
    bq25895_v_set_boost_voltage( 5100 );
    bq25895_v_set_charger( FALSE );

    // bq25895_v_print_regs();

    while(1){

        charge_status = bq25895_u8_get_charge_status();
        batt_volts = bq25895_u16_get_batt_voltage();
        vbus_volts = bq25895_u16_get_vbus_voltage();
        sys_volts = bq25895_u16_get_sys_voltage();
        batt_charge_current = bq25895_u16_get_charge_current();
        batt_fault = bq25895_u8_get_faults();
        vbus_status = bq25895_u8_get_vbus_status();

        /*

        Charger notes:

        If not using USB, need to short USB data lines to enable DCP mode (1.5A and beyond).
        Otherwise may get limited to 500 mA by DPDM.

        OR turn AUTO_DPDM off.  Remember force DPDM will run even if auto is off, so bypass
        that as well if you don't want DPDM at all.

        */


        // set MINSYS to 3.0V for ADC accuracy.  VBAT must be greater than MINSYS.

        if( bq25895_b_get_vbus_good() && !vbus_connected ){

            log_v_debug_P( PSTR("VBUS OK - resetting config") );

            // reset settings
            bq25895_v_reset();

            // turn off charger
            bq25895_v_set_charger( FALSE );

            // turn off high voltage DCP, and maxcharge
            bq25895_v_clr_reg_bits( BQ25895_REG_MAXC_EN,   BQ25895_BIT_MAXC_EN );
            bq25895_v_clr_reg_bits( BQ25895_REG_HVDCP_EN,  BQ25895_BIT_HVDCP_EN );

            // bq25895_v_write_reg( BQ25895_REG_VINDPM, 0 );
            // bq25895_v_set_reg_bits( BQ25895_REG_VINDPM, BQ25895_BIT_FORCE_VINDPM );

            // turn on auto dpdm
            // bq25895_v_set_reg_bits( BQ25895_REG_AUTO_DPDM, BQ25895_BIT_AUTO_DPDM );

            // turn OFF auto dpdm
            bq25895_v_clr_reg_bits( BQ25895_REG_AUTO_DPDM, BQ25895_BIT_AUTO_DPDM );

            bq25895_v_set_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );
            // bq25895_v_clr_reg_bits( BQ25895_REG_ICO, BQ25895_BIT_ICO_EN );

            // boost frequency can only be changed when OTG boost is turned off.
            bq25895_v_set_boost_mode( FALSE );
            bq25895_v_set_boost_1500khz();
            bq25895_v_set_boost_mode( TRUE );

            bq25895_v_set_watchdog( BQ25895_WATCHDOG_OFF );
            bq25895_v_enable_adc_continuous();
            bq25895_v_set_boost_voltage( 5100 );

            // charge config for NCR18650B
            bq25895_v_set_inlim( 2000 );
            // bq25895_v_set_inlim( 3000 );
            bq25895_v_set_pre_charge_current( 160 );
            bq25895_v_set_fast_charge_current( 1625 );
            // bq25895_v_set_fast_charge_current( 250 );
            // bq25895_v_set_fast_charge_current( 3000 );
            bq25895_v_set_termination_current( 65 );
            bq25895_v_set_charge_voltage( 4200 );


            // disable ILIM pin
            bq25895_v_set_inlim_pin( FALSE );

            // run auto DPDM (which can override the current limits)
            // bq25895_v_force_dpdm();

            // re-enable charging
            bq25895_v_set_charger( TRUE );

            vbus_connected = TRUE;
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
            }
        }


        // if( status != 0 ){

            // log_v_debug_P( PSTR("batt: %d curr: %d vbus: %x %umV good?:%d chrg: %x inlim: %u fault:%x"),
            //     bq25895_u16_get_batt_voltage(),
            //     bq25895_u16_get_charge_current(),
            //     bq25895_u8_get_vbus_status(),
            //     bq25895_u16_get_vbus_voltage(),
            //     bq25895_b_get_vbus_good(),
            //     status,
            //     bq25895_u16_get_inlim(),
            //     bq25895_u8_get_faults() );

        static uint8_t counter;

        if( ( charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE) ||
            ( charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE) ){

            batt_charging = TRUE;

            if( ( counter % 60 ) == 0 ){

                log_v_debug_P( PSTR("batt: %d curr: %d"),
                    batt_volts,
                    bq25895_u16_get_charge_current() );

                if( batt_fault != 0 ){

                    log_v_debug_P( PSTR("flt:%x"), batt_fault );
                }
            }
        }
        else{
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
        // if( ( batt_soc != 0 ) && 
        //     ( new_batt_soc == 0 ) &&
        //     ( batt_soc_startup >= 95 ) ){ // above 95% is close enough to full charge

        //     // save energy usage as capacity
        //     uint64_t energy_total = energy_u64_get_total();

        //     kv_i8_set( BQ25895_KV_GROUP,
        //                KV_ID_ENERGY_TOTAL,
        //                &energy_total,
        //                sizeof(energy_total) );
        // }

        batt_soc = new_batt_soc;

        TMR_WAIT( pt, 1000 );
        // TMR_WAIT( pt, 10000 );
    }

PT_END( pt );
}
