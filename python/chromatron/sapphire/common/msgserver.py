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

import time
import struct
import socket
import logging
from .broadcast import get_broadcast_addresses, get_local_addresses
from .util import synchronized
from .ribbon import Ribbon, run_all, stop_all
import select

class UnknownMessage(Exception):
    pass

class InvalidMessage(Exception):
    pass

class InvalidVersion(Exception):
    pass


class _Timer(Ribbon):
    def __init__(self, interval, port, repeat=True):
        super().__init__(name='timer')

        self._timer_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._timer_sock.bind(('localhost', 0))
        self.interval = interval    
        self.port = self._timer_sock.getsockname()[1]
        self.dest_port = port
        self.repeat = repeat

        self.start()

    def _process(self):
        time.sleep(self.interval)
        self._timer_sock.sendto(''.encode(), ('localhost', self.dest_port))

        if not self.repeat:
            self.stop()

class MsgServer(Ribbon):
    def __init__(self, name='msg_server', port=0, listener_port=None, listener_mcast=None, ignore_unknown=True):
        super().__init__(name)

        self._port = port
        self._listener_port = listener_port
        self._listener_mcast = listener_mcast
        self._listener_sock = None

        self._messages = {}
        self._handlers = {}
        self._msg_type_offset = None
        self._protocol_version = None
        self._protocol_version_offset = None
        self._timers = {}

        self.ignore_unknown = ignore_unknown

        self._timer_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._timer_sock.bind(('0.0.0.0', 0))
        self._timer_port = self._timer_sock.getsockname()[1]

        self.__server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        if self._port is None:
            self.__server_sock.bind(('0.0.0.0', 0))
            
        else:
            self.__server_sock.bind(('0.0.0.0', self._port))

        self._port = self.__server_sock.getsockname()[1]

        self._inputs = [self.__server_sock, self._timer_sock]
        
        if self._listener_port is not None:
            self._listener_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._listener_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            self._listener_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

            try:
                # this option may fail on some platforms
                self._listener_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

            except AttributeError:
                pass

            self._listener_sock.bind(('0.0.0.0', self._listener_port))

            logging.info(f"MsgServer {self.name}: server on {self._port} listening on {self._listener_port}")

            logging.info(f'MsgServer {self.name}: joining multicast group {self._listener_mcast}')
            mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
            self._listener_sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

            self._inputs.append(self._listener_sock)

        else:  
            logging.info(f"MsgServer {self.name}: server on {self._port}")

        self.start()

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

    def _process(self):
        try:
            readable, writable, exceptional = select.select(self._inputs, [], [], 1.0)

            with self._lock:
                for s in readable:
                    try:
                        data, host = s.recvfrom(1024)

                        # process data
                        if (s == self.__server_sock) or (s == self._listener_sock):
                            # filter out messages from our own ports
                            if (s == self.__server_sock) and (host[1] == self._listener_port):
                                continue

                            elif (s == self._listener_sock) and (host[1] == self._port):
                                continue

                            msg = self._deserialize(data)
                            response = None                    
        
                            response, host = self._process_msg(msg, host)
                            
                            if response:
                                self.transmit(response, host)

                        # run timers
                        elif host[1] in self._timers:    
                            self._timers[host[1]]()

                    except UnknownMessage as e:
                        raise

                    except Exception as e:
                        logging.exception(e)


        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)
        
    @synchronized
    def start_timer(self, interval, handler, repeat=True):
        timer = _Timer(interval, self._timer_port)
        self._timers[timer.port] = handler
    
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
    
    @synchronized
    def transmit(self, msg, host):
        try:
            if host[0] == '<broadcast>':
                data = msg.pack()
                port = host[1]
                    
                # are we multicasting:
                if self._listener_mcast:
                    # send to multicast address
                    self._listener_sock.sendto(msg.pack(), (self._listener_mcast, port))
    
                else:
                    # no, this is a normal broadcast.
                    # transmit on all IP interfaces
                    for addr in get_broadcast_addresses():
                        self.__server_sock.sendto(data, (addr, port))

            else:
                self.__server_sock.sendto(msg.pack(), host)

        except socket.error:
            pass

    def _process_msg(self, msg, host):     
        # check if receiving a message we sent
        # this can happen in multicast groups
        if (host[0] in get_local_addresses()) and (host[1] == self._port):
            return
   
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

    @synchronized
    def stop(self):
        super()._shutdown()
        
        logging.info(f"MsgServer {self.name}: shutting down")
        self.clean_up()

        # leave multicast group
        if self._listener_port is not None and self._listener_mcast is not None:
            logging.info(f'MsgServer {self.name}: leaving multicast group {self._listener_mcast}')
            mreq = struct.pack("4sl", socket.inet_aton(self._listener_mcast), socket.INADDR_ANY)
            try:
                self._listener_sock.setsockopt(socket.IPPROTO_IP, socket.IP_DROP_MEMBERSHIP, mreq)

            except OSError as e:
                logging.exception(e)
                # can't find an explanation for why the socket occasionally gives this error,
                # but we're shutting down anyway.
                pass

        logging.info(f"MsgServer {self.name}: shut down complete")

    def clean_up(self):
        pass

