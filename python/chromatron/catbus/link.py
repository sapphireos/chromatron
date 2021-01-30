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

import os
import time
import logging

from elysianfields import *
from .data_structures import *
from .catbustypes import *
from .options import *
from sapphire.common import MsgServer, util, catbus_string_hash, run_all
from sapphire.protocols import services
from .database import *
from .catbus import *

from fnvhash import fnv1a_64


LINK_VERSION            = 1
LINK_MAGIC              = 0x4b4e494c # 'LINK'

LINK_SERVICE            = "link"

LINK_MCAST_ADDR         = "239.43.96.32"

LINK_MIN_TICK_RATE      = 20 / 1000.0
LINK_MAX_TICK_RATE      = 2000 / 1000.0
LINK_RETRANSMIT_RATE    = 2000 / 1000.0

LINK_DISCOVER_RATE      = 4000 / 1000.0

LINK_CONSUMER_TIMEOUT   = 64000 / 1000.0
LINK_PRODUCER_TIMEOUT   = 64000 / 1000.0
LINK_REMOTE_TIMEOUT     = 32000 / 1000.0

LINK_BASE_PRIORITY      = 128



class MsgHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="magic"),
                  Uint8Field(_name="type"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="flags"),
                  Uint8Field(_name="reserved"),
                  Uint64Field(_name="origin_id"),
                  CatbusHash(_name="universe")]

        super().__init__(_fields=fields, **kwargs)

        self.magic          = LINK_MAGIC
        self.version        = LINK_VERSION
        self.flags          = 0 
        self.type           = 0
        self.reserved       = 0
        self.origin_id      = 0
        self.universe       = 0
        
LINK_MSG_TYPE_CONSUMER_QUERY        = 1        
class ConsumerQueryMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  CatbusHash(_name="key"),
                  CatbusQuery(_name="query"),
                  Uint8Field(_name="mode"),
                  Uint64Field(_name="hash")]

        super().__init__(_name="consumer_query", _fields=fields, **kwargs)

        self.header.type = LINK_MSG_TYPE_CONSUMER_QUERY

LINK_MSG_TYPE_CONSUMER_MATCH        = 2
class ConsumerMatchMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint64Field(_name="hash")]

        super().__init__(_name="consumer_match", _fields=fields, **kwargs)

        self.header.type = LINK_MSG_TYPE_CONSUMER_MATCH

        
LINK_MSG_TYPE_PRODUCER_QUERY        = 3        
class ProducerQueryMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  CatbusHash(_name="key"),
                  CatbusQuery(_name="query"),
                  Uint16Field(_name="rate"),
                  Uint64Field(_name="hash")]

        super().__init__(_name="producer_query", _fields=fields, **kwargs)

        self.header.type = LINK_MSG_TYPE_PRODUCER_QUERY

LINK_MSG_TYPE_CONSUMER_DATA         = 10
class ConsumerDataMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint64Field(_name="hash"),
                  CatbusData(_name="data")]

        super().__init__(_name="consumer_data", _fields=fields, **kwargs)

        self.header.type = LINK_MSG_TYPE_CONSUMER_DATA

LINK_MSG_TYPE_PRODUCER_DATA         = 11
class ProducerDataMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint64Field(_name="hash"),
                  CatbusData(_name="data")]

        super().__init__(_name="producer_data", _fields=fields, **kwargs)

        self.header.type = LINK_MSG_TYPE_PRODUCER_DATA

LINK_MODE_SEND  = 0
LINK_MODE_RECV  = 1
LINK_MODE_SYNC  = 2

LINK_AGG_ANY    = 0
LINK_AGG_MIN    = 1
LINK_AGG_MAX    = 2
LINK_AGG_SUM    = 3
LINK_AGG_AVG    = 4

LINK_RATE_MIN   = 20 / 1000.0
LINK_RATE_MAX   = 30000 / 1000.0

class LinkState(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="source_key"),
                  Uint32Field(_name="dest_key"),
                  CatbusQuery(_name="query"),
                  Uint8Field(_name="mode"),
                  Uint8Field(_name="aggregation"),
                  Uint16Field(_name="filter"),
                  Uint16Field(_name="rate")]

        super().__init__(_name="link_state", _fields=fields, **kwargs)


