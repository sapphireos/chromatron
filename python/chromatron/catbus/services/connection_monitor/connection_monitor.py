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

import sys
import time
import logging
from catbus import CatbusService, Client, NoResponseFromHost
from sapphire.common import util, Ribbon, run_all

class ConnectionMonitor(Ribbon):
    def initialize(self, settings={}):
        self.name = 'connection_monitor'
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

        self.client = Client()
        self.directory = {}

    def loop(self):
        self.directory = self.client.get_directory()

        if self.directory == None:
            self.wait(10.0)
            return

        logging.info(f"Pinging {len(self.directory)} devices")

        for device_id, device in self.directory.items():
            self.client.connect(device['host'])

            try:
                elapsed = self.client.ping()

                logging.info(f"{device['name']} @ {device['host']}: {elapsed * 1000}")
            
            except NoResponseFromHost:
                logging.warn(f"{device['name']} @ {device['host']}: no response")

        self.wait(30.0)

    def clean_up(self):
        self.kv.stop()


def main():
    util.setup_basic_logging(console=True)

    settings = {}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    service = ConnectionMonitor(settings=settings)

    run_all()

if __name__ == '__main__':
    main()
