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



#ifndef _USART_BAUDS_H
#define _USART_BAUDS_H

typedef uint8_t baud_t;

#define BAUD_2400       0
#define BAUD_4800       1
#define BAUD_9600       2
#define BAUD_14400      3
#define BAUD_19200      4
#define BAUD_28800      5
#define BAUD_38400      6
#define BAUD_57600      7
#define BAUD_76800      8
#define BAUD_115200     9
#define BAUD_230400     10
#define BAUD_250000     11
#define BAUD_500000     12
#define BAUD_1000000    13
#define BAUD_2000000    14

extern const PROGMEM uint16_t bauds[];

#endif
