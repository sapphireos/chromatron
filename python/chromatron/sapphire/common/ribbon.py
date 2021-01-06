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
import threading
import logging
import inspect
from queue import Queue, Empty

class MsgQueueEmptyException(Exception):
    pass

class PositionalArgumentsNotSupported(Exception):
    pass


class Ribbon(threading.Thread):

    __lock = threading.Lock()
    __ribbons = []
    __ribbon_id = 0

    @classmethod
    def shutdown(cls, timeout=10.0):
        logging.info("Ribbon: shutting down all ribbons")

        with cls.__lock:
            for r in cls.__ribbons:
                r.stop()

        for i in range(int(timeout)):
            alive = False

            with cls.__lock:
                for r in cls.__ribbons:
                    if r.is_alive():
                        alive = True
                        break

            if alive:
                time.sleep(1.0)

            else:
                break

        with cls.__lock:
            for r in cls.__ribbons:
                if r.is_alive():
                    logging.error("Ribbon: %s failed to shut down" % (r.name))

    @classmethod
    def count(cls):
        return len(cls.__ribbons)

    def __init__(self,
                 name=None,
                 initialize_func=None,
                 clean_up_func=None,
                 loop_func=None,
                 auto_start=True,
                 queue=None,
                 **kwargs):
        super(Ribbon, self).__init__()

        if name:
            self.name = name

        else:
            try:
                self.name = self.NAME
                
            except AttributeError:
                pass

        self._stop_event = threading.Event()

        if initialize_func:
            self.initialize = initialize_func

        if clean_up_func:
            self.clean_up = clean_up_func

        if loop_func:
            self.loop = loop_func

        if queue:
            self.queue = queue

        else:
            self.queue = Queue()

        self.daemon = True

        with self.__lock:
            self.__ribbons.append(self)
            self.ribbon_id = self.__ribbon_id
            self.__ribbon_id += 1

        self._kwargs = kwargs

        try:
            # build dict of default kwargs for method
            argspec = inspect.getargspec(self.initialize)

            method_args = argspec.args[1:]
            defaults = argspec.defaults
            method_kwargs = {}
            for i in range(len(method_args)):
                try:
                    method_kwargs[method_args[i]] = defaults[i]

                except IndexError:
                    # this occurs if we pass a non-keyword argument
                    raise PositionalArgumentsNotSupported

            # now, override method kwargs
            for k, v in self._kwargs.items():
                if k in method_kwargs:
                    method_kwargs[k] = v

            self._initialize(**method_kwargs)

            if auto_start:
                self.start()

        except Exception as e:
            logging.exception("Ribbon: %s failed to initialize with: %s" % (self.name, e))
            raise

    def start(self):
        super(Ribbon, self).start()

    def _initialize(self, **kwargs):
        self.initialize(**kwargs)

    def initialize(self, **kwargs):
        pass

    def clean_up(self):
        pass

    def _loop(self):
        self.loop()

    def loop(self):
        time.sleep(1.0)

    def run(self):
        # append ribbon ID to name
        self.name += '.%d' % (self.ribbon_id)

        logging.info("Ribbon: %s started" % (self.name))

        while not self._stop_event.is_set():
            try:
                self._loop()

            except Exception as e:
                logging.exception("Ribbon: %s unexpected exception: %s" % (self.name, e))
                time.sleep(2.0)

        try:
            self.clean_up()

        except Exception as e:
            logging.exception("Ribbon: %s failed to clean up with: %s" % (self.name, e))

        with self.__lock:
            self.__ribbons.remove(self)

        logging.info("Ribbon: %s stopped" % (self.name))

    def wait(self, timeout):
        self._stop_event.wait(timeout)

    def join(self, timeout=10.0):
        super(Ribbon, self).join(timeout)

        if self.is_alive():
            logging.info("Ribbon: %s join timed out" % (self.name))

    def stop(self):
        if not self._stop_event.is_set():
            logging.info("Ribbon: %s shutting down" % (self.name))

            self._stop_event.set()

    def post_msg(self, msg):
        self.queue.put(msg)

    def recv_msg(self, timeout=None):
        if timeout == None:
            q_timeout = 4.0
        else:
            q_timeout = timeout

        while not self._stop_event.is_set():
            try:
                return self.queue.get(timeout=q_timeout)

            except Empty:
                if timeout:
                    raise MsgQueueEmptyException

    def recv_all_msgs(self, timeout=None):
        if timeout == None:
            q_timeout = 4.0
        else:
            q_timeout = timeout

        msgs = []

        while not self._stop_event.is_set():
            try:
                msgs = [self.queue.get(timeout=q_timeout)]
                break

            except Empty:
                if timeout != None:
                    raise MsgQueueEmptyException

        qsize = self.queue.qsize()

        for i in range(qsize):
            msgs.append(self.queue.get())

        return msgs



