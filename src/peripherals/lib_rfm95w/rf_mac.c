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

#ifdef ESP32

#include "rfm95w.h"
#include "rf_mac.h"

#include "flash_fs.h"
#include "hal_boards.h"

/*



Basic power consumptions:
Rx: 10-12 mA

Tx:
7 dBm:  20 mA
13 dBm: 29 mA
17 dBm: 87 mA

20 dBm is technically available (120 mA), but requires a 1% duty cycle.
We will stick to 17 dBm for simplicity.

Minimum bandwidth is 62.5 KHz.
(below that requires a TCXO, which we don't have)

We are using Band 1 (900 MHz)


FCC requirements:
https://www.sunfiretesting.com/LoRa-FCC-Certification-Guide/

At 17 dBm, we are good on power limits.

For digital spread spectrum with no FHSS, (15.247(a)(2)), 
we must occupy at least 500 KHz of bandwidth at -6 dB.
Our 500 KHz bandwidth probably hits this, but none of the others do.

Below 500 KHz, we must use FHSS.

FHSS requirements:
Channels must be 25 KHz apart at least, or 20 dB bandwidth.

If 20dB bandwidth < 250 KHz, we need >= 50 channels using
at most 400 msec per channel in a 20 second span.
- bandwidths below 250 KHz probably qualify here.

If 20dB bandwidth > 250 KHz, we need >= 25 channels using
at most 400 msec per channel in a 10 second span.
- our 250 KHz bandwidth setting should cover this.

The 50 channel/20 second program should cover everything.








Lora bitrate is SF * ( BW / (2**SF) ) * CR

Lora

SF  BW      CR    Bitrate  Airtime (64 bytes, 512 bytes) (ms)
12  62.5    4/8   91       5626  45008
12  500     4/8   732      699   5592
12  62.5    4/5   146      3506  28048
12  500     4/5   73       7013  56104

6   62.5    4/8   2929     174   1392
6   500     4/8   23437    21    168
6   62.5    4/5   4687     109   872
6   500     4/5   37500    13    104




*/


int8_t rf_mac_i8_init( void ){

    if( sys_u8_get_mode() == SYS_MODE_SAFE ){

        return 0;
    }

    uint8_t board = ffs_u8_read_board_type();

    if( board == BOARD_TYPE_ELITE ){

        rfm95w_v_init( IO_PIN_27_A10, IO_PIN_26_A0 );

        return 0;
    }

    // no radio present
    return -1;
}

#endif