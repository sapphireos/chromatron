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

#include "vl53l0x.h"

#include "vl53l0x_api.h"
#include "vl53l0x_i2c_platform.h"

static uint16_t distance_mm;

KV_SECTION_META kv_meta_t vl53l0x_kv[] = {
    {CATBUS_TYPE_UINT16,     0, KV_FLAGS_READ_ONLY, &distance_mm,    0, "vl53l0x_distance"},   
    
};


static VL53L0X_Dev_t dev;

void print_pal_error(VL53L0X_Error Status){
    char buf[VL53L0X_MAX_STRING_LENGTH];
    VL53L0X_GetPalErrorString(Status, buf);
    trace_printf("API Status: %i : %s\n", Status, buf);
}

void print_range_status(VL53L0X_RangingMeasurementData_t* pRangingMeasurementData){
    char buf[VL53L0X_MAX_STRING_LENGTH];
    uint8_t RangeStatus;

    /*
     * New Range Status: data is valid when pRangingMeasurementData->RangeStatus = 0
     */

    RangeStatus = pRangingMeasurementData->RangeStatus;

    VL53L0X_GetRangeStatusString(RangeStatus, buf);
    trace_printf("Range Status: %i : %s\n", RangeStatus, buf);

}


PT_THREAD( vl53l0x_thread( pt_t *pt, void *state ) )
{       	
PT_BEGIN( pt );  

    int32_t status = 0;

    dev.I2cDevAddr = VL53L0X_I2C_ADDR;

    uint8_t byte = 0;

    int8_t tries = 16;

    while( tries > 0 ){

        tries--;

        byte = 0;
        VL53L0X_read_byte( &dev, 0xC0, &byte );
        if( byte != 0xEE ){

            // log_v_debug_P( PSTR("0x%02x"), byte );

            continue;
        }

        byte = 0;
        VL53L0X_read_byte( &dev, 0xC1, &byte );
        if( byte != 0xAA ){

            // log_v_debug_P( PSTR("0x%02x"), byte );

            continue;
        }

        byte = 0;
        VL53L0X_read_byte( &dev, 0xC2, &byte );
        if( byte != 0x10 ){

            // log_v_debug_P( PSTR("0x%02x"), byte );

            continue;
        }

        break;
    }

    if( tries < 0 ){

        goto end;
    }

    status = VL53L0X_DataInit( &dev ); // Data initialization

    if( status != 0 ){

        log_v_error_P( PSTR("Error: %d"), status );

        goto end;
    }

    log_v_info_P( PSTR("VL53L0X detected") );            

    status = VL53L0X_StaticInit( &dev ); // Device Initialization
    if( status != 0 ){

        log_v_error_P( PSTR("Error: %d"), status );

        goto end;
    }

    uint8_t VhvSettings;
    uint8_t PhaseCal;
    status = VL53L0X_PerformRefCalibration( &dev, &VhvSettings, &PhaseCal );
    if( status != 0 ){

        log_v_error_P( PSTR("Error: %d"), status );

        goto end;
    }

    uint32_t refSpadCount;
    uint8_t isApertureSpads;
    status = VL53L0X_PerformRefSpadManagement( &dev, &refSpadCount, &isApertureSpads );
    if( status != 0 ){

        log_v_error_P( PSTR("Error: %d"), status );

        goto end;
    }

    // Enable/Disable Sigma and Signal check
    // if( status == VL53L0X_ERROR_NONE ){
    //     status = VL53L0X_SetLimitCheckEnable( &dev,
    //             VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1 );
    // }
    // if( status == VL53L0X_ERROR_NONE ){
    //     status = VL53L0X_SetLimitCheckEnable( &dev,
    //             VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1 );
    // }

    // if( status == VL53L0X_ERROR_NONE ){
    //     status = VL53L0X_SetLimitCheckEnable( &dev,
    //             VL53L0X_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 1 );
    // }

    // if( status == VL53L0X_ERROR_NONE ){
    //     status = VL53L0X_SetLimitCheckValue( &dev,
    //             VL53L0X_CHECKENABLE_RANGE_IGNORE_THRESHOLD,
    //             (FixPoint1616_t)(1.5*0.023*65536) );
    // }

    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetLimitCheckEnable(&dev,
                VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
    }
    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetLimitCheckEnable(&dev,
                VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
    }
                
    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetLimitCheckValue(&dev,
                VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
                (FixPoint1616_t)(0.1*65536));
    }           
    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetLimitCheckValue(&dev,
                VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
                (FixPoint1616_t)(60*65536));            
    }
    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(&dev,
                33000);
    }
    
    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetVcselPulsePeriod(&dev, 
                VL53L0X_VCSEL_PERIOD_PRE_RANGE, 18);
    }
    if (status == VL53L0X_ERROR_NONE) {
        status = VL53L0X_SetVcselPulsePeriod(&dev, 
                VL53L0X_VCSEL_PERIOD_FINAL_RANGE, 14);
    }

    VL53L0X_SetDeviceMode(&dev, VL53L0X_DEVICEMODE_CONTINUOUS_RANGING);
    VL53L0X_StartMeasurement(&dev);

    while(1){

        TMR_WAIT( pt, 25 );

        int32_t status = VL53L0X_ERROR_NONE;
        VL53L0X_RangingMeasurementData_t    RangingMeasurementData;
        FixPoint1616_t LimitCheckCurrent;
        
        // status = VL53L0X_PerformSingleRangingMeasurement(&dev,
        //         &RangingMeasurementData);
        status = VL53L0X_GetRangingMeasurementData(&dev,
                &RangingMeasurementData);

        // print_pal_error(Status);
        // print_range_status(&RangingMeasurementData);

        VL53L0X_GetLimitCheckCurrent(&dev,
                VL53L0X_CHECKENABLE_RANGE_IGNORE_THRESHOLD, &LimitCheckCurrent);

        // trace_printf("RANGE IGNORE THRESHOLD: %f\n\n", (float)LimitCheckCurrent/65536.0);

        if( status != VL53L0X_ERROR_NONE ){

            log_v_error_P( PSTR("Error: %d"), status );

            TMR_WAIT( pt, 5000 );

            continue;
        }

        distance_mm = RangingMeasurementData.RangeMilliMeter;

        if( distance_mm >= VL53L0X_MAX_VALUE ){

            distance_mm = 0xffff;            
        }

        // trace_printf("Measured distance: %i\n\n", RangingMeasurementData.RangeMilliMeter);
    }


end:
    ;

PT_END( pt );	
}



void vl53l0x_v_init( void ){

    i2c_v_init( I2C_BAUD_400K );

    thread_t_create( vl53l0x_thread,
                     PSTR("vl53l0x"),
                     0,
                     0 );
}

