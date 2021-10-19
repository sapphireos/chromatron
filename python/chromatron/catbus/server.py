# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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
from .client import Client
from sapphire.common.broadcast import send_udp_broadcast

from .messages import *
from .catbustypes import *

from sapphire.common import catbus_string_hash, util, MsgServer, synchronized



class Server(MsgServer):
    def __init__(self, data_port=0, database=None, visible=True):
        super().__init__(name=f"{database[META_TAG_NAME]}.{'server'}", port=data_port, listener_port=CATBUS_ANNOUNCE_PORT, listener_mcast=CATBUS_ANNOUNCE_MCAST_ADDR)
        self._database = database

        self.visible = visible

        logging.info(f"Data server {self.name} on port {self._port}")

        self._hash_lookup = {}

        self._database.add_item('uptime', 0, 'uint32', readonly=True)

        self.register_message(ErrorMsg, self._handle_error)
        self.register_message(DiscoverMsg, self._handle_discover)
        self.register_message(AnnounceMsg, self._handle_announce)
        self.register_message(LookupHashMsg, self._handle_lookup_hash)
        self.register_message(ResolvedHashMsg, self._handle_resolved_hash)
        self.register_message(GetKeyMetaMsg, self._handle_get_key_meta)
        self.register_message(GetKeysMsg, self._handle_get_keys)
        self.register_message(SetKeysMsg, self._handle_set_keys)
        self.register_message(ShutdownMsg, self._handle_shutdown)

        self.start_timer(4.0, self._process_announce)

        self._default_callback = None

        # if server is visible, send announce immediately on startup
        if self.visible:
            self.start_timer(0.1, self._send_announce, repeat=False)

        self._announce_handlers = []
        self._shutdown_handlers = []

        self.start()

    @synchronized
    def register_announce_handler(self, handler):
        self._announce_handlers.append(handler)

    @synchronized
    def register_shutdown_handler(self, handler):
        self._shutdown_handlers.append(handler)

    def clean_up(self):
        self._send_shutdown()
        time.sleep(0.1)
        self._send_shutdown()
        time.sleep(0.1)
        self._send_shutdown()
        time.sleep(0.1)
        self._send_shutdown()
        time.sleep(0.1)
        self._send_shutdown()

    @synchronized
    def resolve_hash(self, hashed_key, host=None):
        if hashed_key == 0:
            return ''
    
        try:
            return self._hash_lookup[hashed_key]

        except KeyError:
            if host:
                c = Client(host)

                key = c.lookup_hash(hashed_key)[hashed_key]

                if len(key) == 0:
                    raise KeyError(hashed_key)

                self._hash_lookup[hashed_key] = key

                return key

            else:
                raise

    def _send_announce(self, host=(CATBUS_ANNOUNCE_MCAST_ADDR, CATBUS_ANNOUNCE_PORT), discovery_id=None):
        msg = AnnounceMsg(
                data_port=self._port,
                query=self._database.get_query())

        if discovery_id:
            msg.header.transaction_id = discovery_id

        self.transmit(msg, host)

    def _send_shutdown(self, host=None):
        msg = ShutdownMsg()

        if host is not None:
            self.transmit(msg, host)

        else:
            # send shutdowns to broadcast main port, and multicast announce port

            broadcast_addr = ('<broadcast>', CATBUS_MAIN_PORT)
            mcast_addr = (CATBUS_ANNOUNCE_MCAST_ADDR, CATBUS_ANNOUNCE_PORT)

            self.transmit(msg, broadcast_addr)
            self.transmit(msg, mcast_addr)


    def _handle_error(self, msg, host):
        if msg.error_code != CATBUS_ERROR_UNKNOWN_MSG:
            logging.error(msg, host)

    def _handle_discover(self, msg, host):
        if self.visible:
            if (self._database.query(*msg.query) or (msg.flags & CATBUS_DISC_FLAG_QUERY_ALL)):
                self._send_announce(host=host, discovery_id=msg.header.transaction_id)

    def _handle_announce(self, msg, host):
        for handler in self._announce_handlers:
            handler(msg, host)

    def _handle_lookup_hash(self, msg, host):
        resolved_hashes = []
        for requested_hash in msg.hashes:
            if requested_hash in self._database:
                resolved_hashes.append(CatbusStringField(self._database.lookup_hash(requested_hash)))

            # resolve tag name
            elif requested_hash in self._database.get_tag_info():
                resolved_hashes.append(CatbusStringField(self._database.get_tag_info()[requested_hash]))

        reply_msg = ResolvedHashMsg(keys=resolved_hashes)
        reply_msg.header.transaction_id = msg.header.transaction_id
        
        return reply_msg, host

    def _handle_resolved_hash(self, msg, host):
        pass
    
    def _handle_get_key_meta(self, msg, host):
        keys = sorted(self._database.hashes())
        
        key_count = len(keys)
        index = msg.page * CATBUS_MAX_KEY_META
        page_count = (key_count / CATBUS_MAX_KEY_META ) + 1
        item_count = key_count - index

        if item_count < 0:
            raise GenericProtocolException

        if item_count > CATBUS_MAX_KEY_META:
            item_count = CATBUS_MAX_KEY_META
    
        meta = []
        for i in range(item_count):
            key = keys[i + index]
            item = self._database.get_item(key)
            meta.append(item.meta)

        reply_msg = KeyMetaMsg(page=msg.page, page_count=page_count, item_count=item_count, meta=meta)
        reply_msg.header.transaction_id = msg.header.transaction_id

        return reply_msg, host

    def _handle_get_keys(self, msg, host):
        if msg.count == 0:
            raise GenericProtocolException

        items = []

        for hashed_key in msg.hashes:
            try:
                items.append(self._database.get_item(hashed_key))

            except KeyError:
                logging.debug(f"Key {hashed_key} not found in database")

        reply_msg = KeyDataMsg(data=items)
        reply_msg.header.transaction_id = msg.header.transaction_id

        return reply_msg, host

    def _handle_set_keys(self, msg, host):
        reply_items = []

        for item in msg.data:
            try:
                db_item = self._database.get_item(item.meta.hash)

            except KeyError:
                raise GenericProtocolException

            # check types
            if db_item.meta.type != item.meta.type:
                raise GenericProtocolException

            # check read only flag
            if db_item.meta.flags & CATBUS_FLAGS_READ_ONLY:
                raise GenericProtocolException

            # set data
            self._database[item.meta.hash] = item.value

            # retrieve updated copy of item
            reply_items.append(self._database.get_item(item.meta.hash))

        reply_msg = KeyDataMsg(data=reply_items)
        reply_msg.header.transaction_id = msg.header.transaction_id

        return reply_msg, host

    def _handle_shutdown(self, msg, host):
        for handler in self._shutdown_handlers:
            handler(msg, host)
        
    def _process_announce(self):
        self._database['uptime'] += 4

        if self.visible:
            self._send_announce()

