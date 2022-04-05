# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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

ZERO = 0x08
ONE =  0x0C

for i in xrange(256):
    entry = 0

    for b in xrange(8):
        temp = i & (1 << (7 - b))

        if temp == 0:
            entry += (ZERO << ((7 - b) * 4))

        else:
            entry += (ONE << ((7 - b) * 4))

    # entry = ~entry

    print '{0x%x, 0x%x, 0x%x, 0x%x},' % ((entry >> 24) & 0xff, (entry >> 16) & 0xff, (entry >> 8) & 0xff, entry & 0xff)
