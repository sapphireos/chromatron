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
#include "logging.h"
#include "gfx_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The sim compiles with -fpack-struct / -fshort-enums, which trips SDL's
 * "enums are int sized" static assert.  We deliberately skip it instead of
 * changing the sim's build flags: this file never depends on the layout of
 * an SDL structure that carries an enum member or on a sub-4-byte struct
 * offset (see the note above) - it uses opaque pointers, the event union's
 * offset-0 Uint32 type tag, and by-value flag enums, which x86-64 passes
 * in a register with the low bits intact regardless of the argument size.
 */
#define DOXYGEN_SHOULD_IGNORE_THIS
#include <SDL2/SDL.h>
#undef DOXYGEN_SHOULD_IGNORE_THIS

#include "hal_sim_sdl.h"

/*

The simulator is a single threaded cooperative process, so all the SDL
calls below run from whichever thread happens to be drawing: the pixel
thread for frames (sim_pixel_thread), the idle wait for events.  No
locking is needed.



*/

// target longest window side, in host pixels
#define SDL_TARGET_SIZE 480

// biggest upscaling factor: keeps a single pixel array from opening a
// window the size of the display when the geometry is 1x1
#define SDL_MAX_SCALE 32

static bool sdl_requested = FALSE;
static bool sdl_dead = FALSE; // init failed, or the window was closed

static SDL_Window *sdl_window;
static SDL_Renderer *sdl_renderer;
static SDL_Texture *sdl_texture;

// staging image: XRGB8888, one cell per pixel plus a 1px gutter
static uint32_t *sdl_buffer;

// current geometry of the staging image / texture
static uint16_t tex_w;
static uint16_t tex_h;

void sim_sdl_v_enable( void ){

    sdl_requested = TRUE;
}

bool sim_sdl_b_enabled( void ){

    return sdl_requested && !sdl_dead;
}

// size of a pixel cell, in host pixels (the gutter is drawn on top of
// the last row and column of each cell)
static uint16_t calc_cell_size( uint16_t cells_x, uint16_t cells_y ){

    uint16_t longest = ( cells_x > cells_y ) ? cells_x : cells_y;

    if( longest == 0 ){

        return SDL_MAX_SCALE;
    }

    uint16_t cell = SDL_TARGET_SIZE / longest;

    if( cell == 0 ){

        cell = 1; // cells of 1px still show the array, just small
    }
    else if( cell > SDL_MAX_SCALE ){

        cell = SDL_MAX_SCALE;
    }

    return cell;
}

// grid geometry for the current pixel configuration
static void calc_grid( uint16_t *cells_x, uint16_t *cells_y ){

    uint16_t count = gfx_u16_get_physical_pix_count();

    uint16_t x = gfx_u16_get_pix_size_x();
    uint16_t y = gfx_u16_get_pix_size_y();

    if( x == 0 ){

        // no 2D configuration: fall back to a square
        x = 1;

        while( ( (uint32_t)x * x ) < count ){

            x++;
        }
    }

    if( ( y == 0 ) || ( ( (uint32_t)x * y ) < count ) ){

        // rows to hold the whole array
        y = ( count + x - 1 ) / x;
    }

    if( x == 0 ){

        x = 1;
    }

    if( y == 0 ){

        y = 1;
    }

    *cells_x = x;
    *cells_y = y;
}

// physical array index of grid coordinate x, y.  This is the 2D half of
// the mapping in gfx_lib.c:calc_index() for the full array: the window
// shows what the strips receive, so the array's transpose / interleave
// / invert settings are honored, but a VM's sub array offset and
// reverse are not (they belong to the program, not to the hardware).
static uint16_t calc_pix_index(
    uint16_t x,
    uint16_t y,
    uint16_t cells_x,
    uint16_t cells_y,
    uint16_t count )
{
    if( gfx_b_get_transpose() ){

        uint16_t temp = y;
        y = x;
        x = temp;
    }

    if( gfx_b_get_interleave_x() ){

        if( ( y % cells_y ) & 1 ){

            x = ( cells_x - 1 ) - ( x % cells_x );
        }
    }

    if( gfx_b_get_invert_x() ){

        x = ( cells_x - 1 ) - ( x % cells_x );
    }

    x %= cells_x;
    y %= cells_y;

    return ( x + ( y * cells_x ) ) % count;
}

