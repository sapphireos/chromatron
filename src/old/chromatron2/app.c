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

#include "sapphire.h"

#include "config.h"

#include "app.h"
#include "pixel.h"
#include "graphics.h"
#include "vm.h"
// #include "vm_sync.h"

#include "hal_i2s.h"

SERVICE_SECTION kv_svc_name_t chromatron_service = {"sapphire.device.chromatron"};

void app_v_init( void ){

    // iokv_v_init();

    gfx_v_init();

    vm_v_init();

    // #ifdef ENABLE_TIME_SYNC
    // vm_sync_v_init();
    // #endif

    // hal_i2s_v_init();
    // hal_i2s_v_start( 22050, 24, TRUE );
}

