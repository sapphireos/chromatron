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

#ifndef _BQ25895_H_
#define _BQ25895_H_

#define BQ25895_MIN_BOOST_VOLTAGE       4550
#define BQ25895_MAX_BOOST_VOLTAGE       5510

#define BQ25895_MIN_VINDPM              3900
#define BQ25895_MAX_VINDPM              15300

#define BQ25895_MAX_FAST_CHARGE_CURRENT 5000

#define BQ25895_TERM_CURRENT            ( 65 * 2 )

// #define BQ25895_SOFT_START
// #define BQ25895_SOFT_START_INITIAL_CHARGE   2000
// #define BQ25895_SOFT_START_CHARGE_INCREMENT 10

#define BQ25895_CHARGE_TEMP_LIMIT		38
#define BQ25895_CHARGE_TEMP_LIMIT_LOWER	36

// NOTE! The datasheet lists the address as 0x6B in the serial Interface
// overview, but then lists it as 0x6A in the register description.
// the 0x6B is a typo!
#define BQ25895_I2C_ADDR 0x6A

#define BQ25895_DEVICE_ID 0x39

#define BQ25895_N_REGS  21

#define BQ25895_REG_INPUT_CURRENT           0x00
#define BQ25895_BIT_ENABLE_ILIM_PIN         ( 1 << 6 )
#define BQ25895_MASK_INPUT_CURRENT_LIM      0x3F
#define BQ25895_BIT_ENABLE_HIZ              ( 1 << 7 )

#define BQ25895_REG_ADC                     0x02
#define BQ25895_BIT_ADC_CONV_RATE           ( 1 << 6 )
#define BQ25895_BIT_ADC_CONV_START          ( 1 << 7 )

#define BQ25895_REG_ICO                     0x02
#define BQ25895_BIT_ICO_EN                  ( 1 << 4 )

#define BQ25895_REG_AUTO_DPDM               0x02
#define BQ25895_BIT_AUTO_DPDM               ( 1 << 0 )

#define BQ25895_REG_FORCE_DPDM              0x02
#define BQ25895_BIT_FORCE_DPDM              ( 1 << 1 )

#define BQ25895_REG_MAXC_EN                 0x02
#define BQ25895_BIT_MAXC_EN                 ( 1 << 2 )

#define BQ25895_REG_HVDCP_EN                0x02
#define BQ25895_BIT_HVDCP_EN                ( 1 << 3 )

#define BQ25895_REG_BOOST_FREQ              0x02
#define BQ25895_BIT_BOOST_FREQ              ( 1 << 5 )

#define BQ25895_REG_WATCHDOG_RST            0x03
#define BQ25895_BIT_WATCHDOG_RST            ( 1 << 6 )

#define BQ25895_REG_CHARGE_EN               0x03
#define BQ25895_BIT_CHARGE_EN               ( 1 << 4 )

#define BQ25895_REG_BOOST_EN                0x03
#define BQ25895_BIT_BOOST_EN                ( 1 << 5 )

#define BQ25895_REG_MINSYS_EN               0x03
#define BQ25895_MASK_MINSYS            		0x0E
#define BQ25895_SHIFT_MINSYS           		1

#define BQ25895_REG_FAST_CHARGE_CURRENT     0x04
#define BQ25895_MASK_FAST_CHARGE            0x7F

#define BQ25895_REG_PRE_CHARGE_CURRENT      0x05
#define BQ25895_MASK_PRE_CHARGE             0xF0
#define BQ25895_SHIFT_PRE_CHARGE            4

#define BQ25895_REG_TERM_CURRENT            0x05
#define BQ25895_MASK_TERM                   0x0F

#define BQ25895_REG_CHARGE_VOLTS            0x06
#define BQ25895_MASK_CHARGE_VOLTS           0xFC
#define BQ25895_SHIFT_CHARGE_VOLTS          2
#define BQ25895_BIT_BATLOWV                ( 1 << 1 )
#define BQ25895_BIT_VRECHG                 ( 1 << 0 )

