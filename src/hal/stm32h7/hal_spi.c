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
#include "hal_io.h"


typedef struct{
    uint32_t pin;
    uint32_t alt;
    uint8_t apb; // which APB bus the port is on
} hal_spi_ch_t;

static const hal_spi_ch_t spi_io[] = {
#ifdef BOARD_CHROMATRONX
	{ IO_PIN_GPIOSCK, GPIO_AF5_SPI4, 2},
    { IO_PIN_GPIOMOSI, GPIO_AF5_SPI4, 2 },
    { IO_PIN_GPIOMISO, GPIO_AF5_SPI4, 2 },
#else
    { IO_PIN_GPIOSCK, GPIO_AF5_SPI4, 2},
    { IO_PIN_GPIOMOSI, GPIO_AF5_SPI4, 2 },
    { IO_PIN_GPIOMISO, GPIO_AF5_SPI4, 2 },
#endif
};

static SPI_HandleTypeDef spi;
static uint32_t actual_freq;

void spi_v_init( uint8_t channel, uint32_t freq ){

	ASSERT( channel < N_SPI_PORTS );

	// get the bus clock for this port
	uint32_t bus_clock = 0;

	// switch( spi_io[channel].apb ){

	// 	case 1:
	// 	case 3:
	// 	case 4:
	// 		bus_clock = HAL_RCC_GetPCLK1Freq();
	// 		break;

	// 	case 2:
	// 		bus_clock = HAL_RCC_GetPCLK2Freq();
	// 		break;
	// }

	// SPI4 is on PLL2
	PLL2_ClocksTypeDef pll2_clk;
	HAL_RCCEx_GetPLL2ClockFreq( &pll2_clk );
	bus_clock = pll2_clk.PLL2_P_Frequency;

	spi.Instance = SPI4;
	__HAL_RCC_SPI4_CLK_ENABLE();

	GPIO_TypeDef *port;
	GPIO_InitTypeDef GPIO_InitStruct;
	
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;	

	hal_io_v_get_port( spi_io[0].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = spi_io[0].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );

	hal_io_v_get_port( spi_io[1].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = spi_io[1].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );

	hal_io_v_get_port( spi_io[2].pin, &port, &GPIO_InitStruct.Pin );
    GPIO_InitStruct.Alternate = spi_io[2].alt;
    HAL_GPIO_Init( port, &GPIO_InitStruct );

	uint32_t prescaler = 2;
	uint32_t baud = 0;

	// search for closest prescaler
	for( ; prescaler <= 255; prescaler *= 2 ){

		// closest match at or below target frequency
		if( ( bus_clock / prescaler ) <= freq ){

			break;
		}

		baud++;
	}

	actual_freq = bus_clock / prescaler;

	spi.Init.Mode 							= SPI_MODE_MASTER;
	spi.Init.Direction 						= SPI_DIRECTION_2LINES;
	spi.Init.DataSize 						= SPI_DATASIZE_8BIT;
	spi.Init.CLKPolarity 					= SPI_POLARITY_LOW;
	spi.Init.CLKPhase 						= SPI_PHASE_1EDGE;
	spi.Init.NSS 							= SPI_NSS_SOFT;
	spi.Init.BaudRatePrescaler 				= baud << 28;
	spi.Init.FirstBit 						= SPI_FIRSTBIT_MSB;
	spi.Init.TIMode 						= SPI_TIMODE_DISABLE;
	spi.Init.CRCCalculation 				= SPI_CRCCALCULATION_DISABLE;
	spi.Init.CRCPolynomial 					= 0;
	spi.Init.CRCLength 						= 0;
	spi.Init.NSSPMode 						= SPI_NSS_PULSE_DISABLE;
	spi.Init.NSSPolarity 					= SPI_NSS_POLARITY_LOW;
	spi.Init.FifoThreshold 					= SPI_FIFO_THRESHOLD_01DATA;
	spi.Init.TxCRCInitializationPattern 	= SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	spi.Init.RxCRCInitializationPattern 	= SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	spi.Init.MasterSSIdleness 				= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	spi.Init.MasterInterDataIdleness 		= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	spi.Init.MasterReceiverAutoSusp 		= SPI_MASTER_RX_AUTOSUSP_DISABLE;
	spi.Init.MasterKeepIOState 				= SPI_MASTER_KEEP_IO_STATE_ENABLE;
	spi.Init.IOSwap 						= SPI_IO_SWAP_DISABLE;

	HAL_SPI_Init( &spi );
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	ASSERT( channel < N_SPI_PORTS );

	return actual_freq;
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

	ASSERT( channel < N_SPI_PORTS );

	uint8_t rx_data;
	
	HAL_SPI_TransmitReceive( &spi, &data, &rx_data, 1, 250 );

	return rx_data;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

	HAL_SPI_Transmit( &spi, (uint8_t *)data, length, 1000 );
}

void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );
	
	HAL_SPI_Receive( &spi, data, length, 1000 );
}