class Link(object):
    def __init__(self,
        mode=LINK_MODE_SEND,
        source_key=None,
        dest_key=None,
        query=[],
        aggregation=LINK_AGG_ANY,
        rate=1000,
        tag=''):

        query.sort(reverse=True)

        self.mode = mode
        self.source_key = source_key
        self.dest_key = dest_key
        self.query = query
        self.aggregation = aggregation
        self.filter = 0
        self.rate = rate
        self.tag = tag

        self._service = None

        self._ticks = self.rate / 1000.0
        self._retransmit_timer = 0
        self._hashed_data = 0

    @property
    def is_leader(self):
        return self._service.is_server

    @property
    def is_follower(self):
        return (not self._service.is_server) and self._service.connected

    @property
    def server(self):
        return self._service.server

    @property
    def query_tags(self):
        query = CatbusQuery()
        query._value = [catbus_string_hash(a) for a in self.query]
        return query

    @property
    def hash(self):
        data = LinkState(
                source_key=catbus_string_hash(self.source_key),
                dest_key=catbus_string_hash(self.dest_key),
                query=self.query_tags,
                mode=self.mode,
                aggregation=self.aggregation,
                filter=self.filter,
                rate=self.rate
                ).pack()

        return fnv1a_64(data)

    def __str__(self):
        if self.mode == LINK_MODE_SEND:
            return f'SEND {self.source_key} to {self.dest_key} at {self.query}'
        elif self.mode == LINK_MODE_RECV:
            return f'RECV {self.source_key} to {self.dest_key} at {self.query}'
        # elif self.mode == LINK_MODE_SYNC:
            # return f'SYNC {self.source_key} to {self.dest_key} at {self.query}'

    def _process_timer(self, elapsed, link_manager):
        self._ticks -= elapsed
        self._retransmit_timer -= elapsed

        if self._ticks > 0 :
            return

        database = link_manager._database

        # update ticks for next iteration
        self._ticks += self.rate / 1000.0

        if self.mode == LINK_MODE_SEND:
            if self.is_leader:
                # AGGREGATION
                print("AGGREGATION")

                # TRANSMIT TO CONSUMER
                print("TX DATA -> CONSUMERS")

                pass

            elif self.is_follower:
                if self.source_key not in database:
                    return

                data = database.get_item(self.source_key)
                hashed_data = catbus_string_hash(data.get_field('value').pack())

                # check if data did not change and the retransmit timer has not expired
                if (hashed_data == self._hashed_data) and \
                   (self._retransmit_timer > 0):
                    return

                self._hashed_data = hashed_data
                self._retransmit_timer = LINK_RETRANSMIT_RATE

                # TRANSMIT PRODUCER DATA
                msg = ProducerDataMsg(hash=self.hash, data=data)

                link_manager.transmit(msg, self.server)

        elif self.mode == LINK_MODE_RECV:
            if self.is_leader:
                # AGGREGATION
                print("AGGREGATION")

                # TRANSMIT TO CONSUMER
                print("TX DATA -> CONSUMERS")


class Producer(object):
    def __init__(self,
        source_key=None,
        link_hash=None,
        leader_ip=None,
        data_hash=None,
        rate=None):

        self.source_key = source_key
        self.link_hash = link_hash
        self.leader_ip = leader_ip
        self.data_hash = data_hash
        self.rate = rate
        
        self._ticks = rate / 1000.0
        self._retransmit_timer = LINK_RETRANSMIT_RATE
        self._timeout = LINK_PRODUCER_TIMEOUT

    def __str__(self):
        return f'PRODUCER: {self.link_hash}'

    def _refresh(self, host):
        self.leader_ip = host[0]
        self._timeout = LINK_PRODUCER_TIMEOUT

    @property
    def timed_out(self):
        return self._timeout < 0.0

    def _process_timer(self, elapsed, link_manager):
        self._timeout -= elapsed

        if self.timed_out:
            logging.info(f"{self} timed out")
            return

        # process this producer
        self._retransmit_timer -= elapsed
        self._ticks -= elapsed

        if self._ticks > 0:
            return

        database = link_manager._database

        # timer has expired, process data

        # update the timer
        self._ticks += self.rate

        if self.source_key not in database:
            logging.warn("source key not found!")
            return

        # get data and data hash
        data = database.get_item(self.source_key)
        hashed_data = catbus_string_hash(data.get_field('value').pack())        

        svc_manager = link_manager._service_manager

        # check if we're the link leader
        # if so, we don't transmit a producer message (since they are coming to us).
        if svc_manager.is_server(LINK_SERVICE, self.link_hash):
            self.data_hash = hashed_data

            return

        # check if data did not change and the retransmit timer has not expired
        if (hashed_data == self._hashed_data) and \
           (self._retransmit_timer > 0):
            return

        self._hashed_data = hashed_data
        self._retransmit_timer = LINK_RETRANSMIT_RATE

        # check if leader is available
        if self.leader_ip is None:
            return

        # TRANSMIT PRODUCER DATA
        msg = ProducerDataMsg(hash=self.hash, data=data)

        link_manager.transmit(msg, self.server)


