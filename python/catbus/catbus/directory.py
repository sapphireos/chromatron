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


import logging
import time
import threading
import select
from client import Client
from copy import copy

from messages import *

from sapphire.common import Ribbon, MsgQueueEmptyException


class Directory(Ribbon):
    def initialize(self):
        self.name = '%s' % ('catbus_directory')

        self.__announce_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__announce_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__announce_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__announce_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self.__announce_sock.setblocking(0)
        self.__announce_sock.bind(('', CATBUS_DISCOVERY_PORT))

        self._inputs = [self.__announce_sock]

        self._hash_lookup = {}
        self._directory = {}

        self._msg_handlers = {
            ErrorMsg: self._handle_error,
            AnnounceMsg: self._handle_announce,
        }

        self._last_ttl = time.time()

        self.__lock = threading.Lock()

    def get_directory(self):
        with self.__lock:
            return copy(self._directory)
   
    def resolve_hash(self, hashed_key, host=None):
        if hashed_key == 0:
            return ''

        with self.__lock:
            try:
                return self._hash_lookup[hashed_key]

            except KeyError:
                if host:
                    c = Client()
                    c.connect(host)

                    key = c.lookup_hash(hashed_key)[hashed_key]

                    if len(key) == 0:
                        raise KeyError(hashed_key)

                    self._hash_lookup[hashed_key] = key

                    return key

                else:
                    raise

    def _handle_error(self, msg, host):
        print msg

    def _handle_announce(self, msg, host):
        resolved_query = [self.resolve_hash(a, host=host) for a in msg.query if a != 0]

        info = {'host': host,
                'query': resolved_query,
                'data_port': msg.data_port,
                'version': msg.header.version,
                'universe': msg.header.universe,
                'ttl': 30}

        with self.__lock:
            self._directory[msg.header.origin_id] = info

    def _process_msg(self, msg, host):
        response = self._msg_handlers[type(msg)](msg, host)

        return response

    def loop(self):
        try:
            readable, writable, exceptional = select.select(self._inputs, [], [], 1.0)

            for s in readable:
                try:
                    data, host = s.recvfrom(1024)

                    msg = deserialize(data)

                    self._process_msg(msg, host)

                except UnknownMessageException as e:
                    pass

                except Exception as e:
                    logging.exception(e)

        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)


        if time.time() - self._last_ttl > 4.0:
            self._last_ttl = time.time()

            with self.__lock:
                for key, info in self._directory.iteritems():
                    info['ttl'] -= 4.0

                self._directory = {k: v for k, v in self._directory.iteritems() if v['ttl'] >= 0.0}


if __name__ == '__main__':

    from pprint import pprint

    d = Directory()

    try:
        while True:
            time.sleep(1.0)

            pprint(d.get_directory())
            print len(d.get_directory())

    except KeyboardInterrupt:
        pass