class UnknownMessage(Exception):
    pass

class InvalidMessage(Exception):
    pass

class InvalidVersion(Exception):
    pass


import socket
import select
from .broadcast import send_udp_broadcast

class _Timer(threading.Thread):
    def __init__(self, interval, port):
        super().__init__()

        self._timer_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._timer_sock.bind(('0.0.0.0', 0))
        self.interval = interval    
        self.port = self._timer_sock.getsockname()[1]
        self.dest_port = port

        self.daemon = True
        self.start()

    def run(self):
        while True:
            time.sleep(self.interval)
            self._timer_sock.sendto('test'.encode(), ('127.0.0.1', self.dest_port))

class RibbonServer(Ribbon):
    def __init__(self, *args, **kwargs):

        auto_start = True
        if 'auto_start' in kwargs:
            auto_start = kwargs['auto_start']

        kwargs['auto_start'] = False
        super().__init__(*args, **kwargs)
        del kwargs['auto_start']
        
        self._timer_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._timer_sock.bind(('0.0.0.0', 0))
        self._timer_port = self._timer_sock.getsockname()[1]

        self._messages = {}
        self._handlers = {}
        self._msg_type_offset = None
        self._protocol_version = None
        self._protocol_version_offset = None
        self._timers = {}
        self._port = None

        self.initialize(**kwargs)

        self.__server_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        if self._port is None:
            try:
                port = self.PORT

            except AttributeError:
                pass

        if self._port is None:
            self.__server_sock.bind(('0.0.0.0', 0))
            
        else:
            self.__server_sock.bind(('0.0.0.0', self._port))

        self._port = self.__server_sock.getsockname()[1]

        self.__server_sock.setblocking(0)

        self._inputs = [self.__server_sock, self._timer_sock]

        if auto_start:
            self.start()

    def initialize(self, port=None):
        self._port = port

    def _initialize(self, **kwargs):
        pass

    def start_timer(self, interval, handler):
        timer = _Timer(interval, self._timer_port)
        self._timers[timer.port] = handler

    def default_handler(self, msg, host):
        logging.debug(f"Unhandled message: {type(msg)} from {host}")        

    def register_message(self, msg, handler=None):
        if handler is None:
            handler = self.default_handler

        header = msg().header

        if self._msg_type_offset is None:
            self._msg_type_offset = header.offset('type')

        if self._protocol_version is None:
            try:
                self._protocol_version_offset = header.offset('version')
                self._protocol_version = header.version

            except KeyError:
                pass

        msg_type = header.type
        self._messages[msg_type] = msg
        self._handlers[msg] = handler

    def _deserialize(self, buf):
        msg_id = int(buf[self._msg_type_offset])

        try:
            return self._messages[msg_id]().unpack(buf)

        except KeyError:
            raise UnknownMessage(msg_id)

        except (struct.error, UnicodeDecodeError) as e:
            raise InvalidMessage(msg_id, len(buf), e)

        
    def transmit(self, msg, host):
        s = self.__server_sock

        try:
            if host[0] == '<broadcast>':
                send_udp_broadcast(s, host[1], msg.pack())

            else:
                s.sendto(msg.pack(), host)

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

    def loop(self):
        pass

    def _loop(self):
        try:
            readable, writable, exceptional = select.select(self._inputs, [], [], 1.0)

            for s in readable:
                try:
                    data, host = s.recvfrom(1024)

                    if s == self.__server_sock:
                        msg = self._deserialize(data)
                        response = None                    

                        response, host = self._process_msg(msg, host)
                        
                        if response:
                            self.transmit(response, host)

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

        self.loop()

def wait_for_signal():
    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

