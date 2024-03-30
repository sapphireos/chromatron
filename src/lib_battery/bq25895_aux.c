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

#include "bq25895.h"

#ifdef ENABLE_AUX_BATTERY

static uint16_t aux_batt_volts;
static uint16_t aux_batt_volts_raw;
static uint16_t aux_vbus_volts;
static uint16_t aux_sys_volts;
static uint16_t aux_batt_charge_current;
static uint16_t aux_batt_instant_charge_current;
static uint16_t aux_batt_charge_power;
static uint16_t aux_batt_max_charge_current;
static bool aux_batt_charging;
static uint8_t aux_batt_fault;
static uint8_t aux_vbus_status;
static uint8_t aux_charge_status;
static bool aux_dump_regs;
// static bool aux_boost_enabled;

// static uint16_t aux_boost_voltage;
static uint16_t aux_vindpm;
static uint16_t aux_iindpm;

static uint16_t aux_current_fast_charge_setting;

// true if MCU system power is sourced from the boost converter
// static bool aux_mcu_source_pmid;

// DEBUG
static uint16_t aux_adc_time_min = 65535;
static uint16_t aux_adc_time_max;
static uint32_t aux_adc_good;
static uint32_t aux_adc_fail;

static int8_t aux_batt_temp = -127;
static int16_t aux_batt_temp_state;

static int8_t aux_batt_temp_raw;

static bool aux_present;


KV_SECTION_OPT kv_meta_t bq25895_aux_info_kv[] = {
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &aux_batt_temp,                  0,  "batt_aux_temp" },
    { CATBUS_TYPE_INT8,    0, KV_FLAGS_READ_ONLY,  &aux_batt_temp_raw,              0,  "batt_aux_temp_raw" },
    { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &aux_batt_charging,              0,  "batt_aux_charging" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_batt_volts,                 0,  "batt_aux_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_batt_volts_raw,             0,  "batt_aux_volts_raw" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_vbus_volts,                 0,  "batt_aux_vbus_volts" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_sys_volts,                  0,  "batt_aux_sys_volts" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &aux_charge_status,              0,  "batt_aux_charge_status" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_batt_charge_current,        0,  "batt_aux_charge_current" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_batt_instant_charge_current,0,  "batt_aux_charge_current_instant" },
    
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_current_fast_charge_setting,0,  "batt_aux_charge_current_setting" },
    
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_batt_charge_power,          0,  "batt_aux_charge_power" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &aux_batt_fault,                 0,  "batt_aux_fault" },
    { CATBUS_TYPE_UINT8,   0, KV_FLAGS_READ_ONLY,  &aux_vbus_status,                0,  "batt_aux_vbus_status" },
    
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &aux_batt_max_charge_current,    0,  "batt_aux_max_charge_current" },
    
    // { CATBUS_TYPE_BOOL,    0, KV_FLAGS_READ_ONLY,  &aux_boost_enabled,              0,  "batt_aux_boost_enabled" },
    // { CATBUS_TYPE_UINT16,  0, KV_FLAGS_PERSIST,    &aux_boost_voltage,              0,  "batt_aux_boost_voltage" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_vindpm,                     0,  "batt_aux_vindpm" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_iindpm,                     0,  "batt_aux_iindpm" },

    { CATBUS_TYPE_BOOL,    0, 0,                   &aux_dump_regs,                  0,  "batt_aux_dump_regs" },

    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_adc_time_min,               0,  "batt_aux_adc_time_min" },
    { CATBUS_TYPE_UINT16,  0, KV_FLAGS_READ_ONLY,  &aux_adc_time_max,               0,  "batt_aux_adc_time_max" },

    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &aux_adc_good,                   0,  "batt_aux_adc_reads" },
    { CATBUS_TYPE_UINT32,  0, KV_FLAGS_READ_ONLY,  &aux_adc_fail,                   0,  "batt_aux_adc_fails" },
};


void init_charger( void );
void set_register_bank_main( void );
void set_register_bank_aux( void );
static bool read_adc_aux( void );


#ifdef ENABLE_AUX_BATTERY
PT_THREAD( bq25895_aux_mon_thread( pt_t *pt, void *state ) );
#endif

void bq25895_aux_v_init( void ){

	if( kv_b_get_boolean( __KV__batt_enable_aux ) ){

        set_register_bank_aux();

        // probe for battery charger
        if( bq25895_u8_get_device_id() != BQ25895_DEVICE_ID ){

            log_v_debug_P( PSTR("aux batt controller not found") );
        }
        else{

            kv_v_add_db_info( bq25895_aux_info_kv, sizeof(bq25895_aux_info_kv) );

            log_v_debug_P( PSTR("BQ25895 AUX detected") );

            aux_present = TRUE;

            init_charger();

            thread_t_create( bq25895_aux_mon_thread,
                     PSTR("bq25895_aux"),
                     0,
                     0 );
        }

        set_register_bank_main();
    }
}

void bq25895_aux_v_enable_charger( void ){

	set_register_bank_aux();

	bq25895_v_enable_charger();

	set_register_bank_main();
}

