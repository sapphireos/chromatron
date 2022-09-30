


import sys
import time
import socket
import json

from catbus.services.mqtt_client import MQTTClient
from catbus.services.datalogger import DataloggerClient

from sapphire.common import util, Ribbon, run_all
import logging

class ShellyLogger(MQTTClient):
    def __init__(self):
        super().__init__()
        
        self.name = 'shelly_logger'

        self.datalogger = DataloggerClient()

        self.start()

        try:
            self.connect(host='omnomnom.local')

        except socket.error:
            logging.warning(f'Shelly MQTT connection failed')
                    
            self.stop()

            raise  

    def on_connect(self, client, userdata, flags, rc):
        logging.info(f'Shelly MQTT connected: {self.name}')

        self.subscribe('shelly/+/+/status/switch:0')
        
    def on_disconnect(self, client, userdata, rc):
        logging.info(f'Shelly MQTT disconnected: {self.name}')

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf8')

        tokens = topic.split('/')
        name = tokens[1]
        location = tokens[2]

        data = json.loads(payload)

        data['tC'] = data['temperature']['tC']

        translate_names = {
            'output': 'enabled',
            'apower': 'power',
            'voltage': 'voltage',
            'current': 'current',
            'tC': 'temperature'
        }

        for key, item_name in translate_names.items():
            value = data[key]

            self.datalogger.log(name, location, translate_names[key], value)


def main():
    util.setup_basic_logging(console=True)

    shelly = ShellyLogger()

    run_all() 

if __name__ == '__main__':
    main()
