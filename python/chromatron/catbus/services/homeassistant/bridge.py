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
from catbus.services.mqtt_client import MQTTClient
from sapphire.common.ribbon import wait_for_signal

from sapphire.common import util, Ribbon

import logging


class MQTTChromatron(MQTTClient):
	pass


class MQTTBridge(Ribbon):
    def initialize(self, settings={}):
        super().initialize()
        self.name = 'ha_bridge'
        self.settings = settings

        # run local catbus directory
        self._catbus_directory = Directory()

        self.devices = {}

        self.update_directory()

    def update_directory(self):
        self.directory = self._catbus_directory.get_directory()

        self._last_directory_update = time.time()

    def loop(self):
    	time.sleep(1.0)

    	print(self.directory())


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