void bq25895_aux_v_disable_charger( void ){

	set_register_bank_aux();

	bq25895_v_disable_charger();

	set_register_bank_main();
}

void bq25895_aux_v_set_vindpm( int16_t mv ){

	set_register_bank_aux();

	bq25895_v_set_vindpm( mv );

	set_register_bank_main();
}

bool bq25895_aux_b_is_batt_fault( void ){

	bool temp = FALSE;

	set_register_bank_aux();

	temp = bq25895_u8_get_faults() != 0;

	set_register_bank_main();

	return temp;
}

uint16_t bq25895_aux_u16_read_vbus( void ){

	uint16_t temp = 0;

	set_register_bank_aux();

	temp = bq25895_u16_read_vbus();

	set_register_bank_main();	

	return temp;
}

bool bq25895_aux_b_is_charging( void ){

	bool temp = FALSE;

	set_register_bank_aux();

	temp = bq25895_b_is_charging();

	set_register_bank_main();

	return temp;
}

uint8_t bq25895_aux_u8_get_charge_status( void ){

	uint8_t temp = 0;

	set_register_bank_aux();

	temp = bq25895_u8_get_charge_status();

	set_register_bank_main();	

	return temp;
}

static bool is_charging( void ){

    return ( aux_charge_status == BQ25895_CHARGE_STATUS_PRE_CHARGE ) ||
           ( aux_charge_status == BQ25895_CHARGE_STATUS_FAST_CHARGE );

}

static bool read_adc_aux( void ){

    aux_batt_fault = bq25895_u8_get_faults();
    aux_vbus_status = bq25895_u8_get_vbus_status();
    aux_vbus_volts = bq25895_u16_get_vbus_voltage();

    uint16_t temp_batt_volts = bq25895_u16_calc_batt_voltage();

    if( temp_batt_volts == 0 ){

        return FALSE;
    }

    if( aux_batt_volts == 0 ){

        aux_batt_volts = temp_batt_volts;
    }

    aux_batt_volts_raw = temp_batt_volts;

    uint16_t temp_charge_current = bq25895_u16_get_charge_current();
    aux_charge_status = bq25895_u8_get_charge_status();
    aux_batt_charging = is_charging();

    if( aux_batt_volts != 0 ){

        aux_batt_volts = util_u16_ewma( temp_batt_volts, aux_batt_volts, BQ25895_VOLTS_FILTER );
    }

    aux_batt_instant_charge_current = temp_charge_current;
    aux_batt_charge_current = util_u16_ewma( temp_charge_current, aux_batt_charge_current, BQ25895_CURRENT_FILTER );
    

    aux_sys_volts = bq25895_u16_get_sys_voltage();
    aux_iindpm = bq25895_u16_get_iindpm();

    int8_t temp = bq25895_i8_get_therm();

    aux_batt_temp_raw = temp;

    if( aux_batt_temp != -127 ){

        aux_batt_temp_state = util_i16_ewma( temp * 256, aux_batt_temp_state, BQ25895_THERM_FILTER );
        aux_batt_temp = aux_batt_temp_state / 256;
    }
    else{

        aux_batt_temp_state = temp * 256;
        aux_batt_temp = temp;
    }
    

    aux_batt_charge_power = ( (uint32_t)aux_batt_charge_current * (uint32_t)aux_batt_volts ) / 1000;

    return TRUE;
}



static bool aux_adc_ready( void ){

    bool temp = FALSE;

    #ifdef ENABLE_AUX_BATTERY
    set_register_bank_aux();
    #endif

    temp = bq25895_b_adc_ready();

    #ifdef ENABLE_AUX_BATTERY
    set_register_bank_main();
    #endif

    return temp;
}

PT_THREAD( bq25895_aux_mon_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );

    // mppt_v_reset();

    TMR_WAIT( pt, 50 );

    while(1){

        static uint32_t start_time;

        #ifdef ENABLE_AUX_BATTERY
        set_register_bank_aux();
        #endif

        bq25895_v_start_adc_oneshot();
        start_time = tmr_u32_get_system_time_ms();

        thread_v_set_alarm( tmr_u32_get_system_time_ms() + 2000 );
        THREAD_WAIT_WHILE( pt, thread_b_alarm_set() && !aux_adc_ready() );

        #ifdef ENABLE_AUX_BATTERY
        set_register_bank_aux();
        #endif

        // read all registers
        bq25895_v_read_all();

        if( aux_dump_regs ){

            aux_dump_regs = FALSE;

            bq25895_v_print_regs();
        }

        if( bq25895_b_adc_ready_cached() && read_adc_aux() ){

            // ADC success

            uint16_t elapsed = tmr_u32_elapsed_time_ms( start_time );

            if( elapsed < aux_adc_time_min ){

                aux_adc_time_min = elapsed;
            }
            
            if( elapsed > aux_adc_time_max ){

                aux_adc_time_max = elapsed;
            }

            aux_adc_good++;

            // run MPPT
            // mppt_v_run( batt_charge_current );
        }
        else{

            aux_adc_fail++;

            TMR_WAIT( pt, 200 );
        }
    }

PT_END( pt );
}


#endif