class Consumer(object):
    def __init__(self,
        link_hash=None,
        ip=None):

        self.link_hash = link_hash
        self.ip = ip
        
        self._timeout = LINK_CONSUMER_TIMEOUT

    def __str__(self):
        return f'CONSUMER: {self.link_hash}'    

    def _refresh(self, host):
        self.leader_ip = host[0]
        self._timeout = LINK_CONSUMER_TIMEOUT

    @property
    def timed_out(self):
        return self._timeout < 0.0

    def _process_timer(self, elapsed, link_manager):
        self._timeout -= elapsed

        if timed_out:
            logging.info(f"{self} timed out")
            return

        # check if we have a matching link
        if self.link_hash not in link_manager._links:
            self._timeout = -1.0
            return

        # check if not link leader, if not, force a time out
        if not link_manager._links[self.link_hash].is_leader:
            self._timeout = -1.0
            return


class Remote(object):
    def __init__(self,
        link_hash=None,
        ip=None,
        data=None):

        self.link_hash = link_hash
        self.ip = ip
        self.data = data
        
        self._timeout = LINK_REMOTE_TIMEOUT

    def __str__(self):
        return f'REMOTE: {self.link_hash}'    

    def _refresh(self, host, data):
        self.leader_ip = host[0]
        self._timeout = LINK_REMOTE_TIMEOUT
        self.data = data

        print(data, host)

    @property
    def timed_out(self):
        return self._timeout < 0.0

    def _process_timer(self, elapsed, link_manager):
        self._timeout -= elapsed

        if self.timed_out:
            logging.info(f"{self} timed out")
            return


"""
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Need to add data port to entire link system so we can run multiple link services
on the same machine!


"""

