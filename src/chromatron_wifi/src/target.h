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

#ifndef _TARGET_H
#define _TARGET_H

#ifndef PROGMEM
	#define PROGMEM
	// #define pgm_read_word(a) *a
#endif

// #define USE_HSV_BRIDGE
#define MAX_PIXELS              320
#define FADER_RATE              20
#define USE_GFX_LIB

#define VM_TARGET_ESP
#define VM_ENABLE_GFX
#define VM_ENABLE_KV
// #define VM_ENABLE_CATBUS

#define VM_MAX_VMS                  4
#define VM_MAX_CALL_DEPTH           8
#define VM_MAX_THREADS              4
#define VM_MAX_IMAGE_SIZE   4096
#define VM_MAX_CYCLES       32768

#endif