#define BQ25895_REG_WATCHDOG                0x07
#define BQ25895_MASK_WATCHDOG               0x30
#define BQ25895_SHIFT_WATCHDOG              4

#define BQ25895_REG_CHARGE_TIMER            0x07
#define BQ25895_MASK_CHARGE_TIMER           0x06
#define BQ25895_SHIFT_CHARGE_TIMER          1

#define BQ25895_REG_SHIP_MODE               0x09
#define BQ25895_BIT_BATFET_DIS              ( 1 << 5 )
#define BQ25895_BIT_BATFET_DLY              ( 1 << 3 )
#define BQ25895_BIT_BATFET_RST_EN           ( 1 << 2 )

#define BQ25895_REG_BOOST_VOLTS             0x0A
#define BQ25895_MASK_BOOST_VOLTS            0xF0
#define BQ25895_SHIFT_BOOST_VOLTS           4

#define BQ25895_REG_VBUS_STATUS             0x0B
#define BQ25895_MASK_VBUS_STATUS            0xE0
#define BQ25895_SHIFT_VBUS_STATUS           5

#define BQ25895_REG_CHARGE_STATUS           0x0B
#define BQ25895_MASK_CHARGE_STATUS          0x18
#define BQ25895_SHIFT_CHARGE_STATUS         3

#define BQ25895_REG_POWER_GOOD              0x0B
#define BQ25895_BIT_POWER_GOOD              ( 1 << 2 )
#define BQ25895_BIT_SDP_STAT                ( 1 << 1 )
#define BQ25895_BIT_VSYS_STAT               ( 1 << 0 )

#define BQ25895_REG_FAULT                   0x0C
#define BQ25895_BIT_WATCHDOG_FAULT          ( 1 << 7 )
#define BQ25895_BIT_BOOST_FAULT             ( 1 << 6 )
#define BQ25895_MASK_CHRG_FAULT             0x30
#define BQ25895_SHIFT_CHRG_FAULT            4
#define BQ25895_BIT_BAT_FAULT               ( 1 << 3 )
#define BQ25895_MASK_NTC_FAULT              0x07
#define BQ25895_SHIFT_NTC_FAULT             0

#define BQ25895_REG_VINDPM                  0x0D
#define BQ25895_BIT_FORCE_VINDPM            ( 1 << 7 )
#define BQ25895_MASK_VINDPM                 0x7F

#define BQ25895_REG_BATT_VOLTAGE            0x0E
#define BQ25895_BIT_THERM_STAT              ( 1 << 7 )
#define BQ25895_MASK_BATT_VOLTAGE           0x7F

#define BQ25895_REG_SYS_VOLTAGE             0x0F
#define BQ25895_MASK_SYS_VOLTAGE            0x7F

#define BQ25895_REG_THERM                   0x10

#define BQ25895_REG_VBUS_GOOD               0x11
#define BQ25895_BIT_VBUS_GOOD               ( 1 << 7 )

#define BQ25895_REG_VBUS_VOLTAGE            0x11
#define BQ25895_MASK_VBUS_VOLTAGE           0x7F

#define BQ25895_REG_CHARGE_CURRENT          0x12
#define BQ25895_MASK_CHARGE_CURRENT         0x7F

#define BQ25895_REG_IINDPM                  0x13
#define BQ25895_MASK_IINDPM                 0x3F
#define BQ25895_BIT_VINDPM                  ( 1 << 7 )
#define BQ25895_BIT_IINDPM                  ( 1 << 6 )

#define BQ25895_REG_RESET                   0x14
#define BQ25895_BIT_RESET                   ( 1 << 7 )

#define BQ25895_REG_DEV_ID                  0x14
#define BQ25895_MASK_DEV_ID                 0x3B


#define BQ25895_WATCHDOG_OFF                0
#define BQ25895_WATCHDOG_40S                1
#define BQ25895_WATCHDOG_80S                2
#define BQ25895_WATCHDOG_160S               3

#define BQ25895_CHARGE_TIMER_5HR            0
#define BQ25895_CHARGE_TIMER_8HR            1
#define BQ25895_CHARGE_TIMER_12HR           2
#define BQ25895_CHARGE_TIMER_20HR           3