// (re)create the window and staging image for the current geometry
static bool create_window( uint16_t cells_x, uint16_t cells_y ){

    uint16_t cell = calc_cell_size( cells_x, cells_y );

    uint16_t new_w = cells_x * cell;
    uint16_t new_h = cells_y * cell;

    if( sdl_window != 0 ){

        if( ( new_w == tex_w ) && ( new_h == tex_h ) ){

            return TRUE; // geometry unchanged
        }

        // the pixel configuration changed size: tear the whole thing
        // down and rebuild it (cheaper than tracking four sizes
        // independently, and this is rare)
        SDL_DestroyTexture( sdl_texture );
        sdl_texture = 0;

        SDL_DestroyRenderer( sdl_renderer );
        sdl_renderer = 0;

        SDL_DestroyWindow( sdl_window );
        sdl_window = 0;

        free( sdl_buffer );
        sdl_buffer = 0;
    }

    uint32_t count = (uint32_t)new_w * new_h;

    sdl_buffer = calloc( count, sizeof(uint32_t) );

    if( sdl_buffer == 0 ){

        log_v_error_P( PSTR("sim sdl: out of memory (%d x %d)"), new_w, new_h );
        sdl_dead = TRUE;
        return FALSE;
    }

    sdl_window = SDL_CreateWindow(
        "Chromatron simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        new_w,
        new_h,
        0 );

    if( sdl_window == 0 ){

        log_v_error_P( PSTR("sim sdl: %s"), SDL_GetError() );
        sdl_dead = TRUE;
        return FALSE;
    }

    sdl_renderer = SDL_CreateRenderer( sdl_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );

    if( sdl_renderer == 0 ){

        // no accelerated renderer (or no vsync): plain software
        sdl_renderer = SDL_CreateRenderer( sdl_window, -1, 0 );
    }

    if( sdl_renderer == 0 ){

        log_v_error_P( PSTR("sim sdl: %s"), SDL_GetError() );
        sdl_dead = TRUE;
        return FALSE;
    }

    sdl_texture = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        new_w,
        new_h );

    if( sdl_texture == 0 ){

        log_v_error_P( PSTR("sim sdl: %s"), SDL_GetError() );
        sdl_dead = TRUE;
        return FALSE;
    }

    tex_w = new_w;
    tex_h = new_h;

    printf( "sim: SDL window open: %d x %d pixels\n", cells_x, cells_y );
    fflush( stdout );

    return TRUE;
}

static bool init_sdl( void ){

    if( SDL_Init( SDL_INIT_VIDEO ) != 0 ){

        // headless host, or no display server: the simulator keeps
        // running without the window
        printf( "sim sdl: %s\n", SDL_GetError() );
        sdl_dead = TRUE;
        return FALSE;
    }

    return TRUE;
}

void sim_sdl_v_frame( void ){

    if( ! sim_sdl_b_enabled() ){

        return;
    }

    if( sdl_window == 0 ){

        if( ! init_sdl() ){

            return;
        }
    }

    sim_sdl_v_pump();

    if( sdl_dead ){

        return;
    }

    uint16_t cells_x;
    uint16_t cells_y;

    calc_grid( &cells_x, &cells_y );

    if( ! create_window( cells_x, cells_y ) ){

        return;
    }

    uint16_t count = gfx_u16_get_physical_pix_count();

    if( count == 0 ){

        return; // nothing to show yet: leave the last frame up
    }

    const uint8_t *array_r = gfx_u8p_get_red();
    const uint8_t *array_g = gfx_u8p_get_green();
    const uint8_t *array_b = gfx_u8p_get_blue();

    uint16_t cell = tex_w / cells_x;
    uint16_t pitch = tex_w;

    for( uint16_t cy = 0; cy < cells_y; cy++ ){

        for( uint16_t cx = 0; cx < cells_x; cx++ ){

            uint16_t index = calc_pix_index( cx, cy, cells_x, cells_y, count );

            // opaque, with the gutter drawn as the last row/column of
            // the cell so the individual pixels stay visible
            uint32_t color = 0xFF000000 |
                ( (uint32_t)array_r[index] << 16 ) |
                ( (uint32_t)array_g[index] << 8 ) |
                ( (uint32_t)array_b[index] );

            uint32_t *dst = &sdl_buffer[ ( (uint32_t)( cy * cell ) * pitch ) + ( cx * cell ) ];

            for( uint16_t row = 0; row < cell; row++ ){

                uint32_t *line = dst + ( (uint32_t)row * pitch );

                for( uint16_t col = 0; col < cell; col++ ){

                    line[col] = ( ( row + 1 ) == cell ) || ( ( col + 1 ) == cell ) ? 0xFF000000 : color;
                }
            }
        }
    }

    SDL_UpdateTexture( sdl_texture, 0, sdl_buffer, tex_w * (int)sizeof(uint32_t) );
    SDL_RenderClear( sdl_renderer );
    SDL_RenderCopy( sdl_renderer, sdl_texture, 0, 0 );
    SDL_RenderPresent( sdl_renderer );
}

void sim_sdl_v_pump( void ){

    if( ! sim_sdl_b_enabled() ){

        return;
    }

    SDL_Event event;

    bool got_event = FALSE;

    while( SDL_PollEvent( &event ) ){

        got_event = TRUE;

        // only the union's offset-0 Uint32 type tag is read here; the
        // struct members themselves have layouts the sim's -fpack-struct
        // flag would get wrong, so no key codes are taken from the event
        if( event.type == SDL_QUIT ){

            // closing the window stops the simulator, like killing it:
            // there is nothing else for the process to do
            printf( "sim: SDL window closed, exiting\n" );
            fflush( stdout );

            exit( 0 );
        }
    }

    if( got_event ){

        // ESC quits too: the scan-code array is plain bytes, so it is
        // safe to read despite the packing flags
        const Uint8 *keys = SDL_GetKeyboardState( 0 );

        if( keys != 0 && keys[ SDL_SCANCODE_ESCAPE ] ){

            printf( "sim: SDL escape pressed, exiting\n" );
            fflush( stdout );

            exit( 0 );
        }
    }
}
