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

#include "hal_boards.h"
#include "flash_fs.h"

#include "battery.h"
#include "fuel_gauge.h"
#include "pixel_power.h"
#include "thermal.h"
#include "energy.h"

#include "charger2.h"
#include "bq25895.h"

#include "solar.h"
#include "buttons.h"

#ifdef ENABLE_BATTERY

static bool batt_enable;

#ifdef ENABLE_BATT_MCP73831
#include "mcp73831.h"
static bool batt_enable_mcp73831;
#endif

static bool charger2_board_installed;

static uint16_t batt_max_charge_voltage = BATT_MAX_FLOAT_VOLTAGE;
static uint16_t batt_min_discharge_voltage = BATT_CUTOFF_VOLTAGE;

static uint8_t batt_cells; // number of cells in system
static uint16_t cell_capacity; // mAh capacity of each cell
static uint32_t total_nameplate_capacity;

static bool startup_on_vbus;

static void set_batt_nameplate_capacity( void ){

    uint8_t n_cells = batt_cells;

    if( n_cells < 1 ){

        n_cells = 1;
    }

    if( cell_capacity == 0 ){

        cell_capacity = 3400; // default to NCR18650B
    }

    total_nameplate_capacity = n_cells * cell_capacity;
}


int8_t batt_kv_handler(
    kv_op_t8 op,
    catbus_hash_t32 hash,
    void *data,
    uint16_t len )
{
    if( op == KV_OP_GET ){

    }
    else if( op == KV_OP_SET ){

        if( hash == __KV__batt_max_charge_voltage ){

            // clamp charge voltage
            if( batt_max_charge_voltage > BATT_MAX_FLOAT_VOLTAGE ){

                batt_max_charge_voltage = BATT_MAX_FLOAT_VOLTAGE;
            }

            #ifdef ENABLE_BATT_MCP73831
            if( batt_enable_mcp73831 ){

                // mcp73831 has a fixed charge voltage:
                batt_max_charge_voltage = MCP73831_FLOAT_VOLTAGE;
            }
            #endif
        }
        else if( hash == __KV__batt_min_discharge_voltage ){

            // clamp charge voltage
            if( batt_min_discharge_voltage < BATT_CUTOFF_VOLTAGE ){

                batt_min_discharge_voltage = BATT_CUTOFF_VOLTAGE;
            }
        }   
        else if( ( hash == __KV__batt_cells ) ||
                 ( hash == __KV__batt_cell_capacity ) ||
                 ( hash == __KV__batt_nameplate_capacity ) ){

            set_batt_nameplate_capacity();
        }
    }
    else{

        ASSERT( FALSE );
    }

    return 0;
}

KV_SECTION_META kv_meta_t battery_enable_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable,                 0,  "batt_enable" },

};

#ifdef ENABLE_BATT_MCP73831
KV_SECTION_OPT kv_meta_t battery_enable_mcp73831_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &batt_enable_mcp73831,        0,  "batt_enable_mcp73831" },
};
#endif

KV_SECTION_OPT kv_meta_t battery_info_kv[] = {
    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_PERSIST,    &charger2_board_installed,  0,  "solar_enable_charger2" },

    { CATBUS_TYPE_UINT16, 0, KV_FLAGS_PERSIST,    &batt_max_charge_voltage,     batt_kv_handler,  "batt_max_charge_voltage" },
    { CATBUS_TYPE_UINT16, 0, KV_FLAGS_PERSIST,    &batt_min_discharge_voltage,  batt_kv_handler,  "batt_min_discharge_voltage" },

    { CATBUS_TYPE_UINT8,  0, KV_FLAGS_PERSIST,    &batt_cells,                  batt_kv_handler,  "batt_cells" },
    { CATBUS_TYPE_UINT16, 0, KV_FLAGS_PERSIST,    &cell_capacity,               batt_kv_handler,  "batt_cell_capacity" },
    { CATBUS_TYPE_UINT32, 0, KV_FLAGS_READ_ONLY,  &total_nameplate_capacity,    batt_kv_handler,  "batt_nameplate_capacity" },

    { CATBUS_TYPE_BOOL,   0, KV_FLAGS_READ_ONLY,  &startup_on_vbus,             0,                "batt_vbus_startup" },
};



PT_THREAD( battery_cutoff_thread( pt_t *pt, void *state ) );

