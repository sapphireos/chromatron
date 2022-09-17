# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
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
import ifaddr
import ipaddress

def get_local_addresses():
    addrs = []    

    adapters = ifaddr.get_adapters()

    for adapter in adapters:
        for ip in adapter.ips:
            try:
                host = ipaddress.IPv4Address(ip.ip)

                addrs.append(str(host))

            except ipaddress.AddressValueError:
                pass

    return addrs

def get_broadcast_addresses():
    addrs = []    

    adapters = ifaddr.get_adapters()

    for adapter in adapters:
        for ip in adapter.ips:
            try:
                host = ipaddress.IPv4Address(ip.ip)
                net = ipaddress.IPv4Network(ip.ip + '/' + str(ip.network_prefix), False)

                addrs.append(str(net.broadcast_address))

            except ipaddress.AddressValueError:
                pass

    return addrs


def send_udp_broadcast(sock, port, data):
    try:
        for addr in get_broadcast_addresses():            
            sock.sendto(data, (addr, port))

    except Exception:
        raise
        sock.sendto(data, ('<broadcast>', port))



