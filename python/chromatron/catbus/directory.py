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

from sapphire.common import util, MsgServer, run_all, synchronous_call, create_task


TTL = 240


class Directory(MsgServer):
    def __init__(self):
        super().__init__(name='catbus_directory', port=0, listener_port=CATBUS_ANNOUNCE_PORT, listener_mcast=CATBUS_ANNOUNCE_MCAST_ADDR)

        self._hash_lookup = {}
        self._directory = {}

        self.register_message(ErrorMsg, self._handle_error)
        self.register_message(AnnounceMsg, self._handle_announce)
        self.register_message(ShutdownMsg, self._handle_shutdown)

        self.start_timer(4.0, self._process_ttl)

    def get_directory(self):    
        return copy(self._directory)
   
    async def resolve_hash(self, hashed_key, host=None):
        if hashed_key == 0:
            return ''

        try:
            return self._hash_lookup[hashed_key]

        except KeyError:
            if host:
                c = Client(host)

                key = await synchronous_call(c.lookup_hash, hashed_key)

                if len(key) == 0:
                    raise KeyError(hashed_key)

                self._hash_lookup[hashed_key] = key[hashed_key]

                return self._hash_lookup[hashed_key]

            else:
                raise

    def _handle_shutdown(self, msg, host):
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

        async def query_device():
            try:
                resolved_query = [await self.resolve_hash(a, host=host) for a in msg.query if a != 0]

            except NoResponseFromHost:
                logging.warn(f"No response from: {host}")

                return

            async def update_info(msg, host):
                c = Client(host)

                name = await synchronous_call(c.get_key, META_TAG_NAME)
                location = await synchronous_call(c.get_key, META_TAG_LOC)

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
                    info = await update_info(msg, host)

                    logging.info(f"Added   : {info['name']:32} @ {info['host']}")

                else:
                    info = self._directory[msg.header.origin_id]

                    # check if query tags have changed
                    if msg.query != info['hashes']:
                        await update_info(msg, host)

                        logging.info(f"Updated   : {info['name']:32} @ {info['host']}")

                    # update data port (in case a service restarted on another port)
                    if msg.data_port != self._directory[msg.header.origin_id]['data_port']:
                        logging.info(f"Changed data port: {info['name']:32} @ {info['host']} to {msg.data_port}")
                        self._directory[msg.header.origin_id]['host'] = host
                        self._directory[msg.header.origin_id]['data_port'] = msg.data_port

                    # reset ttl
                    self._directory[msg.header.origin_id]['ttl'] = TTL

            except NoResponseFromHost as e:
                logging.warn(f"No response from: {host}")

        # put the device query on the event loop to avoid blocking
        create_task(query_device())

    def _process_ttl(self):
        for key, info in self._directory.items():
            info['ttl'] -= 4.0

            if info['ttl'] < 0.0:
                logging.info(f"Timed out: {info['name']:32} @ {info['host']}")

        self._directory = {k: v for k, v in self._directory.items() if v['ttl'] >= 0.0}


import asyncio

class DirectoryServer(object):
    def __init__(self, directory=None):

        self._loop = asyncio.get_event_loop()

        svr_coro = self._loop.create_server(lambda: self, '0.0.0.0', CATBUS_DIRECTORY_PORT)
        self.server = self._loop.run_until_complete(svr_coro)

        logging.info(f"Directory server on TCP {CATBUS_DIRECTORY_PORT}")

        self.directory = directory

    def connection_made(self, transport):
        logging.info(f"Connection from {transport.get_extra_info('peername')}")

        data = json.dumps(self.directory.get_directory())
        transport.write(struct.pack('<L', len(data)))
        transport.write(data.encode('utf-8'))

        transport.close()

    def connection_lost(self, exc):
        pass

    def data_received(self, data):
        pass
        

def main():
    try:
        LOG_FILE_PATH = sys.argv[1]
    except IndexError:
        LOG_FILE_PATH = "catbus_directory.log"

    util.setup_basic_logging(console=True, filename=LOG_FILE_PATH)

    d = Directory()
    svr = DirectoryServer(directory=d)
    
    run_all()


if __name__ == '__main__':
    main()