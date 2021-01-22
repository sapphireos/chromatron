# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2020  Jeremy Billheimer
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

import asyncio
import struct
import socket

class MsgServer(object):
    _server_id = 0
    _servers = []

    def __init__(self, name='msg_server', port=0, listener_port=None, listener_mcast=None, loop=None):
        self.name = name + f'.{self._server_id}'

        self._server_id += 1

        self._loop = loop
        if self._loop is None:
            self._loop = asyncio.get_event_loop()

        self._port = port
        self._listener_port = listener_port
        self._listener_mcast = listener_mcast
        self._listener_sock = None

        self._servers.append(self)

    def __str__(self):
        if self._listener_port is None:
            return f'{self.name} @ {self._port}'
        else:
            return f'{self.name} @ {self._port} & {self._listener_port}'
        
    def connection_made(self, transport):
        sock = transport.get_extra_info('socket')
        port = sock.getsockname()[1]

        if port == self._listener_port and self._listener_mcast is not None:
            self._listener_sock = sock
            mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    def datagram_received(self, data, addr):
        print("Received:", data, addr)

    def error_received(self, exc):
        print('Error received:', exc)

    async def start(self):
        await self._loop.create_datagram_endpoint(lambda: self, local_addr=('0.0.0.0', self._port), reuse_port=False, allow_broadcast=True)

        if self._listener_port is not None:
            await self._loop.create_datagram_endpoint(lambda: self, local_addr=('0.0.0.0', self._listener_port), reuse_port=True, allow_broadcast=True)

    async def stop(self):
        await self.clean_up()

    async def clean_up(self):
        print("closing")

        if self._listener_port is not None and self._listener_mcast is not None:
            sock = self._listener_sock
            mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, mreq)


        # await asyncio.sleep(4.0)

        print("closed")



def stop_all(loop=asyncio.get_event_loop()):
    for s in MsgServer._servers:
        loop.run_until_complete(s.stop())

def run_all(loop=asyncio.get_event_loop()):
    for s in MsgServer._servers:
        loop.create_task(s.start())

    try:
        loop.run_forever()
    except KeyboardInterrupt:  # pragma: no branch
        pass
    finally:
        stop_all(loop=loop)

        loop.close()


if __name__ == '__main__':
    s = MsgServer(port=0, listener_port=32041, listener_mcast='239.43.96.31')

    print(s)


    run_all()