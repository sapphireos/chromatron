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


#include "trace.h"

#include "system.h"
#include "cmd_usart.h"

#include <stdarg.h>

#ifndef BOOTLOADER

static char buf[TRACE_BUF_SIZE];

int _trace_printf(PGM_P format, ...){
  int ret;
  va_list ap;

  va_start (ap, format);

  memset( buf, 0, sizeof(buf) );

  // Print to the local buffer
  ret = vsnprintf_P (buf, sizeof(buf), format, ap);
  if (ret > 0)
    {
      // Transfer the buffer to the device
      cmd_usart_v_send_data( (uint8_t *)buf, ret );
    }

  va_end (ap);
  return ret;
  // return 0;
}

int trace_printf_ram(const char* format, ...){
  int ret;
  va_list ap;

  va_start (ap, format);

  memset( buf, 0, sizeof(buf) );

  // Print to the local buffer
  ret = vsnprintf (buf, sizeof(buf), format, ap);
  if (ret > 0)
    {
      // Transfer the buffer to the device
      cmd_usart_v_send_data( (uint8_t *)buf, ret );
    }

  va_end (ap);
  return ret;
  // return 0;
}
#endif
