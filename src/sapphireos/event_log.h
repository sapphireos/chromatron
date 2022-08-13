
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

#ifndef _EVENT_LOG_H
#define _EVENT_LOG_H

#include "system.h"

#include "events.h"

#include "catbus_common.h"


// #define ENABLE_EVENT_LOG

#define EVENT_LOG_MAX_ENTRIES           1024
#define EVENT_LOG_SLACK_SPACE           16
#define EVENT_LOG_RECORD_INTERVAL       32

// if defined, event log will stop logging after
// EVENT_LOG_MAX_ENTRIES is written.
#define EVENT_LOG_ONE_SHOT



typedef struct{
    catbus_hash_t32 event_id;
    uint32_t param;
    uint32_t timestamp;
} event_t;


#define EVENT_LOG_MAX_SIZE ( EVENT_LOG_MAX_ENTRIES * sizeof(event_t) )



// prototypes:

void event_v_init( void );


#ifdef ENABLE_EVENT_LOG

    void event_v_log( catbus_hash_t32 event_id, uint32_t param );
    void event_v_flush( void );

#else
    #define event_v_flush() {};

#endif

#if defined(ENABLE_EVENT_LOG) && !defined(NO_EVENT_LOGGING)

    #define EVENT(id, param) event_v_log(id, param)

#else

    #define EVENT(id, param)

#endif


#endif