class LinkManager(MsgServer):
    def __init__(self, database=None):
        super().__init__(name='link_manager', listener_port=CATBUS_LINK_PORT, listener_mcast=LINK_MCAST_ADDR)

        if database is None:
            # create default database
            database = Database()
            database.add_item('kv_test_key', os.getpid(), 'uint32')

        self._database = database
        self._service_manager = services.ServiceManager()
        
        self.register_message(ConsumerQueryMsg, self._handle_consumer_query)
        self.register_message(ConsumerMatchMsg, self._handle_consumer_match)
        self.register_message(ProducerQueryMsg, self._handle_producer_query)
        self.register_message(ConsumerDataMsg, self._handle_consumer_data)
        self.register_message(ProducerDataMsg, self._handle_producer_data)
        
        self.start_timer(LINK_MIN_TICK_RATE, self._process_links)
        self.start_timer(LINK_MIN_TICK_RATE, self._process_producers)
        self.start_timer(LINK_MIN_TICK_RATE, self._process_consumers)
        self.start_timer(LINK_MIN_TICK_RATE, self._process_remotes)
        self.start_timer(LINK_DISCOVER_RATE, self._process_discovery)

        self._links = {}
        self._producers = {}
        self._consumers = {}
        self._remotes = {}

    async def clean_up(self):
        pass

    def add_link(self, link):
        link._service = self._service_manager.join_team(LINK_SERVICE, link.hash, self._port, priority=LINK_BASE_PRIORITY)
        self._links[link.hash] = link

    def _handle_consumer_query(self, msg, host):
        if msg.mode == LINK_MODE_SEND:
            # check if we have this link, if so, we are part of the send group,
            # not the consumer group, even if we would otherwise match the query.
            # this is a bit of a corner case, but it handles the scenario where
            # a send producer also matches as a consumer and is receiving data
            # it is trying to send.
            if msg.hash in self._links:
                return

            # query local database
            if not self._database.query(*msg.query):
                return

        elif msg.mode == LINK_MODE_RECV:
            logging.warn("receive links should not be sending consumer query")
            return

        # check if we have this key
        if msg.key not in self._database:
            return


        # logging.debug("consumer match")

        # TRANSMIT CONSUMER MATCH
        msg = ConsumerMatchMsg(hash=msg.hash)
        self.transmit(msg, host)

    def _handle_producer_query(self, msg, host):
        # query local database
        if not self._database.query(*msg.query):
            return
        
        # check if we have this key
        if msg.key not in self._database:
            return

        # UPDATE PRODUCER STATE
        if msg.hash not in self._producers:
            p = Producer(
                    msg.key,
                    msg.hash,
                    host[0], # ADD DATA PORT!
                    rate=msg.rate)

            self._producers[msg.hash] = p

            logging.debug(f"Create {p}")

        self._producers[msg.hash]._refresh(host)

    def _handle_consumer_match(self, msg, host):
        # UPDATE CONSUMER STATE

        if msg.hash not in self._consumers:
            return

        if msg.hash not in self._consumers:
            c = Consumer(
                    msg.hash,
                    host[0]) # ADD DATA PORT!

            self._consumers[msg.hash] = c

            logging.debug(f"Create {c}")

        self._consumers[msg.hash]._refresh(host)

    def _handle_consumer_data(self, msg, host):
        # check if we have this key
        if msg.hash not in self._database:
            logging.error("key not found!")
            return


        # UPDATE DATABASE
        # print(msg.data)
        self._database[msg.hash] = msg.data.value
        
    def _handle_producer_data(self, msg, host):
        # check if we have this link
        try:
            link = self._links[msg.hash]

        except KeyError:
            return

        if not link.is_leader:
            logging.error("link is not a leader!")
            return

        if link.dest_key not in self._database:
            logging.error("dest key not found!")
            return

        if msg.data.meta.hash != catbus_string_hash(link.source_key):
            logging.error("producer sent wrong source key!")
            return

        # UPDATE REMOTE STATE
        if msg.hash not in self._remotes:
            r = Remote(
                    msg.hash,
                    host[0]) # ADD DATA PORT!

            self._remotes[msg.hash] = r

            logging.debug(f"Create {r}")

        self._remotes[msg.hash]._refresh(host, msg.data)
    
    def _process_discovery(self):
        for link in self._links.values():
            if link.is_leader:
                if link.mode == LINK_MODE_SEND:
                    msg = ConsumerQueryMsg(
                            key=catbus_string_hash(link.dest_key),
                            query=link.query_tags,
                            mode=link.mode,
                            hash=link.hash)

                    self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))

                elif link.mode == LINK_MODE_RECV:

                    msg = ProducerQueryMsg(
                            key=catbus_string_hash(link.source_key),
                            query=link.query_tags,
                            mode=link.mode,
                            rate=link.rate,
                            hash=link.hash)

                    self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))

            elif link.is_follower: # not link leader
                if link.mode == LINK_MODE_RECV:
                    msg = ConsumerMatchMsg(hash=link.hash)

                    self.transmit(msg, (link.server, CATBUS_LINK_PORT))

    
    def _process_producers(self):
        for p in self._producers.values():
            p._process_timer(LINK_MIN_TICK_RATE, self)

        # prune
        self._producers = {k: v for k, v in self._producers.items() if not v.timed_out}

    def _process_consumers(self):
        for c in self._consumers.values():
            c._process_timer(LINK_MIN_TICK_RATE, self)

        # prune
        self._consumers = {k: v for k, v in self._consumers.items() if not v.timed_out}

    def _process_remotes(self):
        for r in self._remotes.values():
            r._process_timer(LINK_MIN_TICK_RATE, self)

        # prune
        self._remotes = {k: v for k, v in self._remotes.items() if not v.timed_out}

    def _process_links(self):
        for link in self._links.values():
            link._process_timer(LINK_MIN_TICK_RATE, self)


def main():
    util.setup_basic_logging(console=True)

    c = CatbusService(tags=['link_group'])
    c.add_item('link_test_key', 0, 'int32')

    s = LinkManager(database=c)

    l = Link(
            mode=LINK_MODE_RECV, 
            source_key='link_test_key', 
            dest_key='kv_test_key', 
            query=['link_group'], 
            aggregation=LINK_AGG_AVG)

    print(hex(l.hash))
    s.add_link(l)

    run_all()

if __name__ == '__main__':
    main()
    