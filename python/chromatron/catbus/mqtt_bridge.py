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
                  MQTTTopic(_name="topic"),
                  CatbusMeta(_name="meta")]

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

        # self.mqtt_client.subscribe("chromatron_mqtt/fx_test")

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
        
        # meta = CatbusMeta(hash=0, type=CATBUS_TYPE_INT32)
        # data = CatbusData(meta=meta, value=int(msg.payload))

        # kv_payload = MQTTKVPayload(data=data)

        # msg = MqttPublishKVMsg(topic=topic, payload=kv_payload)
        # print(msg)

        # print(self.subs)
        if msg.topic in self.subs:
            meta = CatbusMeta(hash=0, type=CATBUS_TYPE_INT32)
            data = CatbusData(meta=meta, value=int(msg.payload))

            kv_payload = MQTTKVPayload(data=data)

            publish_msg = MqttPublishKVMsg(topic=topic, payload=kv_payload)

            for host in self.subs[msg.topic]:
                print(host, publish_msg)
                self.transmit(publish_msg, host)
        

    def _handle_publish(self, msg, host):
        # print(msg)
        # print(msg.payload.data.pack())

        # shovel the raw bytes in to MQTT
        self.mqtt_client.publish(msg.topic.topic, msg.payload.data.pack())  

    def _handle_publish_kv(self, msg, host):
        # print(msg)
    
        self.mqtt_client.publish(msg.topic.topic, json.dumps(msg.payload.data.toBasic()['value']))

    def _handle_subscribe(self, msg, host):
        # print(msg)
        self.mqtt_client.subscribe(msg.topic.topic)

    def _handle_subscribe_kv(self, msg, host):
        print(msg)

        if msg.topic not in self.subs:
            self.subs[msg.topic.topic] = {}

        self.subs[msg.topic.topic][host] = SUB_TIMEOUT

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
    

