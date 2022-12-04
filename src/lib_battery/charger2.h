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

#ifndef _CHARGER2_H
#define _CHARGER2_H

// PCA9536 connections on Charger2
#define CHARGER2_PCA9536_IO_QON     0
#define CHARGER2_PCA9536_IO_S2      1
#define CHARGER2_PCA9536_IO_SPARE   2
#define CHARGER2_PCA9536_IO_BOOST   3



void charger2_v_init( void );

bool charger2_b_read_qon( void );
bool charger2_b_read_s2( void );
bool charger2_b_read_spare( void );


#endif
