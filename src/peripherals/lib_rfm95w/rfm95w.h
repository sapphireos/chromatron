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

#ifndef _RFM95W_H_
#define _RFM95W_H_

#define RFM95W_FIFO_LEN             64
// using the FSK/OOK value of 64 bytes.
// the Lora modem can do 256 bytes, but
// we will restrict to 64 so we handle
// both modem types the same way.

#define RFM95W_FOSC                 32000000

#define RFM95W_FREQ_MIN             903000000
#define RFM95W_FREQ_MAX             927000000

#define RFM95W_SPREADING_MIN        6
#define RFM95W_SPREADING_MAX        12

#define RFM95W_POWER_MAX			17
#define RFM95W_POWER_MIN			2

#define RFM95W_PREAMBLE_MIN         10

#define RFM95W_RegFifo              0x00
#define RFM95W_RegOpMode            0x01

#define RFM95W_OP_MODE_MASK         0x07
#define RFM95W_OP_MODE_SLEEP        0
#define RFM95W_OP_MODE_STANDBY      1
#define RFM95W_OP_MODE_FSTX         2
#define RFM95W_OP_MODE_TX           3
#define RFM95W_OP_MODE_FSRX         4
#define RFM95W_OP_MODE_RXCONT       5
#define RFM95W_OP_MODE_RXSINGLE     6
#define RFM95W_OP_MODE_CAD          7
#define RFM95W_BIT_LORA_MODE        0x80
#define RFM95W_BIT_LF_MODE          0x08

#define RFM95W_RegFrMsb             0x06
#define RFM95W_RegFrMid             0x07
#define RFM95W_RegFrLsb             0x08

#define RFM95W_RegPaConfig          0x09
#define RFM95W_BIT_PABOOST          0x80
#define RFM95W_SHFT_MAX_POWER       4
#define RFM95W_MASK_MAX_POWER       0x07
#define RFM95W_MASK_OUT_POWER       0x0F


#define RFM95W_RegFifoAddrPtr       0x0D
#define RFM95W_RegFifoTxBaseAddr    0x0E
#define RFM95W_RegFifoRxBaseAddr    0x0F
#define RFM95W_RegFifoRxCurrentAddr 0x10

#define RFM95W_RegIrqFlagsMask      0x11
#define RFM95W_BIT_RxTimeoutMask            ( 1 << 7 )
#define RFM95W_BIT_RxDoneMask               ( 1 << 6 )
#define RFM95W_BIT_PayloadCrcErrorMask      ( 1 << 5 )
#define RFM95W_BIT_ValidHeaderMask          ( 1 << 4 )
#define RFM95W_BIT_TxDoneMask               ( 1 << 3 )
#define RFM95W_BIT_CadDoneMask              ( 1 << 2 )
#define RFM95W_BIT_FhssChangeChannelMask    ( 1 << 1 )
#define RFM95W_BIT_CadDetectedMask          ( 1 << 0 )

#define RFM95W_RegIrqFlags          0x12
#define RFM95W_BIT_RxTimeout         ( 1 << 7 )
#define RFM95W_BIT_RxDone            ( 1 << 6 )
#define RFM95W_BIT_PayloadCrcError   ( 1 << 5 )
#define RFM95W_BIT_ValidHeader       ( 1 << 4 )
#define RFM95W_BIT_TxDone            ( 1 << 3 )
#define RFM95W_BIT_CadDone           ( 1 << 2 )
#define RFM95W_BIT_FhssChangeChannel ( 1 << 1 )
#define RFM95W_BIT_CadDetected       ( 1 << 0 )

#define RFM95W_RegRxNbBytes             0x13
#define RFM95W_RegRxHeaderCntValueMsb   0x14
#define RFM95W_RegRxHeaderCntValueLsb   0x15
#define RFM95W_RegRxPacketCntValueMsb   0x16
#define RFM95W_RegRxPacketCntValueLsb   0x17

#define RFM95W_RegModemStat             0x18
#define RFM95W_MASK_RX_CODING_RATE      0x07
#define RFM95W_SHIFT_RX_CODING_RATE     5
#define RFM95W_BIT_ModemClear           ( 1 << 4 )
#define RFM95W_BIT_HeaderValid          ( 1 << 3 )
#define RFM95W_BIT_RX_ongoing           ( 1 << 2 )
#define RFM95W_BIT_SignalSync           ( 1 << 1 )
#define RFM95W_BIT_SignalDetected       ( 1 << 0 )

