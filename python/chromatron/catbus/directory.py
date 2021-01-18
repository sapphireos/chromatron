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


import logging
import time
import os
import threading
import select
from .client import Client
from copy import copy
import socket
import json
import struct
import sys

from .options import *
from .messages import *
from .data_structures import *

from sapphire.buildtools import firmware_package
LOG_FILE_PATH = os.path.join(firmware_package.data_dir(), 'catbus_directory.log')

from sapphire.common import Ribbon, MsgQueueEmptyException, util


TTL = 240


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
        self.__announce_sock.bind(('', CATBUS_ANNOUNCE_PORT))

        self.__main_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__main_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__main_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__main_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self.__main_sock.setblocking(0)
        self.__main_sock.bind(('', CATBUS_MAIN_PORT))

        self._inputs = [self.__announce_sock, self.__main_sock]

        self._hash_lookup = {}
        self._directory = {}

        self._msg_handlers = {
            ErrorMsg: self._handle_error,
            AnnounceMsg: self._handle_announce,
            ShutdownMsg: self._handle_shutdown,
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

                    key = c.lookup_hash(hashed_key)

                    if len(key) == 0:
                        raise KeyError(hashed_key)

                    self._hash_lookup[hashed_key] = key[hashed_key]

                    return self._hash_lookup[hashed_key]

                else:
                    raise

    def _handle_shutdown(self, msg, host):
        with self.__lock:
            try:
                info = self._directory[msg.header.origin_id]

                logging.info(f"Shutdown: {info['name']:32} @ {info['host']}")

                # remove entry
                del self._directory[msg.header.origin_id]

            except KeyError:
                pass

    def _handle_error(self, msg, host):
        print(msg)

    def _handle_announce(self, msg, host):
        # update host port with advertised data port
        host = (host[0], msg.data_port)

        try:
            resolved_query = [self.resolve_hash(a, host=host) for a in msg.query if a != 0]

        except NoResponseFromHost:
            logging.warn(f"No response from: {host}")

            return

        with self.__lock:

            def update_info(msg, host):
                c = Client()
                c.connect(host)
                name = c.get_key(META_TAG_NAME)
                location = c.get_key(META_TAG_LOC)

                info = {'host': tuple(host),
                        'name': name,
                        'location': location,
                        'hashes': msg.query,
                        'query': resolved_query,
                        'tags': [t for t in msg.query if t != 0],
                        'data_port': msg.data_port,
                        'version': msg.header.version,
                        'universe': msg.header.universe,
                        'ttl': TTL}

                self._directory[msg.header.origin_id] = info

                return info

            try:
                # check if we have this node already
                if msg.header.origin_id not in self._directory:
                    info = update_info(msg, host)

                    logging.info(f"Added   : {info['name']:32} @ {info['host']}")

                else:
                    info = self._directory[msg.header.origin_id]

                    # check if query tags have changed
                    if msg.query != info['hashes']:
                        update_info(msg, host)

                        logging.info(f"Updated   : {info['name']:32} @ {info['host']}")

                    # reset ttl
                    self._directory[msg.header.origin_id]['ttl'] = TTL

            except NoResponseFromHost as e:
                logging.warn(f"No response from: {host}")

                return

    def _process_msg(self, msg, host):
        try:
            response = self._msg_handlers[type(msg)](msg, host)

        except KeyError:
            raise UnknownMessageException

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
                for key, info in self._directory.items():
                    info['ttl'] -= 4.0

                    if info['ttl'] < 0.0:
                        logging.info(f"Timed out: {info['name']:32} @ {info['host']}")

                self._directory = {k: v for k, v in self._directory.items() if v['ttl'] >= 0.0}

class DirectoryServer(Ribbon):
    def initialize(self, directory=None):
        self.name = '%s' % ('directory_server')
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self.sock.bind(('localhost', CATBUS_DIRECTORY_PORT))
        self.sock.settimeout(1.0)
        self.sock.listen(1)

        self.directory = directory
        
    def loop(self):
        try:
            conn, addr = self.sock.accept()

            data = json.dumps(self.directory.get_directory())

            conn.send(struct.pack('<L', len(data)))
            conn.send(data.encode('utf-8'))

            conn.close()

        except socket.timeout:
            pass

    def clean_up(self):
        self.sock.close()

def main():
    try:
        LOG_FILE_PATH = sys.argv[1]
    except IndexError:
        LOG_FILE_PATH = "catbus_directory.log"

    util.setup_basic_logging(console=True, filename=LOG_FILE_PATH)

    d = Directory()
    svr = DirectoryServer(directory=d)
    

    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

if __name__ == '__main__':
    main()