/*
typedef struct{
	SPI_TypeDef *spi;
	USART_TypeDef *usart;
	uint8_t apb; // which APB bus the port is on
	uint32_t pin0;
	GPIO_TypeDef *port0;
	uint8_t alt0;
	uint32_t pin1;
	GPIO_TypeDef *port1;
	uint8_t alt1;
	uint32_t pin2;
	GPIO_TypeDef *port2;
	uint8_t alt2;
} port_def_t;

static const port_def_t port_defs[] = {
	
	{
		SPI4, 
		0, 
		2, 
		MOSI_Pin, 
		MOSI_GPIO_Port, 
		GPIO_AF5_SPI4,
		MISO_Pin, 
		MISO_GPIO_Port, 
		GPIO_AF5_SPI4,
		SCK_Pin, 
		SCK_GPIO_Port, 
		GPIO_AF5_SPI4,
	}, 		// SPI4 on Nuclear J4
	#ifdef BOARD_CHROMATRONX
	{
		0, 
		USART6, 
		2,
		AUX_SPI_MOSI_Pin,
		AUX_SPI_MOSI_GPIO_Port,
		GPIO_AF7_USART6,
		AUX_SPI_MISO_Pin,
		AUX_SPI_MISO_GPIO_Port,
		GPIO_AF7_USART6,
		AUX_SPI_SCK_Pin,
		AUX_SPI_SCK_GPIO_Port,
		GPIO_AF7_USART6,
	},		// AUX SPI on Chromatron X (not on a connector)
	#endif
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

	__HAL_RCC_SPI4_CLK_ENABLE();

	GPIO_InitTypeDef GPIO_InitStruct;
	
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;	

    // init IO
	GPIO_InitStruct.Pin 		= port_defs[channel].pin0;
	GPIO_InitStruct.Alternate 	= port_defs[channel].alt0;
	HAL_GPIO_Init( port_defs[channel].port0, &GPIO_InitStruct );
	
	GPIO_InitStruct.Pin 		= port_defs[channel].pin1;
	GPIO_InitStruct.Alternate 	= port_defs[channel].alt1;
	HAL_GPIO_Init( port_defs[channel].port1, &GPIO_InitStruct );

	GPIO_InitStruct.Pin 		= port_defs[channel].pin2;
	GPIO_InitStruct.Alternate 	= port_defs[channel].alt2;
	HAL_GPIO_Init( port_defs[channel].port2, &GPIO_InitStruct );


	if( port_defs[channel].spi != 0 ){
		// SPI type
		ports[channel].type = TYPE_SPI;

		SPI_HandleTypeDef *spi = &ports[channel].spi_usart.spi;
		spi->Instance = port_defs[channel].spi;

		uint32_t prescaler = 2;
		uint32_t baud = 0;

		// search for closest prescaler
		for( ; prescaler <= 255; prescaler *= 2 ){

			// closest match at or below target frequency
			if( ( bus_clock / prescaler ) <= freq ){

				break;
			}

			baud++;
		}

		ports[channel].actual_freq = bus_clock / prescaler;

		spi->Init.Mode 							= SPI_MODE_MASTER;
		spi->Init.Direction 					= SPI_DIRECTION_2LINES;
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
		spi->Init.MasterSSIdleness 				= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
		spi->Init.MasterInterDataIdleness 		= SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
		spi->Init.MasterReceiverAutoSusp 		= SPI_MASTER_RX_AUTOSUSP_DISABLE;
		spi->Init.MasterKeepIOState 			= SPI_MASTER_KEEP_IO_STATE_ENABLE;
		spi->Init.IOSwap 						= SPI_IO_SWAP_DISABLE;

		HAL_SPI_Init( spi );
	}
	else{
		// USART in SPI mode
		ports[channel].type = TYPE_USART;

		USART_HandleTypeDef *usart = &ports[channel].spi_usart.usart;
		usart->Instance = port_defs[channel].usart;

		usart->Init.BaudRate 		= freq;
		usart->Init.WordLength 		= USART_WORDLENGTH_8B;
		usart->Init.StopBits 		= USART_STOPBITS_1;
		usart->Init.Parity 			= USART_PARITY_NONE;
		usart->Init.Mode 			= USART_MODE_TX_RX;
		usart->Init.CLKPolarity 	= USART_POLARITY_LOW;
		usart->Init.CLKPhase 		= USART_PHASE_1EDGE;
		usart->Init.CLKLastBit 		= USART_LASTBIT_DISABLE;
		usart->Init.Prescaler 		= USART_PRESCALER_DIV2; // might have to change
		usart->Init.NSS 			= USART_NSS_SW;
		usart->Init.SlaveMode 		= USART_SLAVEMODE_DISABLE;
		usart->Init.FIFOMode 		= USART_FIFOMODE_DISABLE;
		usart->Init.TXFIFOThreshold	= USART_TXFIFO_THRESHOLD_1_4;
		usart->Init.RXFIFOThreshold = USART_RXFIFO_THRESHOLD_1_4;

		HAL_USART_Init( usart );	
	}
}

uint32_t spi_u32_get_freq( uint8_t channel ){

	ASSERT( channel < N_SPI_PORTS );

	return ports[channel].actual_freq;
}

uint8_t spi_u8_send( uint8_t channel, uint8_t data ){

	ASSERT( channel < N_SPI_PORTS );

	uint8_t rx_data;

	if( ports[channel].type == TYPE_SPI ){

		HAL_SPI_TransmitReceive( &ports[channel].spi_usart.spi, &data, &rx_data, 1, 250 );
	}
	else{

		HAL_USART_TransmitReceive( &ports[channel].spi_usart.usart, &data, &rx_data, 1, 250 );
	}

	return rx_data;
}

void spi_v_write_block( uint8_t channel, const uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

	if( ports[channel].type == TYPE_SPI ){

		HAL_SPI_Transmit( &ports[channel].spi_usart.spi, (uint8_t *)data, length, 1000 );
	}
	else{

		HAL_USART_Transmit( &ports[channel].spi_usart.usart, (uint8_t *)data, length, 1000 );
	}
}

void spi_v_read_block( uint8_t channel, uint8_t *data, uint16_t length ){

	ASSERT( channel < N_SPI_PORTS );

	if( ports[channel].type == TYPE_SPI ){

		HAL_SPI_Receive( &ports[channel].spi_usart.spi, data, length, 1000 );
	}
	else{

		HAL_USART_Receive( &ports[channel].spi_usart.usart, data, length, 1000 );
	}
}
*/
