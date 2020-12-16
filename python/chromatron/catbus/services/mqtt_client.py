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

from sapphire.common.ribbon import wait_for_signal

from sapphire.common import util, Ribbon

import logging

import paho.mqtt.client as mqtt


class MQTTClient(Ribbon):
    def initialize(self, settings={}):
        super().initialize()
        self.name = 'mqtt_client'
        self.settings = settings

        self.mqtt = mqtt.Client()

        self.mqtt.on_connect = self.on_connect
        self.mqtt.on_disconnect = self.on_disconnect
        self.mqtt.on_message = self.on_message

        self.connect()

    def connect(self, host=None):
        if host is None:
            try:
                host = self.settings['host']   

            except KeyError:
                host = 'localhost'

        self.mqtt.connect(host)

    def on_connect(self, client, userdata, flags, rc):
        print("Connected with result code "+str(rc))

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

