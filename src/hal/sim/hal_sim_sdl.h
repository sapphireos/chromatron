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

#ifndef _HAL_SIM_SDL_H
#define _HAL_SIM_SDL_H

/*

SDL front end for the simulator: draws the pixel array in a window on
the host screen.

The window shows the framebuffer the graphics engine hands to the pixel
driver (gfx_u8p_get_red/green/blue, the same bytes the strips receive
after dithering/ordering is applied), laid out with the pixel array's
configured geometry (pix_size_x / pix_size_y, transpose, x interleave
and x invert).

It is switched on by the --sdl command line option (see hal_sim.c).
Everything is created lazily on the first frame, because the pixel
configuration is not loaded when the sim platform initializes (timers
initialize before the KV system).

*/

// turn the display on: call from main() before the OS starts
void sim_sdl_v_enable( void );

// TRUE when the display was requested (--sdl)
bool sim_sdl_b_enabled( void );

// draw the current framebuffer: call once per pixel frame, after the
// graphics engine has updated the RGB arrays (PIX_SIGNAL_0)
void sim_sdl_v_frame( void );

// run the host window event loop: called from the idle wait so the
// window stays responsive while the simulator is idle
void sim_sdl_v_pump( void );

#endif
