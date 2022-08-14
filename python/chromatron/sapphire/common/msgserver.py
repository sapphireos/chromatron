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

import time
import struct
import socket
import logging
from .broadcast import get_broadcast_addresses, get_local_addresses
from .util import synchronized
from .ribbon import Ribbon, run_all, stop_all
import select
import random
import string


class UnknownMessage(Exception):
    pass

class InvalidMessage(Exception):
    pass

class InvalidVersion(Exception):
    pass


class _Timer(Ribbon):
    def __init__(self, name, interval, port, handler, repeat=True):
        super().__init__(name=name, suppress_logs=True)

        self._timer_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._timer_sock.bind(('localhost', 0))
        self.interval = interval    
        self.port = self._timer_sock.getsockname()[1]
        self.dest_port = port
        self.handler = handler
        self.repeat = repeat
        code = ''.join(random.choices(string.ascii_uppercase + string.digits, k=32))
        self.check_code = code.encode()

        self.start()

    def _process(self):
        self.wait(self.interval)
        self._timer_sock.sendto(self.check_code, ('localhost', self.dest_port))

        if not self.repeat:
            self.stop()


class BaseServer(Ribbon):
    def __init__(self, name='base_server', port=0, listener_port=None, listener_mcast=None, mode='udp'):
        super().__init__(name, suppress_logs=True)

        self._port = port
        self._listener_port = listener_port
        self._listener_mcast = listener_mcast
        self._listener_sock = None

        self._messages = {}
        self._handlers = {}
        self._msg_type_offset = None
        self._protocol_version = None
        self._protocol_version_offset = None
        self._protocol_magic = None
        self._protocol_magic_offset = None
        
        self._timers = {}

        self._timer_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._timer_sock.bind(('localhost', 0))
        self._timer_sock.setblocking(0)
        self._timer_port = self._timer_sock.getsockname()[1]

        self.mode = mode

        if mode == 'udp':
            self.__server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            # self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

            # try:
            #     # this option may fail on some platforms
            #     self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

            # except AttributeError:
            #     pass

        elif mode == 'tcp':
            self.__server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        else:
            assert False

        if self._port is None:
            self.__server_sock.bind(('0.0.0.0', 0))
            
        else:
            self.__server_sock.bind(('0.0.0.0', self._port))

        self.__server_sock.setblocking(0)
        self._port = self.__server_sock.getsockname()[1]

        self._inputs = [self.__server_sock, self._timer_sock]
        
        if self._listener_port is not None:
            self._listener_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._listener_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self._listener_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self._listener_sock.setblocking(0)

            # try:
            #     # this option may fail on some platforms
            #     self._listener_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

            # except AttributeError:
            #     pass

            self._listener_sock.bind(('0.0.0.0', self._listener_port))

            logging.debug(f"{self.name}: server on {self._port} listening on {self._listener_port}")

            if self._listener_mcast is not None:
                logging.debug(f'{self.name}: joining multicast group {self._listener_mcast}')
                mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
                self._listener_sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

            self._inputs.append(self._listener_sock)

        else:  
            logging.debug(f"{self.name}: server on {self._port}")

    @property
    @synchronized
    def port(self):
        return self._port

    @property
    @synchronized
    def listen_port(self):
        return self._listener_port
    
    def __str__(self):
        if self._listener_port is None:
            return f'{self.name} @ {self._port}'
        else:
            return f'{self.name} @ {self._port} & {self._listener_port}'

    def received_packet(self, data, host):
        pass

    @synchronized
    def send_packet(self, data, host):
        try:
            if host[0] == '<broadcast>':
                port = host[1]
                    
                # are we multicasting:
                if self._listener_mcast:
                    # send to multicast address
                    self.__server_sock.sendto(data, (self._listener_mcast, port))
    
                else:
                    # no, this is a normal broadcast.
                    # transmit on all IP interfaces
                    for addr in get_broadcast_addresses():
                        self.__server_sock.sendto(data, (addr, port))

            else:
                self.__server_sock.sendto(data, host)

        except socket.error:
            pass

    def _process(self, timeout=1.0):
        try:
            readable, writable, exceptional = select.select(self._inputs, [], [], timeout)

            with self._lock:
                for s in readable:
                    try:
                        data, host = s.recvfrom(4096)

                        # process data
                        if (s == self.__server_sock) or (s == self._listener_sock):
                            self.received_packet(data, host)
                            
                        # run timers
                        elif host[1] in self._timers:    
                            timer = self._timers[host[1]]

                            if data == timer.check_code: 
                                timer.handler()

                            else:
                                raise ValueError("Invalid check code in timer!")

                    except Exception as e:
                        logging.exception(e)


        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)
        
    @synchronized
    def start_timer(self, interval, handler, repeat=True):
        timer = _Timer(f'{self.name}.timer', interval, self._timer_port, handler, repeat)
        self._timers[timer.port] = timer
    
    @synchronized
    def stop(self):
        super()._shutdown()
        
        logging.debug(f"{self.name}: shutting down")
        
        # shut down timers
        for timer in self._timers.values():
            timer.stop()

        self.clean_up()

        # leave multicast group
        if self._listener_port is not None and self._listener_mcast is not None:
            logging.debug(f'{self.name}: leaving multicast group {self._listener_mcast}')
            mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
            try:
                self._listener_sock.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, mreq)

            except OSError as e:
                # can't find an explanation for why the socket occasionally gives this error,
                # but we're shutting down anyway.
                pass

        logging.debug(f"{self.name}: shut down complete")

    def clean_up(self):
        pass