#define RFM95W_RegPktSnrValue       0x19
#define RFM95W_RegPktRssiValue      0x1A
#define RFM95W_RegRssiValue         0x1B

#define RFM95W_RegModemConfig       0x1D
#define RFM95W_MASK_BW              0x0F
#define RFM95W_SHIFT_BW             4
#define RFM95W_MASK_CODING_RATE     0x07
#define RFM95W_SHIFT_CODING_RATE    1
#define RFM95W_BIT_ImplicitHeader   ( 1 << 0 )

#define RFM95W_BW_7800              0
#define RFM95W_BW_10400             1
#define RFM95W_BW_15600             2
#define RFM95W_BW_20800             3
#define RFM95W_BW_31200             4
#define RFM95W_BW_41700             5
#define RFM95W_BW_62500             6
#define RFM95W_BW_125000            7
#define RFM95W_BW_250000            8
#define RFM95W_BW_500000            9

#define RFM95W_CODING_MIN           1
#define RFM95W_CODING_MAX           4

#define RFM95W_CODING_4_5           1
#define RFM95W_CODING_4_6           2
#define RFM95W_CODING_4_7           3
#define RFM95W_CODING_4_8           4

#define RFM95W_RegModemConfig2      0x1E
#define RFM95W_MASK_SF              0x0F
#define RFM95W_SHIFT_SF             4
#define RFM95W_BIT_TxContinuousMode ( 1 << 3 )
#define RFM95W_BIT_RxPayloadCrcOn   ( 1 << 2 )
#define RFM95W_MASK_RX_TIMEOUT_MSB  0x03

#define RFM95W_RegSymbTimeoutLsb    0x1F
#define RFM95W_RegPreambleMsb       0x20
#define RFM95W_RegPreambleLsb       0x21

#define RFM95W_RegPayloadLength     0x22
#define RFM95W_RegMaxPayloadLength  0x23
#define RFM95W_RegHopPeriod         0x24
#define RFM95W_RegFifoRxByteAddr    0x25

#define RFM95W_RegModemConfig3      0x26
#define RFM95W_BIT_LowDataRateOptimize ( 1 << 3 )

#define RFM95W_RegDetectOptimize        0x31
#define RFM95W_MASK_DetectionOptimize   0x07

#define RFM95W_RegDetectionThreshold    0x37

// Sync word
// Default is 0x12
// LoraWAN is 0x34
#define RFM95W_RegSyncWord      	0x39


int8_t rfm95w_i8_init( uint8_t cs, uint8_t reset );

// low level api
uint8_t rfm95w_u8_read_reg( uint8_t addr );
void rfm95w_v_write_reg( uint8_t addr, uint8_t data );
void rfm95w_v_set_reg_bits( uint8_t addr, uint8_t mask );
void rfm95w_v_clr_reg_bits( uint8_t addr, uint8_t mask );
void rfm95w_v_read_fifo( uint8_t *buf, uint8_t len );
void rfm95w_v_write_fifo( uint8_t *buf, uint8_t len );

// config api
void rfm95w_v_set_mode( uint8_t mode );
void rfm95w_v_set_spreading_factor( uint8_t data );
void rfm95w_v_set_coding_rate( uint8_t data );
void rfm95w_v_set_bandwidth( uint8_t data );
void rfm95w_v_set_power( int8_t data );
void rfm95w_v_set_frequency( uint32_t freq );
int16_t rfm95w_i16_get_rssi( void );
int16_t rfm95w_i16_get_packet_rssi( void );
int16_t rfm95w_i16_get_packet_snr( void );
void rfm95w_v_set_preamble_length( uint16_t data );
void rfm95w_v_set_sync_word( uint8_t data );

// status api
void rfm95w_v_clear_irq_flags( void );
bool rfm95w_b_is_rx_done( void );
bool rfm95w_b_is_rx_busy( void );
bool rfm95w_b_is_rx_ok( void );
bool rfm95w_b_is_tx_done( void );

// oacket API
// bool rfm95w_b_is_rx_ready( void );
uint8_t rfm95w_u8_read_rx_len( void );
void rfm95w_v_get_rx_data( uint8_t *data, uint8_t len );

// bool rfm95w_b_is_tx_busy( void );
// bool rfm95w_b_is_tx_ready( void );
// bool rfm95w_b_transmit( uint8_t *data, uint8_t len );
void rfm95w_v_transmit( void );

#endif





