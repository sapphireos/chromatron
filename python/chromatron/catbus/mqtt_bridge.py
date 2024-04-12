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

import os
import time
import logging

from elysianfields import *
from .data_structures import *
from .catbustypes import *
# from .options import *
from sapphire.common import MsgServer, util, catbus_string_hash, run_all, synchronized
from sapphire.protocols import services
# from .database import *
from .catbus import *
from .services.mqtt_client import MQTTClient
import threading

from fnvhash import fnv1a_64


MQTT_BRIDGE_PORT = 44899


MQTT_MSG_MAGIC    = 0x5454514d # 'MQTT'
MQTT_MSG_VERSION  = 1

SUB_TIMEOUT       = 60.0


class TestField(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusData(_name="data"),
                  StringField(_name="topic", _length=128)]
                  
        super().__init__(_fields=fields, **kwargs)

class MQTTPayload(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="data_len"),
                  ArrayField(_name="data", _field=Uint8Field)]
        
        super().__init__(_fields=fields, **kwargs)

        if 'data' in kwargs:
            self.data_len = len(kwargs['data'])

class MQTTKVPayload(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusData(_name="data")]
        
        super().__init__(_fields=fields, **kwargs)

class MQTTTopic(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="topic_len")]
                  # topic follows

        # look up type
        if 'topic' in kwargs:
            valuefield = StringField(_length=len(kwargs['topic']), _name='topic')
            valuefield._value = kwargs['topic']
    
            fields.append(valuefield)

        super().__init__(_fields=fields, **kwargs)

        if 'topic' in kwargs:
            self.topic_len = len(self.topic)

    def unpack(self, buffer):
        super().unpack(buffer)

        buffer = buffer[self.size():]

        # get value field based on type
        try:
            valuefield = StringField(_length=self.topic_len, _name='topic')

            try:
                valuefield.unpack(buffer)

            except UnicodeDecodeError as e:
                valuefield.unpack("~~~%s: %s~~~" % (type(e), str(e)))
            
            self._fields['topic'] = valuefield

        except KeyError as e:
            raise DataUnpackingError(e)

        except UnknownTypeError:
            self._fields['topic'] = UnknownField()

        return self

class MQTTMsgHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="magic"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="type"),
                  Uint8Field(_name="qos"),
                  Uint8Field(_name="flags")]

        super().__init__(_fields=fields, **kwargs)

        self.magic          = MQTT_MSG_MAGIC
        self.version        = MQTT_MSG_VERSION
        self.flags          = 0 
        self.type           = 0
        self.qos            = 0