class MsgServer(BaseServer):
    def __init__(self, **kwargs):
        self._messages = {}
        self._handlers = {}
        self._msg_type_offset = None
        self._protocol_version = None
        self._protocol_version_offset = None
        self._protocol_magic = None
        self._protocol_magic_offset = None

        super().__init__(**kwargs)

    def received_packet(self, data, host):
        msg = self._deserialize(data)

        response = None                    

        response, host = self._process_msg(msg, host)
        
        if response:
            self.transmit(response, host)

    def _deserialize(self, buf):
        try:
            msg_id = int(buf[self._msg_type_offset])

        except TypeError:
            raise UnknownMessage("No messages defined")

        try:
            return self._messages[msg_id]().unpack(buf)

        except KeyError:
            raise UnknownMessage(msg_id)

        except (struct.error, UnicodeDecodeError) as e:
            raise InvalidMessage(msg_id, len(buf), e)

        # check protocol version and magic        
        protocol_version = int(buf[self._protocol_version_offset])
        protocol_magic = int(buf[self._protocol_magic_offset])

        if protocol_version != self._protocol_version:
            raise UnknownMessage(f'Incorrect protocol version: {protocol_version}')

        elif protocol_magic != self._protocol_magic:
            raise UnknownMessage(f'Incorrect protocol magic: {protocol_magic}')
    
    def _process_msg(self, msg, host):     
        # check if receiving a message we sent
        # this can happen in multicast groups
        if (host[1] in [self._port, self._listener_port]) and (host[0] in get_local_addresses()):
            return None, None

        # run handler
        handler = self._handlers[type(msg)]
        tokens = handler(msg, host)

        # normally, you'd just try to access the tuple and
        # handle the exception. However, that will raise a TypeError, 
        # and if we handle a TypeError here, then any TypeError generated
        # in the message handler will essentially get eaten.
        if not isinstance(tokens, tuple):
            return None, None

        if len(tokens) < 2:
            return None, None

        return tokens[0], tokens[1]
    
    def default_handler(self, msg, host):
        logging.debug(f"Unhandled message: {type(msg)} from {host}")        

    @synchronized
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

        if self._protocol_magic is None:
            try:
                self._protocol_magic_offset = header.offsetof('magic')
                self._protocol_magic = header.magic

            except KeyError:
                pass

        try:
            msg_type = header.type

        except KeyError:
            msg_type = header.msg_type

        self._messages[msg_type] = msg
        self._handlers[msg] = handler

    @synchronized
    def transmit(self, msg, host):
        self.send_packet(msg.pack(), host)
