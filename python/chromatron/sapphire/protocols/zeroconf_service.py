#
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
#

import socket
from zeroconf import IPVersion, ServiceInfo, Zeroconf


class ZeroconfService(object):
    def __init__(self, protocol='_http._tcp.local', name='test', port=80, properties={}):
        self.service_info = ServiceInfo(
            protocol,
            f"{name}.{protocol}",
            addresses=[socket.inet_aton("0.0.0.0")], # bind to all addresses
            port=port,
            properties=properties,
        )

        self.zeroconf = Zeroconf(ip_version=IPVersion.V4Only)
        self.zeroconf.register_service(self.service_info)

    def stop(self):
        self.zeroconf.unregister_service(self.service_info)
        self.zeroconf.close()
