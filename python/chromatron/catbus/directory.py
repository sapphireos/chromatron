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


import logging
import time
import os
import select
from .client import Client
from copy import copy
import socket
import json
import struct
import sys

from .catbus import CatbusService
from .options import *
from .messages import *
from .data_structures import *

from sapphire.buildtools import firmware_package
LOG_FILE_PATH = os.path.join(firmware_package.data_dir(), 'catbus_directory.log')

from sapphire.common import util, MsgServer, run_all, synchronized, Ribbon

TTL = 240

class Directory(CatbusService):
    def __init__(self):
        super().__init__(name='DIRECTORY')

        self.register_announce_handler(self._handle_announce)
        self.register_shutdown_handler(self._handle_shutdown)

        self._directory = {}
        self._hash_lookup = {}

        self._directory_server = DirectoryServer(directory=self)

        self.add_item('directory_port', self._directory_server._port)

        logging.info(f'DirectoryServer on port {self._directory_server.port}')

        self.start_timer(4.0, self._process_ttl)

    @synchronized
    def get_directory(self):    
        return copy(self._directory)

    @synchronized
    def resolve_hash(self, hashed_key, host=None):
        if hashed_key == 0:
            return ''

        try:
            return self._hash_lookup[hashed_key]

        except KeyError:
            if host:
                with Client(host) as c:                
                    key = c.lookup_hash(hashed_key)

                if len(key) == 0:
                    raise KeyError(hashed_key)

                self._hash_lookup[hashed_key] = key[hashed_key]

                return self._hash_lookup[hashed_key]

            else:
                raise

    def _handle_shutdown(self, msg, host):
        try:
            info = self._directory[str(tuple(host))]

            logging.info(f"Shutdown: {info['name']:32} @ {info['host']}")

            # remove entry
            del self._directory[str(tuple(host))]

        except KeyError:
            pass
  
    def _handle_announce(self, msg, host):
        # update host port with advertised data port
        host = (host[0], msg.data_port)

        def query_device():
            try:
                resolved_query = [self.resolve_hash(a, host=host) for a in msg.query if a != 0]

            except NoResponseFromHost:
                logging.warn(f"No response from: {host}")

                return

            def update_info(msg, host):
                with Client(host) as c:
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
                        'device_id': msg.header.origin_id,
                        'ttl': TTL}

                self._directory[str(tuple(host))] = info

                return info

            try:
                # check if we have this node already
                if str(tuple(host)) not in self._directory:
                    info = update_info(msg, host)

                    logging.info(f"Added   : {info['name']:32} @ {info['host']}")

                else:
                    info = self._directory[str(tuple(host))]

                    # check if query tags have changed
                    if msg.query != info['hashes']:
                        logging.debug(msg.query)
                        logging.debug(info['hashes'])

                        update_info(msg, host)

                        logging.info(f"Updated   : {info['name']:32} @ {info['host']}")

                    # update data port (in case a service restarted on another port)
                    if msg.data_port != self._directory[str(tuple(host))]['data_port']:
                        logging.info(f"Changed data port: {info['name']:32} @ {info['host']} to {msg.data_port}")
                        self._directory[str(tuple(host))]['host'] = host
                        self._directory[str(tuple(host))]['data_port'] = msg.data_port

                    # reset ttl
                    self._directory[str(tuple(host))]['ttl'] = TTL

            except NoResponseFromHost as e:
                logging.warn(f"No response from: {host}")

        query_device()
    
    def _process_ttl(self):
        for key, info in self._directory.items():
            info['ttl'] -= 4.0

            if info['ttl'] < 0.0:
                logging.info(f"Timed out: {info['name']:32} @ {info['host']}")

        self._directory = {k: v for k, v in self._directory.items() if v['ttl'] >= 0.0}    


class DirectoryServer(Ribbon):
    def __init__(self, directory=None):
        super().__init__('directory_server')
        
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self._sock.bind(('0.0.0.0', 0))
        self._sock.settimeout(1.0)
        self._sock.listen(1)

        self._port = self._sock.getsockname()[1]    

        self.directory = directory

        self.start()

    @property
    @synchronized
    def port(self):
        return self._port
        
    def _process(self):
        try:
            conn, addr = self._sock.accept()

            data = json.dumps(self.directory.get_directory())

            conn.send(struct.pack('<L', len(data)))
            conn.send(data.encode('utf-8'))

            conn.close()

        except socket.timeout:
            pass

    def clean_up(self):
        self._sock.close()


def main():
    try:
        LOG_FILE_PATH = sys.argv[1]
    except IndexError:
        LOG_FILE_PATH = "catbus_directory.log"

    util.setup_basic_logging(console=True, filename=LOG_FILE_PATH)

    d = Directory()    
    
    run_all()


if __name__ == '__main__':
    main()