#
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
#

import os
import sys
import time
import json

from sapphire.common import util, Ribbon

import logging

import paho.mqtt.client as mqtt
import socket

class MQTTHostNotFound(Exception):
    pass

class MQTTClient(Ribbon):
    def __init__(self, host='localhost'):
        super().__init__()

        if "MQTT_HOST" in os.environ:
            host = os.environ["MQTT_HOST"]

        self.host = host
        self.mqtt = mqtt.Client()

        self.mqtt.on_connect = self.on_connect
        self.mqtt.on_disconnect = self.on_disconnect
        self.mqtt.on_message = self.on_message

        self._connected = False
        self._connecting = False

    @property
    def connected(self):
        return self._connected

    def connect(self):
        try:
            self.mqtt.connect(self.host)        

        except socket.gaierror:
            raise MQTTHostNotFound(self.host)

    def clean_up(self):
        self.mqtt.disconnect()

    def on_connect(self, client, userdata, flags, rc):
        self._connected = True
        self._connecting = False

        logging.info("Connected with result code "+str(rc))

    def on_disconnect(self, client, userdata, rc):
        self._connected = False
        self._connecting = False

        if rc != 0:
            logging.info("Unexpected disconnection.")

        time.sleep(1.0)

    def on_message(self, client, userdata, msg):
        logging.info(msg.topic + " " + str(msg.payload))

    def publish(self, topic, payload, qos=0, retain=False):
        # logging.debug(f'Publish to: {topic}')
        self.mqtt.publish(topic, payload, qos=qos, retain=retain)

    def subscribe(self, topic, qos=0):
        # logging.debug(f'Subcribe to: {topic}')
        self.mqtt.subscribe(topic, qos=qos)

    def unsubscribe(self, topic):
        # logging.debug(f'Unsubcribe from: {topic}')
        self.mqtt.unsubscribe(topic)

    def _process(self):
        if not self._connected and not self._connecting:
            try:
                self._connecting = True
                self.connect()
                logging.info(f'MQTT connected')

            except socket.error:
                self._connecting = False
                logging.warning(f'MQTT connection failed')
                
                time.sleep(2.0)

        self.mqtt.loop(timeout=1.0)
        
        
        