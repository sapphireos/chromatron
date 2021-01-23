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
import logging
from .broadcast import get_broadcast_addresses

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
        self._transport = None

        self._messages = {}
        self._handlers = {}
        self._msg_type_offset = None
        self._protocol_version = None
        self._protocol_version_offset = None
        self._timers = {}


        self._loop.run_until_complete(self._loop.create_datagram_endpoint(lambda: self, local_addr=('0.0.0.0', self._port), reuse_port=False, allow_broadcast=True))

        if self._listener_port is not None:
            self._loop.run_until_complete(self._loop.create_datagram_endpoint(lambda: self, local_addr=('0.0.0.0', self._listener_port), reuse_port=True, allow_broadcast=True))
            logging.info(f"MsgServer {self.name}: server on {self._port} listening on {self._listener_port}")

        else:  
            logging.info(f"MsgServer {self.name}: server on {self._port}")


        self._servers.append(self)

    def __str__(self):
        if self._listener_port is None:
            return f'{self.name} @ {self._port}'
        else:
            return f'{self.name} @ {self._port} & {self._listener_port}'

    def start_timer(self, interval, handler):
        def _process_timer(loop, interval, handler):
            handler()
            loop.call_later(interval, _process_timer, loop, interval, handler)

        self._loop.call_later(interval, _process_timer, self._loop, interval, handler)
        
    def default_handler(self, msg, host):
        logging.debug(f"Unhandled message: {type(msg)} from {host}")        

    def register_message(self, msg, handler=None):    
        if handler is None:
            handler = self.default_handler

        header = msg().header

        if self._msg_type_offset is None:
            try:
                self._msg_type_offset = header.offsetof('type')

            except KeyError:
                self._msg_type_offset = header.offsetof('msg_type')

        if self._protocol_version is None:
            try:
                self._protocol_version_offset = header.offsetof('version')
                self._protocol_version = header.version

            except KeyError:
                pass

        try:
            msg_type = header.type

        except KeyError:
            msg_type = header.msg_type

        self._messages[msg_type] = msg
        self._handlers[msg] = handler

    def _deserialize(self, buf):
        try:
            msg_id = int(buf[self._msg_type_offset])

        except TypeError:
            raise Exception("No messages defined")

        try:
            return self._messages[msg_id]().unpack(buf)

        except KeyError:
            raise UnknownMessage(msg_id)

        except (struct.error, UnicodeDecodeError) as e:
            raise InvalidMessage(msg_id, len(buf), e)
    
    def transmit(self, msg, host):
        try:
            if host[0] == '<broadcast>':
                data = msg.pack()
                port = host[1]
                    
                # are we multicasting:
                if self._listener_mcast:
                    # send to multicast address
                    self._transport.sendto(msg.pack(), (self._listener_mcast, port))
    
                else:
                    # no, this is a normal broadcast.
                    # transmit on all IP interfaces
                    for addr in get_broadcast_addresses():
                        self._transport.sendto(data, (addr, port))

            else:
                self._transport.sendto(msg.pack(), host)

        except socket.error:
            pass

    def _process_msg(self, msg, host):        
        tokens = self._handlers[type(msg)](msg, host)

        # normally, you'd just try to access the tuple and
        # handle the exception. However, that will raise a TypeError, 
        # and if we handle a TypeError here, then any TypeError generated
        # in the message handler will essentially get eaten.
        if not isinstance(tokens, tuple):
            return None, None

        if len(tokens) < 2:
            return None, None

        return tokens[0], tokens[1]


    def connection_made(self, transport):
        sock = transport.get_extra_info('socket')
        port = sock.getsockname()[1]

        if port == self._listener_port:
            self._listener_sock = sock

            if self._listener_mcast is not None:
                mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
                sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

        else:
            self._transport = transport
            self._port = port

    def datagram_received(self, data, addr):
        msg = self._deserialize(data)
        response = None                    

        response, host = self._process_msg(msg, host)
        
        if response:
            self.transmit(response, host)

    def error_received(self, exc):
        logging.error('Error received:', exc)

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
    try:
        loop.run_forever()
    except KeyboardInterrupt:  # pragma: no branch
        pass
    finally:
        stop_all(loop=loop)

        loop.close()

def meow():
    print('meow')

def main():
    from . import util
    util.setup_basic_logging(console=True)

    s = MsgServer(port=0, listener_port=32042, listener_mcast='239.43.96.31')

    # print(s)
    s.start_timer(1.0, meow)

    run_all()


if __name__ == '__main__':
    main()