#define BQ25895_CHARGE_STATUS_NOT_CHARGING  0
#define BQ25895_CHARGE_STATUS_PRE_CHARGE    1
#define BQ25895_CHARGE_STATUS_FAST_CHARGE   2
#define BQ25895_CHARGE_STATUS_CHARGE_DONE   3

#define BQ25895_SYSMIN_3_0V					0
#define BQ25895_SYSMIN_3_1V					1
#define BQ25895_SYSMIN_3_2V					2
#define BQ25895_SYSMIN_3_3V					3
#define BQ25895_SYSMIN_3_4V					4
#define BQ25895_SYSMIN_3_5V					5
#define BQ25895_SYSMIN_3_6V					6
#define BQ25895_SYSMIN_3_7V					6

int8_t bq25895_i8_init( void );

void bq25895_v_read_all( void );
uint8_t bq25895_u8_read_reg( uint8_t addr );
void bq25895_v_write_reg( uint8_t addr, uint8_t data );
void bq25895_v_set_reg_bits( uint8_t addr, uint8_t mask );
void bq25895_v_clr_reg_bits( uint8_t addr, uint8_t mask );

void bq25895_v_reset( void );
void bq25895_v_set_inlim_pin( bool enable );
void bq25895_v_set_hiz( bool enable );
void bq25895_v_set_inlim( uint16_t current );
uint16_t bq25895_u16_get_inlim( void );
void bq25895_v_enable_adc_continuous( void );
void bq25895_v_start_adc_oneshot( void );
bool bq25895_b_adc_ready( void );
void bq25895_v_set_boost_1500khz( void );
bool bq25895_b_is_boost_1500khz( void );
void bq25895_v_set_boost_mode( bool enable );
bool bq25895_b_is_boost_enabled( void );
void bq25895_v_force_dpdm( void );
void bq25895_v_set_minsys( uint8_t sysmin );
void bq25895_v_set_fast_charge_current( uint16_t current );
void bq25895_v_set_pre_charge_current( uint16_t current );
void bq25895_v_set_termination_current( uint16_t current );
void bq25895_v_set_charge_voltage( uint16_t volts );
void bq25895_v_set_batlowv( bool high );
void bq25895_v_enable_ship_mode( bool delay );
void bq25895_v_leave_ship_mode( void );
void bq25895_v_set_system_reset( bool enable );
void bq25895_v_set_boost_voltage( uint16_t volts );
uint8_t bq25895_u8_get_vbus_status( void );
bool bq25895_b_get_vbus_good( void );
uint8_t bq25895_u8_get_charge_status( void );
bool bq25895_b_is_charging( void );
bool bq25895_b_power_good( void );
uint8_t bq25895_u8_get_faults( void );
uint16_t bq25895_u16_get_batt_voltage( void );
uint16_t bq25895_u16_get_vbus_voltage( void );
uint16_t bq25895_u16_get_sys_voltage( void );
uint16_t bq25895_u16_get_charge_current( void );
int8_t bq25895_i8_calc_temp( uint8_t ratio );
int8_t bq25895_8_get_therm( void );
int8_t bq25895_i8_get_temp( void );
void bq25895_v_set_watchdog( uint8_t setting );
void bq25895_v_kick_watchdog( void );
void bq25895_v_set_charger( bool enable );
void bq25895_v_set_charge_timer( uint8_t setting );
uint8_t bq25895_u8_get_device_id( void );
void bq25895_v_print_regs( void );
void bq25895_v_set_vindpm( int16_t mv );
uint16_t bq25895_u16_get_iindpm( void );
bool bq25895_b_get_vindpm( void );
bool bq25895_b_get_iindpm( void );
uint16_t bq25895_u16_read_vbus( void );
// int8_t bq25895_i8_get_case_temp( void );
// int8_t bq25895_i8_get_ambient_temp( void );

void bq25895_v_enable_charger( void );
void bq25895_v_disable_charger( void );

#endif
