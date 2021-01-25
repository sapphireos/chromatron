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

from elysianfields import *
from .data_structures import *
from .catbustypes import *
from .options import *
from sapphire.common import MsgServer, util, catbus_string_hash, run_all
from sapphire.protocols import services

from fnvhash import fnv1a_64


LINK_VERSION            = 1
LINK_MAGIC              = 0x4b4e494c # 'LINK'

LINK_SERVICE            = "link"

LINK_MCAST_ADDR         = "239.43.96.32"


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
                  Uint8Field(_name="rate"),
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

LINK_RATE_MIN   = 20
LINK_RATE_MAX   = 30000

class LinkState(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="tag"),
                  Uint32Field(_name="source_key"),
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
        tag=None):

        self.mode = mode
        self.source_key = source_key
        self.dest_key = dest_key
        self.query = query
        self.aggregation = aggregation
        self.filter = None
        self.rate = rate
        self.tag = tag
        self._service = None

    @property
    def is_leader(self):
        return self._service.is_server

    @property
    def query_tags(self):
        query = CatbusQuery()
        query._value = [catbus_string_hash(a) for a in self.query]
        return query

    @property
    def hash(self):
        data = LinkState(
                tag=catbus_string_hash(self.tag),
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


"""
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Need to add data port to entire link system so we can run multiple link services
on the same machine!


"""

class LinkManager(MsgServer):
    def _init__(self, database=None):
        super()._init__(name='link_manager', listener_port=CATBUS_LINK_PORT, listener_mcast=LINK_MCAST_ADDR)

        self._database = database
        self._service_manager = services.ServiceManager()
        
        self.register_message(ConsumerQueryMsg, self._handle_consumer_query)
        self.register_message(ConsumerMatchMsg, self._handle_consumer_match)
        self.register_message(ProducerQueryMsg, self._handle_producer_query)
        self.register_message(ConsumerDataMsg, self._handle_consumer_data)
        self.register_message(ProducerDataMsg, self._handle_producer_data)
            
        # self.start_timer(1.0, self._process_timers)
        # self.start_timer(4.0, self._process_offer_timer)

        self._links = {}

    async def clean_up(self):
        await super().clean_up()

    def add_link(self, link):
        link._service = self._service_manager.join_team(LINK_SERVICE, link.hash, self._port)
        self._links[link.hash] = link

    def _handle_consumer_query(self, msg, host):
        pass

    def _handle_consumer_match(self, msg, host):
        pass

    def _handle_producer_query(self, msg, host):
        pass

    def _handle_consumer_data(self, msg, host):
        pass

    def _handle_producer_data(self, msg, host):
        pass


def main():
    util.setup_basic_logging(console=True)

    s = LinkManager()
    l = Link(mode=LINK_MODE_SEND, source_key='kv_test_key', dest_key='kv_test_key', query=['meow'])
    print(l.hash)
    s.add_link(l)

    run_all()

if __name__ == '__main__':
    main()
    