MQTT_MSG_PUBLISH        = 20      
class MqttPublishMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header"),
                  MQTTTopic(_name="topic"),
                  MQTTPayload(_name="payload")]

        super().__init__(_name="mqtt_publish", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_PUBLISH

    # def unpack(self, buffer):
    #     super().unpack(buffer)

    #     buffer = buffer[self.size():]

    #     # get payload field based on type
    #     try:
    #         valuefield = get_field_for_type(self.payload_type, _name='payload')

    #         # if self.meta.array_len == 0:
    #         try:
    #             valuefield.unpack(buffer)

    #         except UnicodeDecodeError as e:
    #             valuefield.unpack("~~~%s: %s~~~" % (type(e), str(e)))
            
    #         self._fields['payload'] = valuefield

    #         # else:
    #         #     array = FixedArrayField(_field=type(valuefield), _length=self.meta.array_len + 1, _name='payload')
    #         #     array.unpack(buffer)
    #         #     self._fields['payload'] = array

    #     except KeyError as e:
    #         raise DataUnpackingError(e)

    #     except UnknownTypeError:
    #         self._fields['payload'] = UnknownField()

    #     return self



MQTT_MSG_PUBLISH_KV        = 21      
class MqttPublishKVMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header"),
                  MQTTTopic(_name="topic"),
                  MQTTKVPayload(_name="payload")]

        super().__init__(_name="mqtt_publish_kv", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_PUBLISH_KV

MQTT_MSG_SUBSCRIBE        = 30
class MqttSubscribeMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header"),
                  MQTTTopic(_name="topic")]

        super().__init__(_name="mqtt_subscribe", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_SUBSCRIBE


MQTT_MSG_SUBSCRIBE_KV     = 31
class MqttSubscribeKVMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header"),
                  MQTTTopic(_name="topic")]

        super().__init__(_name="mqtt_subscribe_kv", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_SUBSCRIBE_KV


MQTT_MSG_PUBLISH_STATUS   = 22      
class MqttPublishStatus(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header"),
                  CatbusQuery(_name="tags"),
                  Uint8Field(_name="mode"),
                  Uint32Field(_name="uptime"),
                  Int8Field(_name="rssi"),
                  Uint8Field(_name="cpu_percent"),
                  Uint16Field(_name="used_heap"),
                  Uint16Field(_name="pixel_power")]

        super().__init__(_name="mqtt_publish_status", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_PUBLISH_STATUS


class MqttBridge(MsgServer):
    def __init__(self):
        super().__init__(name='mqtt_bridge', port=MQTT_BRIDGE_PORT)

        self.register_message(MqttPublishMsg, self._handle_publish)
        self.register_message(MqttPublishKVMsg, self._handle_publish_kv)
        self.register_message(MqttSubscribeMsg, self._handle_subscribe)
        self.register_message(MqttSubscribeKVMsg, self._handle_subscribe_kv)
        self.register_message(MqttPublishStatus, self._handle_status)
            
        # self.start_timer(LINK_MIN_TICK_RATE, self._process_all)
        # self.start_timer(LINK_DISCOVER_RATE, self._process_discovery)

        self.mqtt_client = MQTTClient()
        self.mqtt_client.mqtt.on_message = self.on_message
        self.mqtt_client.start()
        self.mqtt_client.connect(host='omnomnom.local')

        self.mqtt_client.subscribe("chromatron_mqtt/fx_test")

        self.subs = {}

        self.start()

    def clean_up(self):
        self.mqtt_client.stop()

    #     msg = ShutdownMsg()

    #     self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))
    #     time.sleep(0.1)
    #     self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))
    #     time.sleep(0.1)
    #     self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))

    def on_message(self, client, userdata, msg):
        # logging.info(msg.topic + " " + str(msg.payload))
        # print(msg)

        # topic = msg.topic
        # topic_len = len(topic)
        # payload = msg.payload
        # payload_len = len(msg.payload)
        # print(topic, topic_len)

        topic = MQTTTopic(topic=msg.topic)
        # print(topic)

        # try type conversions:
        # msg_payload = Int64Field(int(msg.payload))

        # payload = MQTTPayload(payload=msg_payload)
        # print(payload)

        # value = int(msg.payload)
        # meta = CatbusMeta(hash=0, flags=0, type=CATBUS_TYPE_INT64, array_len=0)
        # data = CatbusData(meta=meta, value=value)
        # print(data)

        # payload = MQTTPayload(data=data)
        # print(payload)

        # print(msg.payload)
        # print(type(msg.payload))
        # print(len(msg.payload))



        # print(list(bytes(msg.payload)))

        # payload = MQTTPayload(data=list(msg.payload))
        # publish_msg = MqttPublishMsg(topic=topic, payload=payload)

        # print(publish_msg)

        # self.transmit(publish_msg, ('10.0.0.211', MQTT_BRIDGE_PORT))
        
        meta = CatbusMeta(hash=0, type=CATBUS_TYPE_INT32)
        data = CatbusData(meta=meta, value=int(msg.payload))

        kv_payload = MQTTKVPayload(data=data)

        msg = MqttPublishKVMsg(topic=topic, payload=kv_payload)
        print(msg)

        if msg.topic in self.subs:
            meta = CatbusMeta(hash=0, type=CATBUS_TYPE_INT32)
            data = CatbusData(meta=meta, value=int(msg.payload))

            kv_payload = MQTTKVPayload(data=data)

            msg = MqttPublishKVMsg(topic=topic, payload=kv_payload)

            for host in self.subs[msg.topic]:
                self.transmit(publish_msg, host)
        

    def _handle_publish(self, msg, host):
        # print(msg)
        # print(msg.payload.data.pack())

        # shovel the raw bytes in to MQTT
        self.mqtt_client.publish(msg.topic.topic, msg.payload.data.pack())  

    def _handle_publish_kv(self, msg, host):
        print(msg)
    
        self.mqtt_client.publish(msg.topic.topic, json.dumps(msg.payload.data.toBasic()['value']))

    def _handle_subscribe(self, msg, host):
        # print(msg)
        self.mqtt_client.subscribe(msg.topic.topic)

    def _handle_subscribe_kv(self, msg, host):
        print(msg)

        if msg.topic not in self.subs:
            self.subs[msg.topic] = {}

        self.subs[msg.topic][host] = SUB_TIMEOUT

        self.mqtt_client.subscribe(msg.topic.topic)

    def _handle_status(self, msg, host):
        dict_data = msg.toBasic()
        
        del dict_data['header']

        # convert hashes to strings
        c = Client((host[0], CATBUS_MAIN_PORT))
        tags = [c.lookup_hash(t)[t] for t in dict_data['tags'] if t != 0]
        dict_data['tags'] = tags


        topic = f'chromatron_mqtt/status/{tags[0]}'

        self.mqtt_client.publish(topic, json.dumps(dict_data))

def main():
    util.setup_basic_logging(console=True)
    
    import colored_traceback
    colored_traceback.add_hook()


    bridge = MqttBridge()

    # import yappi
    # yappi.start()

    run_all()

    # yappi.get_func_stats().print_all()

if __name__ == '__main__':

    # meta = CatbusMeta(hash=0, type=CATBUS_TYPE_INT32)
    # data = CatbusData(meta=meta, value=123)

    # t = TestField(data=data, topic="topic")

    # from pprint import pprint

    # print(t)
    # print(t.toBasic()['data'].toJSON())

    main()
    




# LINK_VERSION            = 1
# LINK_MAGIC              = 0x4b4e494c # 'LINK'

# LINK_SERVICE            = "link"

# LINK_MCAST_ADDR         = "239.43.96.32"

# LINK_MIN_TICK_RATE      = 20 / 1000.0
# LINK_MAX_TICK_RATE      = 2000 / 1000.0
# LINK_RETRANSMIT_RATE    = 2000 / 1000.0

# LINK_DISCOVER_RATE      = 4000 / 1000.0

# LINK_CONSUMER_TIMEOUT   = 64000 / 1000.0
# LINK_PRODUCER_TIMEOUT   = 64000 / 1000.0
# LINK_REMOTE_TIMEOUT     = 32000 / 1000.0

# LINK_BASE_PRIORITY      = 128



# class MsgHeader(StructField):
#     def __init__(self, **kwargs):
#         fields = [Uint32Field(_name="magic"),
#                   Uint8Field(_name="type"),
#                   Uint8Field(_name="version"),
#                   Uint8Field(_name="flags"),
#                   Uint8Field(_name="reserved"),
#                   Uint64Field(_name="origin_id"),
#                   CatbusHash(_name="universe")]

#         super().__init__(_fields=fields, **kwargs)

#         self.magic          = LINK_MAGIC
#         self.version        = LINK_VERSION
#         self.flags          = 0 
#         self.type           = 0
#         self.reserved       = 0
#         self.origin_id      = 0
#         self.universe       = 0
        
# LINK_MSG_TYPE_CONSUMER_QUERY        = 1        
# class ConsumerQueryMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   CatbusHash(_name="key"),
#                   CatbusQuery(_name="query"),
#                   Uint8Field(_name="mode"),
#                   Uint64Field(_name="hash")]

#         super().__init__(_name="consumer_query", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_CONSUMER_QUERY

# LINK_MSG_TYPE_CONSUMER_MATCH        = 2
# class ConsumerMatchMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   Uint64Field(_name="hash")]

#         super().__init__(_name="consumer_match", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_CONSUMER_MATCH

        
# LINK_MSG_TYPE_PRODUCER_QUERY        = 3        
# class ProducerQueryMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   CatbusHash(_name="key"),
#                   CatbusQuery(_name="query"),
#                   Uint16Field(_name="rate"),
#                   Uint64Field(_name="hash")]

#         super().__init__(_name="producer_query", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_PRODUCER_QUERY

# LINK_MSG_TYPE_CONSUMER_DATA         = 10
# class ConsumerDataMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   Uint64Field(_name="hash"),
#                   CatbusData(_name="data")]

#         super().__init__(_name="consumer_data", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_CONSUMER_DATA

# LINK_MSG_TYPE_PRODUCER_DATA         = 11
# class ProducerDataMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   Uint64Field(_name="hash"),
#                   CatbusData(_name="data")]

#         super().__init__(_name="producer_data", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_PRODUCER_DATA

# LINK_MSG_TYPE_ADD                   = 20
# class AddMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   CatbusHash(_name="source_key"),
#                   CatbusHash(_name="dest_key"),
#                   CatbusQuery(_name="query"),
#                   CatbusHash(_name="tag"),
#                   Uint8Field(_name="mode"),
#                   Uint8Field(_name="aggregation"),
#                   Uint16Field(_name="rate"),
#                   Uint16Field(_name="filter")]

#         super().__init__(_name="link_add", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_ADD

# LINK_MSG_TYPE_DELETE                 = 21
# class DeleteMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   CatbusHash(_name="tag"),
#                   Uint64Field(_name="hash")]

#         super().__init__(_name="link_delete", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_DELETE

# LINK_MSG_TYPE_CONFIRM                = 22
# class ConfirmMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header"),
#                   Int32Field(_name="status")]

#         super().__init__(_name="link_confirm", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_CONFIRM

# LINK_MSG_TYPE_SHUTDOWN                = 30
# class ShutdownMsg(StructField):
#     def __init__(self, **kwargs):
#         fields = [MsgHeader(_name="header")]

#         super().__init__(_name="link_shutdown", _fields=fields, **kwargs)

#         self.header.type = LINK_MSG_TYPE_SHUTDOWN


# LINK_MODE_SEND  = 0
# LINK_MODE_RECV  = 1
# LINK_MODE_SYNC  = 2

# LINK_AGG_ANY    = 0
# LINK_AGG_MIN    = 1
# LINK_AGG_MAX    = 2
# LINK_AGG_SUM    = 3
# LINK_AGG_AVG    = 4

# LINK_RATE_MIN   = 20 / 1000.0
# LINK_RATE_MAX   = 30000 / 1000.0

# class _LinkState(StructField):
#     def __init__(self, **kwargs):
#         fields = [Uint32Field(_name="source_key"),
#                   Uint32Field(_name="dest_key"),
#                   CatbusQuery(_name="query"),
#                   Uint8Field(_name="mode"),
#                   Uint8Field(_name="aggregation"),
#                   Uint16Field(_name="filter"),
#                   Uint16Field(_name="rate")]

#         super().__init__(_name="link_state", _fields=fields, **kwargs)


# class Link(object):
#     def __init__(self,
#         mode=LINK_MODE_SEND,
#         source_key=None,
#         dest_key=None,
#         query=[],
#         aggregation=LINK_AGG_ANY,
#         rate=1000,
#         tag=''):

#         query.sort(reverse=True)

#         self.mode = mode
#         self.source_key = source_key
#         self.dest_key = dest_key
#         self.query = query
#         self.aggregation = aggregation
#         self.filter = 0
#         self.rate = rate
#         self.tag = tag

#         self._service = None

#         self._ticks = self.rate / 1000.0
#         self._retransmit_timer = 0
#         self._hashed_data = 0

#         self._lock = None

#     @property
#     @synchronized
#     def is_leader(self):
#         return self._service.is_server

#     @property
#     @synchronized
#     def is_follower(self):
#         return (not self._service.is_server) and self._service.connected

#     @property
#     @synchronized
#     def server(self):
#         return self._service.server

#     @property
#     @synchronized
#     def query_tags(self):
#         query = CatbusQuery()
#         query._value = [catbus_string_hash(a) for a in self.query]
#         return query

#     @property
#     @synchronized
#     def hash(self):
#         data = _LinkState(
#                 source_key=catbus_string_hash(self.source_key),
#                 dest_key=catbus_string_hash(self.dest_key),
#                 query=self.query_tags,
#                 mode=self.mode,
#                 aggregation=self.aggregation,
#                 filter=self.filter,
#                 rate=self.rate
#                 ).pack()

#         return fnv1a_64(data)

#     def __str__(self):
#         if self.mode == LINK_MODE_SEND:
#             return f'SEND {self.source_key} to {self.dest_key} at {self.query}'
#         elif self.mode == LINK_MODE_RECV:
#             return f'RECV {self.source_key} to {self.dest_key} at {self.query}'
#         # elif self.mode == LINK_MODE_SYNC:
#             # return f'SYNC {self.source_key} to {self.dest_key} at {self.query}'

#     def _process_timer(self, elapsed, link_manager):
#         self._ticks -= elapsed
#         self._retransmit_timer -= elapsed

#         if self._ticks > 0 :
#             return

#         database = link_manager._database

#         # update ticks for next iteration
#         self._ticks += self.rate / 1000.0

#         if self.mode == LINK_MODE_SEND:
#             if self.is_leader:
#                 data = link_manager._aggregate(self)

#                 if data is not None:
#                     link_manager._send_consumer_data(self, data)

#             elif self.is_follower:
#                 if self.source_key not in database:
#                     return

#                 data = database.get_item(self.source_key)
#                 hashed_data = catbus_string_hash(data.get_field('value').pack())

#                 # check if data did not change and the retransmit timer has not expired
#                 if (hashed_data == self._hashed_data) and \
#                    (self._retransmit_timer > 0):
#                     return

#                 self._hashed_data = hashed_data
#                 self._retransmit_timer = LINK_RETRANSMIT_RATE

#                 # TRANSMIT PRODUCER DATA
#                 msg = ProducerDataMsg(hash=self.hash, data=data)

#                 link_manager.transmit(msg, self.server)

#         elif self.mode == LINK_MODE_RECV:
#             if self.is_leader:
#                 data = link_manager._aggregate(self)

#                 if data is not None:
#                     # set our own database
#                     database[self.dest_key] = data.value

#                     # transmit to consumers
#                     link_manager._send_consumer_data(self, data)


# class _Producer(object):
#     def __init__(self,
#         source_key=None,
#         link_hash=None,
#         leader_addr=None,
#         data_hash=None,
#         rate=None):

#         self.source_key = source_key
#         self.link_hash = link_hash
#         self.leader_addr = leader_addr
#         self.data_hash = data_hash
#         self.rate = rate
        
#         self._ticks = rate / 1000.0
#         self._retransmit_timer = LINK_RETRANSMIT_RATE
#         self._timeout = LINK_PRODUCER_TIMEOUT

#     def __str__(self):
#         return f'PRODUCER: {self.link_hash}'

#     def _refresh(self, host):
#         self.leader_addr = host
#         self._timeout = LINK_PRODUCER_TIMEOUT

#     @property
#     def timed_out(self):
#         return self._timeout < 0.0

#     def _process_timer(self, elapsed, link_manager):
#         self._timeout -= elapsed

#         if self.timed_out:
#             logging.info(f"{self} timed out")
#             return

#         # process this producer
#         self._retransmit_timer -= elapsed
#         self._ticks -= elapsed

#         if self._ticks > 0:
#             return

#         database = link_manager._database

#         # timer has expired, process data

#         # update the timer
#         self._ticks += self.rate / 1000.0

#         if self.source_key not in database:
#             logging.warn("source key not found!")
#             return

#         # get data and data hash
#         data = database.get_item(self.source_key)
#         hashed_data = catbus_string_hash(data.get_field('value').pack())        

#         svc_manager = link_manager._service_manager

#         # check if we're the link leader
#         # if so, we don't transmit a producer message (since they are coming to us).
#         if svc_manager.is_server(LINK_SERVICE, self.link_hash):
#             self.data_hash = hashed_data

#             return

#         # check if data did not change and the retransmit timer has not expired
#         if (hashed_data == self.data_hash) and \
#            (self._retransmit_timer > 0):
#             return

#         self.data_hash = hashed_data
#         self._retransmit_timer = LINK_RETRANSMIT_RATE

#         # check if leader is available
#         if self.leader_addr is None:
#             return

#         # TRANSMIT PRODUCER DATA
#         msg = ProducerDataMsg(hash=self.link_hash, data=data)

#         link_manager.transmit(msg, self.leader_addr)


# class _Consumer(object):
#     def __init__(self,
#         link_hash=None,
#         host=None):

#         self.link_hash = link_hash
#         self.host = host
        
#         self._timeout = LINK_CONSUMER_TIMEOUT

#     def __str__(self):
#         return f'CONSUMER: {self.link_hash}'    

#     def _refresh(self):
#         self._timeout = LINK_CONSUMER_TIMEOUT

#     @property
#     def timed_out(self):
#         return self._timeout < 0.0

#     def _process_timer(self, elapsed, link_manager):
#         self._timeout -= elapsed

#         if self.timed_out:
#             logging.info(f"{self} timed out")
#             return

#         # check if we have a matching link
#         if self.link_hash not in link_manager._links:
#             self._timeout = -1.0
#             return

#         # check if not link leader, if not, force a time out
#         if not link_manager._links[self.link_hash].is_leader:
#             self._timeout = -1.0
#             return


# class _Remote(object):
#     def __init__(self,
#         link_hash=None,
#         host=None,
#         data=None,
#         sub=None):

#         self.link_hash = link_hash
#         self.host = host
#         self.data = data
#         self.sub = sub
        
#         self._timeout = LINK_REMOTE_TIMEOUT

#     def __str__(self):
#         return f'REMOTE: {self.link_hash}'    

#     def _refresh(self, host, data):
#         self.host = host
#         self._timeout = LINK_REMOTE_TIMEOUT
#         self.data = data

#         if self.sub:
#             self.sub._set(data.value)

#     @property
#     def timed_out(self):
#         return self._timeout < 0.0

#     def _process_timer(self, elapsed, link_manager):
#         self._timeout -= elapsed

#         if self.timed_out:
#             logging.info(f"{self} timed out")
#             return


# class LinkClient(Client):
#     def __init__(self, host=None, universe=0):
#         super().__init__(host, universe, default_port=CATBUS_LINK_PORT)

#     def add_link(self, mode, source_key, dest_key, query, aggregation=LINK_AGG_ANY, rate=1000, tag=''):
#         msg = AddMsg(
#                 mode=mode,
#                 source_key=source_key,
#                 dest_key=dest_key,
#                 query=query,
#                 tag=tag,
#                 aggregation=aggregation,
#                 rate=rate)

#         response, host = self._exchange(msg)

#     def delete_link(self, tag=None, link_hash=None):
#         msg = LinkDeleteMsg()

#         if tag:
#             msg.tag = catbus_string_hash(tag)

#         elif link_hash:
#             msg.hash = link_hash

#         else:
#             assert False

#         response, host = self._exchange(msg)



#     def __init__(self, key, host, callback):
#         self.key = key
#         self.host = host
#         self.callback = callback

#         self._lock = threading.Lock()

#         self._data = None

#     @property
#     @synchronized
#     def data(self):
#         return self._data

#     def _set(self, data):
#         with self._lock:
#             self._data = data

#         if self.callback:
#             self.callback(self.key, data, self.host)

# class Subscription(object):
#     def __init__(self, key, host, callback):
#         self.key = key
#         self.host = host
#         self.callback = callback

#         self._lock = threading.Lock()

#         self._data = None

#     @property
#     @synchronized
#     def data(self):
#         return self._data

#     def _set(self, data):
#         with self._lock:
#             self._data = data

#         if self.callback:
#             self.callback(self.key, data, self.host)

# def sub_hash(key, host):
#     return (catbus_string_hash(key) << 32) + catbus_string_hash(str(host))

# """
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# Need to add data port to entire link system so we can run multiple link services
# on the same machine!


# """

# class LinkManager(MsgServer):
#     def __init__(self, database=None, service_manager=None, test_mode=True):
#         super().__init__(name='link_manager', listener_port=CATBUS_LINK_PORT, listener_mcast=LINK_MCAST_ADDR)

#         if database is None:
#             # create default database
#             database = Database()
#             database.add_item('kv_test_key', os.getpid(), 'uint32')

#         self._database = database
        
#         if service_manager is None:
#             service_manager = services.ServiceManager()

#         self._service_manager = service_manager

#         self.register_message(ConsumerQueryMsg, self._handle_consumer_query)
#         self.register_message(ConsumerMatchMsg, self._handle_consumer_match)
#         self.register_message(ProducerQueryMsg, self._handle_producer_query)
#         self.register_message(ConsumerDataMsg, self._handle_consumer_data)
#         self.register_message(ProducerDataMsg, self._handle_producer_data)
#         self.register_message(ShutdownMsg, self._handle_shutdown)
            
#         self.start_timer(LINK_MIN_TICK_RATE, self._process_all)
#         self.start_timer(LINK_DISCOVER_RATE, self._process_discovery)

#         self._links = {}
#         self._producers = {}
#         self._consumers = []
#         self._remotes = {}
#         self._subscriptions = {}
#         self._meta = {} # used for publish

#         if test_mode:
#             database.add_item('link_test_key', 0, 'int32')
#             database.add_item('link_test_key2', 0, 'int32')
#             # self.start_timer(1.0, self._process_link_test)

#         self.start()

#     def clean_up(self):
#         self._service_manager.stop()

#         msg = ShutdownMsg()

#         self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))
#         time.sleep(0.1)
#         self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))
#         time.sleep(0.1)
#         self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))

#     def _handle_shutdown(self, msg, host):
#         self._producers = {k: v for k, v in self._producers.items() if v.leader_addr != host}
        
#         self._consumers = [c for c in self._consumers if c.host != host]

#         self._remotes = {k: v for k, v in self._remotes.items() if v.host != host}

#     def _aggregate(self, link):
#         if link.mode == LINK_MODE_SEND:
#             key = link.source_key

#         elif link.mode == LINK_MODE_RECV:
#             key = link.dest_key

#         try:
#             data_item = self._database.get_item(key)

#         except KeyError:
#             logging.error(f"key {key} not found!")
#             return


#         local_data = data_item.value
        
#         try:
#             v0 = local_data[0]

#         except TypeError:
#             v0 = local_data

#         assert isinstance(v0, int) or isinstance(v0, float)

#         remote_data = [r.data.value for r in self._remotes.values() if r.link_hash == link.hash]

#         if link.mode == LINK_MODE_SEND:
#             data_set = [local_data]
#             data_set.extend(remote_data)

#         elif link.mode == LINK_MODE_RECV:
#             if len(self._remotes) == 0:
#                 return None

#             data_set = remote_data

#         else:
#             raise NotImplementedError(link.mode)

#         if link.aggregation == LINK_AGG_ANY:
#             value = data_set[0]

#         elif link.aggregation == LINK_AGG_MIN:
#             value = min(data_set)

#         elif link.aggregation == LINK_AGG_MAX:
#             value = max(data_set)

#         elif link.aggregation == LINK_AGG_SUM:
#             value = sum(data_set)

#         elif link.aggregation == LINK_AGG_AVG:
#             value = sum(data_set) / len(data_set)

#         else:
#             raise Exception('WTF', link.aggregation)

#         data_item.value = value

#         return data_item

#     @synchronized
#     def _add_link(self, link):
#         link._service = self._service_manager.join_team(LINK_SERVICE, link.hash, self._port, priority=LINK_BASE_PRIORITY)
#         self._links[link.hash] = link

#     @synchronized
#     def link(self, mode, source_key, dest_key, query=[], aggregation=LINK_AGG_ANY, rate=1000, tag=''):
#         l = Link(
#             mode,
#             source_key,
#             dest_key,
#             query,
#             aggregation,
#             rate,
#             tag)

#         l._lock = self._lock

#         self._add_link(l)

#         logging.info(f'{l}')

#     def receive(self, *args, **kwargs):
#         self.link(LINK_MODE_RECV, *args, **kwargs)

#     def send(self, *args, **kwargs):
#         self.link(LINK_MODE_SEND, *args, **kwargs)

#     def subscribe(self, key, host, rate=1000, callback=None):
#         if isinstance(host, str):
#             host = (host, CATBUS_LINK_PORT)

#         h = sub_hash(key, host)

#         sub = {
#             'host': host,
#             'key': key,
#             'rate': rate,
#             'item': Subscription(key, host, callback)
#         }

#         if h not in self._subscriptions:
#             # send a query now to try and link the target faster
#             self._send_subscription(sub, host)

#             logging.info(f'Subscribe to {key} on {host}')

#         self._subscriptions[h] = sub

#         return sub['item']

#     def publish(self, key, value, host):
#         if isinstance(host, str):
#             host = (host, CATBUS_LINK_PORT)

#         key_hash = catbus_string_hash(key)

#         array_len = 0
#         v0 = value
#         if isinstance(value, list):
#             array_len = len(value) - 1
#             v0 = value[0]

#         if isinstance(v0, int):
#             data_type = CATBUS_TYPE_INT64
        
#         elif isinstance(v0, float):
#             data_type = CATBUS_TYPE_FLOAT

#         else:
#             assert False

#         meta = CatbusMeta(
#             hash=key_hash, 
#             type=data_type,
#             array_len=array_len)

#         data = CatbusData(meta=meta, value=value)

#         msg = ConsumerDataMsg(hash=data.meta.hash, data=data)

#         self.transmit(msg, host)

#     def _send_consumer_data(self, link, data):
#         link_hash = link.hash
        
#         consumers = [c for c in self._consumers if c.link_hash == link_hash]

#         data.meta.hash = catbus_string_hash(link.dest_key)
#         msg = ConsumerDataMsg(hash=data.meta.hash, data=data)

#         for c in consumers:
#             self.transmit(msg, c.host)

#     def _handle_consumer_query(self, msg, host):
#         if msg.mode == LINK_MODE_SEND:
#             # check if we have this link, if so, we are part of the send group,
#             # not the consumer group, even if we would otherwise match the query.
#             # this is a bit of a corner case, but it handles the scenario where
#             # a send producer also matches as a consumer and is receiving data
#             # it is trying to send.
#             if msg.hash in self._links:
#                 return

#             # query local database
#             if not self._database.query(*msg.query):
#                 return

#         elif msg.mode == LINK_MODE_RECV:
#             logging.warn("receive links should not be sending consumer query")
#             return

#         # check if we have this key
#         if msg.key not in self._database:
#             return


#         # logging.debug("consumer match")

#         # TRANSMIT CONSUMER MATCH
#         msg = ConsumerMatchMsg(hash=msg.hash)
#         self.transmit(msg, host)

#     def _handle_producer_query(self, msg, host):
#         # query local database
#         if not self._database.query(*msg.query):
#             return
        
#         # check if we have this key
#         if msg.key not in self._database:
#             return

#         # UPDATE PRODUCER STATE
#         if msg.hash not in self._producers:
#             p = _Producer(
#                     msg.key,
#                     msg.hash,
#                     host,
#                     rate=msg.rate)

#             self._producers[msg.hash] = p

#             logging.debug(f"Create {p} at {host}")

#         self._producers[msg.hash]._refresh(host)

#     def _handle_consumer_match(self, msg, host):
#         # UPDATE CONSUMER STATE
#         found = False
#         for consumer in self._consumers:
#             if consumer.host == host and consumer.link_hash == msg.hash:
#                 found = True
#                 break

#         if not found:
#             c = _Consumer(
#                     msg.hash,
#                     host)

#             self._consumers.append(c)

#             logging.debug(f"Create {c} at {host}")

#         for consumer in self._consumers:
#             if consumer.host == host and consumer.link_hash == msg.hash:
#                 consumer._refresh()

#     def _handle_consumer_data(self, msg, host):
#         # check if we have this key
#         if msg.hash not in self._database:
#             logging.error("key not found!")
#             return

#         # UPDATE DATABASE
#         self._database[msg.hash] = msg.data.value
        
#     def _handle_producer_data(self, msg, host):
#         sub = None
#         # check if we have this link, or if this is from a subscription
#         try:
#             link = self._links[msg.hash]

#             if not link.is_leader:
#                 logging.error("link is not a leader!")
#                 return

#             if link.dest_key not in self._database:
#                 logging.error(f"dest key {link.dest_key} not found!")
#                 return

#             if msg.data.meta.hash != catbus_string_hash(link.source_key):
#                 logging.error("producer sent wrong source key!")
#                 return

#         except KeyError:
#             if msg.hash not in self._subscriptions:
#                 logging.error(f"link or subscription not found for producer data!")
#                 return

#             sub = self._subscriptions[msg.hash]

#         # UPDATE REMOTE STATE
#         if msg.hash not in self._remotes:
#             item = None
#             if sub:
#                 item = sub['item']

#             r = _Remote(
#                     msg.hash,
#                     host,
#                     sub=item)

#             self._remotes[msg.hash] = r

#             logging.debug(f"Create {r}")

#         self._remotes[msg.hash]._refresh(host, msg.data)
    
#     def _process_discovery(self):
#         for link in self._links.values():
#             if link.is_leader:
#                 if link.mode == LINK_MODE_SEND:
#                     msg = ConsumerQueryMsg(
#                             key=catbus_string_hash(link.dest_key),
#                             query=link.query_tags,
#                             mode=link.mode,
#                             hash=link.hash)

#                     self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))

#                 elif link.mode == LINK_MODE_RECV:

#                     msg = ProducerQueryMsg(
#                             key=catbus_string_hash(link.source_key),
#                             query=link.query_tags,
#                             rate=link.rate,
#                             hash=link.hash)

#                     self.transmit(msg, ('<broadcast>', CATBUS_LINK_PORT))

#             elif link.is_follower: # not link leader
#                 if link.mode == LINK_MODE_RECV:
#                     msg = ConsumerMatchMsg(hash=link.hash)

#                     self.transmit(msg, (link.server, CATBUS_LINK_PORT))

#         for sub in self._subscriptions.values():
#             self._send_subscription(sub, sub['host'])

#     def _send_subscription(self, sub, host):
#         msg = ProducerQueryMsg(
#                         key=catbus_string_hash(sub['key']),
#                         rate=sub['rate'],
#                         hash=sub_hash(sub['key'], host))
        
#         self.transmit(msg, host)
        
#     def _process_producers(self):
#         for p in self._producers.values():
#             p._process_timer(LINK_MIN_TICK_RATE, self)

#         # prune
#         self._producers = {k: v for k, v in self._producers.items() if not v.timed_out}

#     def _process_consumers(self):
#         for c in self._consumers:
#             c._process_timer(LINK_MIN_TICK_RATE, self)

#         # prune
#         self._consumers = [v for v in self._consumers if not v.timed_out]

#     def _process_remotes(self):
#         for r in self._remotes.values():
#             r._process_timer(LINK_MIN_TICK_RATE, self)

#         # prune
#         self._remotes = {k: v for k, v in self._remotes.items() if not v.timed_out}

#     def _process_link_test(self):
#         self._database['link_test_key'] += 1

#     def _process_links(self):
#         for link in self._links.values():
#             link._process_timer(LINK_MIN_TICK_RATE, self)

#     def _process_all(self):
#         self._process_producers()
#         self._process_consumers()
#         self._process_remotes()
#         self._process_links()

# def main():
#     util.setup_basic_logging(console=True)

#     # from .catbus import CatbusService

#     # c = CatbusService(tags=['__TEST__'])
        
#     # import sys
    
#     # if len(sys.argv) < 1:
#     #     pass

#     # elif sys.argv[1] == 'send':
#     #     c.send('link_test_key', 'link_test_key2', ['grid'])

#     # elif sys.argv[1] == 'receive':
#     #     c.receive('link_test_key', 'link_test_key2', ['grid'])

#     # elif sys.argv[1] == 'subscribe':
#     #     item = c.subscribe('link_test_key', sys.argv[2], callback=callback)
#     # elif sys.argv[1] == 'publish':
#     #     c.publish('link_test_key', int(sys.argv[3]), sys.argv[2])

#     # import yappi
#     # yappi.start()

#     run_all()

#     # yappi.get_func_stats().print_all()

# if __name__ == '__main__':
#     main()
    
