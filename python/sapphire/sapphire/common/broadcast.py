# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2019  Jeremy Billheimer
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

import socket

try:
    import netifaces

except ImportError:
    print("Skipping import for netifaces")

def get_broadcast_addresses():
    addrs = []
    for interface in netifaces.interfaces():
        try:
            for addr in netifaces.ifaddresses(interface)[socket.AF_INET]:
                try:
                    addrs.append(addr['broadcast'])

                except KeyError:
                    pass

        except KeyError:
            pass


    return addrs


def send_udp_broadcast(sock, port, data):
    try:
        for addr in get_broadcast_addresses():
            sock.sendto(data, (addr, port))

    except NameError:
        sock.sendto(data, ('<broadcast>', port))