void batt_v_init( void ){

    // always init button module
    button_v_init();

    set_batt_nameplate_capacity();

    // check if battery module enabled
    if( !batt_enable ){

        return;
    }

    #ifdef ENABLE_BATT_MCP73831
    kv_v_add_db_info( battery_enable_mcp73831_kv, sizeof(battery_enable_mcp73831_kv) );

    if( batt_enable_mcp73831 ){

        mcp73831_v_init();
    }
    else if( bq25895_i8_init() < 0 ){
    #else
    if( bq25895_i8_init() < 0 ){
    #endif

        log_v_warn_P( PSTR("No battery controlled enabled or detected") );

        return;
    }


    // only add batt info if a battery controller is actually present
    kv_v_add_db_info( battery_info_kv, sizeof(battery_info_kv) );

    set_batt_nameplate_capacity();


    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        // mcp73831 has a fixed charge voltage:
        // need to do this after the KV DB is inited:
        batt_max_charge_voltage = MCP73831_FLOAT_VOLTAGE;
    }
    #endif

    if( charger2_board_installed ){
        
        charger2_v_init();
    }


    energy_v_init();

    fuel_v_init();

    thermal_v_init();

    pixelpower_v_init();


    trace_printf("Battery controller enabled\n");

    thread_t_create( battery_cutoff_thread,
                     PSTR("batt_cutoff_monitor"),
                     0,
                     0 );

    #ifdef ENABLE_SOLAR
    solar_v_init();
    #endif
}

bool batt_b_enabled( void ){

    return batt_enable;
}

bool batt_b_has_charger2_board( void ){

    return charger2_board_installed;
}

uint16_t batt_u16_get_charge_voltage( void ){

    return batt_max_charge_voltage;
}

uint16_t batt_u16_get_min_discharge_voltage( void ){

    // verify the min discharge voltage is in a reasonable range
    if( batt_min_discharge_voltage < BATT_CUTOFF_VOLTAGE ){

        batt_min_discharge_voltage = BATT_CUTOFF_VOLTAGE;
    }

    return batt_min_discharge_voltage;
}

#ifdef ENABLE_BATT_MCP73831
bool batt_b_is_mcp73831_enabled( void ){

    return batt_enable_mcp73831;
}
#endif

void batt_v_enable_charge( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        // MCP73831 has no charge enable control on our boards
        return;
    }
    #endif

    bq25895_v_enable_charger();
}

void batt_v_disable_charge( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        // MCP73831 has no charge enable control on our boards
        return;
    }
    #endif
    
    bq25895_v_disable_charger();   
}

int8_t batt_i8_get_batt_temp( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return -127;
    }
    #endif

    return bq25895_i8_get_temp();
}

uint16_t batt_u16_get_vbus_volts( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return mcp73831_u16_get_vbus_volts();
    }
    #endif

    return bq25895_u16_read_vbus();
}

bool batt_b_is_vbus_connected( void ){

    return batt_u16_get_vbus_volts() >= BATT_MIN_CHARGE_VBUS_VOLTS;
}

uint16_t batt_u16_get_batt_volts( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return mcp73831_u16_get_batt_volts();
    }
    #endif

    return bq25895_u16_get_batt_voltage();
}

uint16_t batt_u16_get_charge_current( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return 0;
    }
    #endif

    return bq25895_u16_get_charge_current();
}

uint8_t batt_u8_get_soc( void ){

    return fuel_u8_get_soc();    
}

bool batt_b_is_charging( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return mcp73831_b_is_charging();        
    }
    #endif

    return bq25895_b_is_charging();
}

bool batt_b_is_charge_complete( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return mcp73831_b_is_charge_complete();
    }
    #endif

    return bq25895_u8_get_charge_status() == BQ25895_CHARGE_STATUS_CHARGE_DONE;
}

bool batt_b_is_external_power( void ){

    if( batt_u16_get_vbus_volts() >= BATT_MIN_CHARGE_VBUS_VOLTS ){

        return TRUE;
    }

    return FALSE;
}

bool batt_b_is_batt_fault( void ){

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        return 0;
    }
    #endif

    return bq25895_u8_get_faults() != 0;
}

uint16_t batt_u16_get_nameplate_capacity( void ){

    return total_nameplate_capacity;
}


void batt_v_shutdown_power( void ){

    log_v_info_P( PSTR("Battery shutdown commanded") );

    #ifdef ENABLE_BATT_MCP73831
    if( batt_enable_mcp73831 ){

        mcp73831_v_shutdown();

        return;
    }
    #endif

    bq25895_v_enable_ship_mode( FALSE );
    bq25895_v_enable_ship_mode( FALSE );
    bq25895_v_enable_ship_mode( FALSE );
}

bool batt_b_startup_on_vbus( void ){

    return startup_on_vbus;
}

PT_THREAD( battery_cutoff_thread( pt_t *pt, void *state ) )
{
PT_BEGIN( pt );
    
    // wait until connection to battery is established
    THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() == 0 );

    // check if VBUS connected on startup    
    startup_on_vbus = batt_b_is_vbus_connected();


    while(1){

        THREAD_WAIT_WHILE( pt, batt_u16_get_batt_volts() >= BATT_EMERGENCY_VOLTAGE );

        log_v_critical_P( PSTR("Battery voltage critical, emergency shutdown") );

        batt_v_shutdown_power();

        TMR_WAIT( pt, 10000 );
    }

PT_END( pt );
}

#endif
