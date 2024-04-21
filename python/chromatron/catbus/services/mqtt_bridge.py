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
from catbus.data_structures import *
from catbus.catbustypes import *
from sapphire.common import MsgServer, util, catbus_string_hash, run_all, synchronized
from sapphire.protocols import services
from catbus import *
from .mqtt_client import MQTTClient
import threading


MQTT_BRIDGE_PORT = 44899


MQTT_MSG_MAGIC    = 0x5454514d # 'MQTT'
MQTT_MSG_VERSION  = 1

CLIENT_TIMEOUT    = 60.0


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
            topic = kwargs['topic'] + '\0' # null terminate!
            valuefield = StringField(_length=len(topic), _name='topic')
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


MQTT_MSG_PUBLISH_STATUS   = 10
class MqttPublishStatus(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header"),
                  CatbusQuery(_name="tags"),
                  Ipv4Field(_name="ip"),
                  Uint8Field(_name="mode"),
                  Uint32Field(_name="uptime"),
                  Int8Field(_name="rssi"),
                  Int8Field(_name="wifi_channel"),
                  Uint8Field(_name="cpu_percent"),
                  Uint16Field(_name="used_heap"),
                  Uint16Field(_name="pixel_power")]

        super().__init__(_name="mqtt_publish_status", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_PUBLISH_STATUS


MQTT_MSG_BRIDGE        = 1
class MqttBridgeMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header")]

        super().__init__(_name="mqtt_bridge", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_BRIDGE

MQTT_MSG_SHUTDOWN       = 2
class MqttShutdown(StructField):
    def __init__(self, **kwargs):
        fields = [MQTTMsgHeader(_name="header")]

        super().__init__(_name="mqtt_shutdown", _fields=fields, **kwargs)

        self.header.type = MQTT_MSG_SHUTDOWN


class ClientTimedOut(Exception):
    pass

class DeviceClient(object):
    def __init__(self, host, bridge):
        super().__init__()

        self.host = host
        self.bridge = bridge

        self.timeout = CLIENT_TIMEOUT

        self.mqtt_client = MQTTClient()
        self.mqtt_client.mqtt.on_message = self.on_message
        self.mqtt_client.start()
        self.mqtt_client.connect(host=bridge.mqtt_host)

        logging.info(f'Started client: {self.host}')

        self.subs = {}

    def on_message(self, client, userdata, msg):
        if msg.topic in self.subs:
            timeout, data_type = self.subs[msg.topic]
            
            # convert data types
            if data_type in [CATBUS_TYPE_BOOL, 
                             CATBUS_TYPE_UINT8,
                             CATBUS_TYPE_INT8,
                             CATBUS_TYPE_UINT16,
                             CATBUS_TYPE_INT16,
                             CATBUS_TYPE_UINT32,
                             CATBUS_TYPE_INT32,
                             CATBUS_TYPE_UINT64,
                             CATBUS_TYPE_INT64]:

                value = int(msg.payload)

            elif data_type in [CATBUS_TYPE_FLOAT, 
                               CATBUS_TYPE_FIXED16]:

                value = float(msg.payload)

            elif data_type is None:
                value = msg.payload

            else:
                logging.error(f"Unsupported data type: {data_type}")
                return

            if data_type is not None:
                meta = CatbusMeta(hash=0, type=data_type)
                data = CatbusData(meta=meta, value=value)

                kv_payload = MQTTKVPayload(data=data)

                publish_msg = MqttPublishKVMsg(topic=msg.topic, payload=kv_payload)

            else:
                publish_msg = MqttPublishMsg(topic=msg.topic, payload=value)

            self.bridge.transmit(publish_msg, self.host)

        else: # topic not in subs, unsubscribe
            self.unsubscribe(msg.topic)

    def clean_up(self):
        logging.info(f'Stopping client: {self.host}')
        self.mqtt_client.stop() 

    def publish(self, topic, data):
        self.mqtt_client.publish(topic, data)  

    def subscribe(self, topic, data_type=None):
        if topic not in self.subs:
            logging.info(f'{self.host} subscribed to {topic}')
        
        self.subs[topic] = [CLIENT_TIMEOUT, data_type]

        self.mqtt_client.subscribe(topic)

    def unsubscribe(self, topic):
        del self.subs[topic]
        self.mqtt_client.unsubscribe(topic)   

    def process_timeouts(self, elapsed):
        self.timeout -= elapsed

        if self.timeout < 0.0:
            raise ClientTimedOut

        remove = []
        for topic, sub in self.subs.items():
            sub[0] -= elapsed

            if sub[0] < 0.0:
                remove.append(topic)

        for topic in remove:
            logging.info(f'{self.host} unsubscribed from {topic}')
            self.unsubscribe(topic)

    def reset_timeout(self):
        self.timeout = CLIENT_TIMEOUT




"""

Client-per-device will be easier than trying to mux/demux here, 
when the broker will already do that for us if we use multiple clients.
The saved loading on the network for doing a mux here isn't worth it,
since the bridge and broker will typically be cabled Ethernet, so we
aren't taking up extra airtime on a wireless network.

Device status message triggers the client connection.
Subscribes are passed through to the broker.

Device timeouts/shutdown will close the client connection.

Device should send a "not subscribed" message when it receives a publish
but there are no subscriptions for it.

"""

class MqttBridge(MsgServer):
    def __init__(self, mqtt_host='omnomnom.local', mqtt_port=1883):
        super().__init__(name='mqtt_bridge', port=MQTT_BRIDGE_PORT)

        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port

        self.clients = {}

        self.register_message(MqttPublishMsg, self._handle_publish)
        self.register_message(MqttPublishKVMsg, self._handle_publish_kv)
        self.register_message(MqttSubscribeMsg, self._handle_subscribe)
        self.register_message(MqttSubscribeKVMsg, self._handle_subscribe_kv)
        self.register_message(MqttPublishStatus, self._handle_status)
        self.register_message(MqttShutdown, self._handle_shutdown)
        self.register_message(MqttBridgeMsg, self._handle_bridge)
            
        self.start_timer(1.0, self._process_devices)

        self.mqtt_client = MQTTClient()
        # self.mqtt_client.mqtt.on_message = self.on_message
        self.mqtt_client.start()
        self.mqtt_client.connect(host=self.mqtt_host)

        self.start()

    def clean_up(self):
        for client in self.clients.values():
            client.clean_up()

        self.clients = {}

        self.mqtt_client.stop()

    def _process_devices(self):
        bridge_msg = MqttBridgeMsg()
        self.transmit(bridge_msg, ("255.255.255.255", MQTT_BRIDGE_PORT))

        remove = []
        for client in self.clients.values():
            try:
                client.process_timeouts(1.0)

            except ClientTimedOut:
                remove.append(client)

        for client in remove:
            client.clean_up()
            del self.clients[client.host]

    def _handle_bridge(self, msg, host):
        pass

    def _handle_shutdown(self, msg, host):
        if host not in self.clients:
            return

        logging.info(f'Shutdown: {host}')

        client = self.clients[host]
        client.clean_up()
        del self.clients[client.host]

    def _handle_publish(self, msg, host):
        if host not in self.clients:
            return

        # shovel the raw bytes in to MQTT
        self.clients[host].publish(msg.topic.topic, msg.payload.data.pack())  

    def _handle_publish_kv(self, msg, host):
        if host not in self.clients:
            return

        self.clients[host].publish(msg.topic.topic, json.dumps(msg.payload.data.toBasic()['value']))

    def _handle_subscribe(self, msg, host):
        if host not in self.clients:
            return
 
        self.clients[host].subscribe(msg.topic.topic, data_type=None)

    def _handle_subscribe_kv(self, msg, host):
        if host not in self.clients:
            return

        self.clients[host].subscribe(msg.topic.topic, data_type=msg.meta.type)

    def _handle_status(self, msg, host):
        dict_data = msg.toBasic()
        
        del dict_data['header']

        # convert hashes to strings
        c = Client((host[0], CATBUS_MAIN_PORT))
        tags = [c.lookup_hash(t)[t] for t in dict_data['tags'] if t != 0]
        dict_data['tags'] = tags

        topic = f'chromatron_mqtt/status/{tags[0]}'


        if host not in self.clients:
            self.clients[host] = DeviceClient(host, self)

        self.clients[host].reset_timeout()

        self.clients[host].publish(topic, json.dumps(dict_data))



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
    main()
    

