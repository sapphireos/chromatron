// <license>
// 
//     This file is part of the Sapphire Operating System.
// 
//     Copyright (C) 2013-2021  Jeremy Billheimer
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


#include "system.h"
#include "target.h"
#include "trace.h"

#ifdef ENABLE_COPROCESSOR
#include "coprocessor.h"
#endif

#include "osapi.h"

#include <stdio.h>
#include <stdarg.h>

#ifndef BOOTLOADER

int trace_printf(const char* format, ...){
  int ret;
  va_list ap;

  va_start (ap, format);

  static char buf[TRACE_BUF_SIZE];
  memset( buf, 0, sizeof(buf) );

  // Print to the local buffer
  ret = vsnprintf (buf, sizeof(buf), format, ap);
  if (ret > 0)
    {
      
      #ifdef ENABLE_COPROCESSOR
      int len = strnlen( buf, sizeof(buf) );
        
        // strip EOL characters
      if( ( buf[len - 1] == '\r' ) ||
          ( buf[len - 1] == '\n' ) ){

          buf[len - 1] = 0;
      } 

      if( ( buf[len - 2] == '\r' ) ||
          ( buf[len - 2] == '\n' ) ){

          buf[len - 2] = 0;
      } 

      ret = coproc_i32_debug_print( buf );
      #else
      // Transfer the buffer to the device
      ret = os_printf("%s", buf);
      #endif

    }

  va_end (ap);
  return ret;
}
#endif
