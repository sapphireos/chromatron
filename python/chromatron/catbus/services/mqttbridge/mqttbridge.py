#
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
#


import sys
import time
import json

from catbus import CatbusService, Directory
from sapphire.common.ribbon import wait_for_signal

from sapphire.common import util, Ribbon

import logging

import paho.mqtt.client as mqtt



class MQTTBridge(Ribbon):
    def initialize(self, settings={}):
        super().initialize()
        self.name = 'mqtt_bridge'
        self.settings = settings

        self.mqtt = mqtt.Client()

        self.mqtt.connect('omnomnom.local')

        self.mqtt.on_connect = self.on_connect
        self.mqtt.on_disconnect = self.on_disconnect
        self.mqtt.on_message = self.on_message

        # run local catbus directory
        # self._catbus_directory = Directory()

        # self.update_directory()


    def update_directory(self):
        self.directory = self._catbus_directory.get_directory()

        self._last_directory_update = time.time()

    def on_connect(self, client, userdata, flags, rc):
        print("Connected with result code "+str(rc))

        # Subscribing in on_connect() means that if we lose the connection and
        # reconnect then subscriptions will be renewed.
        # client.subscribe("$SYS/#")

        payload = {
            'state_topic': 'chromatron/node0/state',
            'command_topic': 'chromatron/node0/command',
            'name': 'jeremy_test2',
            'unique_id': 124,
            'brightness_command_topic': 'chromatron/node0/brightness',
            'brightness_state_topic': 'chromatron/node0/brightness',
            'hs_command_topic': 'chromatron/node0/hs',
            'hs_state_topic': 'chromatron/node0/hs',
        }

        self.mqtt.subscribe(payload['state_topic'])
        self.mqtt.subscribe(payload['command_topic'])
        self.mqtt.subscribe(payload['brightness_command_topic'])
        self.mqtt.subscribe(payload['brightness_state_topic'])
        self.mqtt.subscribe(payload['hs_command_topic'])
        self.mqtt.subscribe(payload['hs_state_topic'])


        self.mqtt.publish('homeassistant/light/chromatron/node0/config', json.dumps(payload))

        time.sleep(0.2)

        # self.mqtt.publish('chromatron/node0/command', json.dumps('ON'))
        self.mqtt.publish('chromatron/node0/state','ON')
        # self.mqtt.publish('chromatron/node0/brightness', json.dumps(50))

    def on_disconnect(self, client, userdata, rc):
        if rc != 0:
            print("Unexpected disconnection.")

    def on_message(self, client, userdata, msg):
        print(msg.topic+" "+str(msg.payload))


    def publish(self, topic, payload):
        self.mqtt.publish(topic, payload, qos=0, retain=False)

    def subscribe(self, topic):
        self.mqtt.subscribe(topic, qos=0)

    def unsubscribe(self, topic):
        self.mqtt.unsubscribe(topic)


    def loop(self):
        self.mqtt.loop(timeout=1.0)




def main():
    util.setup_basic_logging(console=True)

    settings = {}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    bridge = MQTTBridge(settings=settings)

    wait_for_signal()

    bridge.stop()
    bridge.join()   

if __name__ == '__main__':
    main()
