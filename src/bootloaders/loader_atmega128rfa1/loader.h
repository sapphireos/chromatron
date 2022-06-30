/* 
 * <license>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 *
 * This file is part of the Sapphire Operating System
 *
 * Copyright 2013 Sapphire Open Systems
 *
 * </license>
 */
 

#ifndef __LOADER_H
#define __LOADER_H

#include "target.h"
#include "system.h"

#define LDR_VERSION_MAJOR   '1'
#define LDR_VERSION_MINOR   '4'

#define FW_LENGTH_ADDRESS 0x120 // this must match the offset in the makefile!

#define MAX_PROGRAM_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)N_APP_PAGES ) )
#define FLASH_LENGTH ( (uint32_t)( (uint32_t)PAGE_SIZE * (uint32_t)TOTAL_PAGES ) )


void ldr_run_app( void );

void ldr_v_set_green_led( void );
void ldr_v_clear_green_led( void );
void ldr_v_set_yellow_led( void );
void ldr_v_clear_yellow_led( void );
void ldr_v_set_red_led( void );
void ldr_v_clear_red_led( void );

void ldr_v_set_clock_prescaler( sys_clock_t8 prescaler );

void ldr_v_erase_app( void );
void ldr_v_copy_partition_to_internal( void );
void ldr_v_read_partition_data( uint32_t offset, uint8_t *dest, uint16_t length );

void ldr_v_set_clock_prescaler( sys_clock_t8 prescaler );

uint32_t ldr_u32_read_partition_length( void );
uint32_t ldr_u32_read_internal_length( void );

uint16_t ldr_u16_get_internal_crc( void );
uint16_t ldr_u16_get_partition_crc( void );

#ifdef LOADER_MODULE_TEST
void ldrmain( void );
#endif




#endif

