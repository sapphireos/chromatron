#
# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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

from catbus import CatbusService, Directory, Client
from catbus.services.mqtt_client import MQTTClient

from sapphire.common import util, Ribbon, run_all

import logging

from chromatron import Chromatron


class MQTTChromatron(MQTTClient):
    def __init__(self, ct=None):
        self.ct = ct
        self.last_update = time.time()

        super().__init__()

        self.connect()
        
    @property
    def device_name(self):
        return self.ct.name

    @property
    def unique_id(self):
        return self.ct.device_id

    @property
    def command_topic(self):
        return f'chromatron/{self.unique_id}/command'

    @property
    def state_topic(self):
        return f'chromatron/{self.unique_id}/state'

    @property
    def brightness_command_topic(self):
        return f'chromatron/{self.unique_id}/brightness/command'

    @property
    def brightness_state_topic(self):
        return f'chromatron/{self.unique_id}/brightness/state'

    @property
    def mqtt_discovery(self):
        payload = {
                    'name':                         self.device_name,
                    'unique_id':                    self.unique_id,
                    'state_topic':                  self.state_topic,
                    'command_topic':                self.command_topic,
                    'brightness_state_topic':       self.brightness_state_topic,
                    'brightness_command_topic':     self.brightness_command_topic,
                    'brightness_scale':             65535,

                  }

        return payload
    
    def update_state(self):
        self.last_update = time.time()

        # publish discovery message
        self.publish(f'homeassistant/light/chromatron/{self.unique_id}/config', json.dumps(self.mqtt_discovery))

        power_state = 'OFF'
        if self.ct.dimmer > 0.0:
            power_state = 'ON'

        # NOTE:
        # after we add gfx on/off support, we can switch from the sub dimmer to master.

        self.publish(self.state_topic, power_state)
        self.publish(self.brightness_state_topic, int(self.ct.sub_dimmer * 65535))

    def on_connect(self, client, userdata, flags, rc):
        self.subscribe(self.command_topic)
        self.subscribe(self.brightness_command_topic)

        self.update_state()

    def on_disconnect(self, client, userdata, rc):
        pass

    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode('utf8')

        # print(topic, payload)

        if topic == self.command_topic:
            if payload == 'OFF':
                self.ct.dimmer = 0.0

            else:
                self.ct.dimmer = 1.0

        elif topic == self.brightness_command_topic:
            self.ct.sub_dimmer = float(payload) / 65535.0

        else:
            return

        self.update_state()

    # def _process(self):
    #     super()._process()

        # if time.time() - self.last_update > 4.0:
            # self.update_state()

class MQTTBridge(Ribbon):
    def __init__(self, settings={}):
        super().__init__()
    
        self.name = 'ha_bridge'
        self.settings = settings

        # run local catbus directory
        # self._catbus_directory = Directory()
        self.client = Client()
        self.directory = {}

        self.devices = {}

        self.start()

    def update_directory(self):
        self._last_directory_update = time.time()

        # self.directory = self._catbus_directory.get_directory()
        directory = self.client.get_directory()

        if directory is None:
            return

        self.directory = directory

    def _process(self):
        self.wait(1.0)

        self.update_directory()

        for info in self.directory.values():
            device_id = info['device_id']

            if device_id not in self.devices:
                if info['name'] != 'grid':
                    continue

                ct = Chromatron(info['host'][0])
                self.devices[device_id] = MQTTChromatron(ct=ct)

                logging.info(f'Added device: {info["name"]}')

        # for device_id, device in self.devices.items():
            # device.update_state()

        # prune devices
        self.devices = {k: v for k, v in self.devices.items() if k in self.directory}

def main():
    util.setup_basic_logging(console=True)

    settings = {}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    bridge = MQTTBridge(settings=settings)

    run_all() 

if __name__ == '__main__':
    main()
