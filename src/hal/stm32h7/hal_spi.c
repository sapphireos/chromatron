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

#include "cpu.h"
#include "system.h"
#include "spi.h"
#include "hal_spi.h"

typedef struct{
	SPI_TypeDef *spi;
	USART_TypeDef *usart;
	uint8_t apb; // which APB bus the port is on
} port_def_t;

static const port_def_t port_defs[] = {
	{SPI4, 0, 2}, 		// SPI4 on Nuclear J4
	{0, USART6, 2},		// AUX SPI on Chromatron X (not on a connector)
};


#define N_SPI_PORTS cnt_of_array(port_defs)


typedef union{
	SPI_HandleTypeDef spi;
	USART_HandleTypeDef usart;
} spi_usart_t;

#define TYPE_SPI 0
#define TYPE_USART 1

typedef struct{
	uint8_t type;
	uint32_t actual_freq;
	spi_usart_t spi_usart;
} spi_port_t;

static spi_port_t ports[N_SPI_PORTS];



void spi_v_init( uint8_t channel, uint32_t freq ){

	ASSERT( channel < N_SPI_PORTS );

	// get the bus clock for this port
	uint32_t bus_clock = 0;

	switch( port_defs[channel].apb ){

		case 1:
		case 3:
		case 4:
			bus_clock = HAL_RCC_GetPCLK1Freq();
			break;

		case 2:
			bus_clock = HAL_RCC_GetPCLK2Freq();
			break;
	}

	if( port_defs[channel].spi != 0 ){
		// SPI type
		ports[channel].type = TYPE_SPI;

		SPI_HandleTypeDef *spi = &ports[channel].spi_usart.spi;
		spi->Instance = port_defs[channel].spi;

		uint32_t prescaler = 2;
		uint32_t baud = 0;

		// search for closest prescaler
		for( ; prescaler <= 256; prescaler *= 2 ){

			// closest match at or below target frequency
			if( ( bus_clock / prescaler ) <= freq ){

				break;
			}

			baud++;
		}

		ports[channel].actual_freq = bus_clock / prescaler;

		spi->Init.Mode 							= SPI_MODE_MASTER;
		spi->Init.Direction 					= SPI_DIRECTION_1LINE;
		spi->Init.DataSize 						= SPI_DATASIZE_8BIT;
		spi->Init.CLKPolarity 					= SPI_POLARITY_LOW;
		spi->Init.CLKPhase 						= SPI_PHASE_1EDGE;
		spi->Init.NSS 							= SPI_NSS_SOFT;
		spi->Init.BaudRatePrescaler 			= baud << 28;
		spi->Init.FirstBit 						= SPI_FIRSTBIT_MSB;
		spi->Init.TIMode 						= SPI_TIMODE_DISABLE;
		spi->Init.CRCCalculation 				= SPI_CRCCALCULATION_DISABLE;
		spi->Init.CRCPolynomial 				= 0;
		spi->Init.CRCLength 					= 0;
		spi->Init.NSSPMode 						= SPI_NSS_PULSE_DISABLE;
		spi->Init.NSSPolarity 					= SPI_NSS_POLARITY_LOW;
		spi->Init.FifoThreshold 				= SPI_FIFO_THRESHOLD_01DATA;
		spi->Init.TxCRCInitializationPattern 	= SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
		spi->Init.RxCRCInitializationPattern 	= SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
		spi->Init.MasterSSIdleness 				= SPI_MASTER_SS_IDLENESS_00CYCLE;
		spi->Init.MasterInterDataIdleness 		= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
		spi->Init.MasterReceiverAutoSusp 		= SPI_MASTER_RX_AUTOSUSP_DISABLE;
		spi->Init.MasterKeepIOState 			= SPI_MASTER_KEEP_IO_STATE_DISABLE;
		spi->Init.IOSwap 						= SPI_IO_SWAP_DISABLE;

		HAL_SPI_Init( spi );
	}
	else{
		// USART in SPI mode
		ports[channel].type = TYPE_USART;

		USART_HandleTypeDef *usart = &ports[channel].spi_usart.usart;
		usart->Instance = port_defs[channel].usart;


	}
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	ASSERT( channel < N_SPI_PORTS );

	return ports[channel].actual_freq;
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

	ASSERT( channel < N_SPI_PORTS );

	return 0;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

}

void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

}

