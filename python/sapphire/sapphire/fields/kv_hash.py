# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>

import crcmod
import struct
from sapphire.common import util

crc_func = crcmod.predefined.mkCrcFun('crc-aug-ccitt')

@util.memoize
def kv_string_hash(s):
    crc0 = crc_func(s)
    crc1 = crc_func(''.join(reversed([chr(ord(a) ^ 0x15) for a in s])))

    h = (crc0 << 16) | crc1

    # convert to signed 32 bit int
    return struct.unpack('<l', struct.pack('<L', h))[0]
