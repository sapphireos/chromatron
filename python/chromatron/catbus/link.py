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


from elysianfields import *
from .data_structures import *
from .catbustypes import *
from .options import *
from ..common import RibbonServer, util, catbus_string_hash


LINK_VERSION            = 1
LINK_MAGIC              = 0x4b4e494c # 'LINK'

LINK_SERVICE            = "link"


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
                  CatbusQuery(_name="query")
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
class ProcucerQueryMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  CatbusHash(_name="key"),
                  CatbusQuery(_name="query")
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



"""
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
Need to add data port to entire link system so we can run multiple link services
on the same machine!


"""

class LinkManager(RibbonServer):
    NAME = 'link_manager'

    def initialize(self):
        super().initialize()
        
        self.register_message(ConsumerQueryMsg, self._handle_consumer_query)
        self.register_message(ConsumerMatchMsg, self._handle_consumer_match)
        self.register_message(ProcucerQueryMsg, self._handle_producer_query)
        self.register_message(ConsumerDataMsg, self._handle_consumer_data)
        self.register_message(ProducerDataMsg, self._handle_producer_data)
            
        # self.start_timer(1.0, self._process_timers)
        # self.start_timer(4.0, self._process_offer_timer)

    def clean_up(self):
        pass

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
