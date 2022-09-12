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
    def __init__(self, settings={}):
        super().__init__()

        self.name = 'connection_monitor'
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

        self.client = Client()
        self.directory = {}

        self.start()

    def _process(self):
        self.directory = self.client.get_directory()

        if self.directory == None:
            self.wait(10.0)
            return

        logging.info(f"Pinging {len(self.directory)} devices")

        for device_id, device in self.directory.items():
            self.client.connect(device['host'])

            N_PINGS = 5

            times = []
            for i in range(N_PINGS):
                try:
                    
                    times.append(self.client.ping())

                    
                except NoResponseFromHost:
                    times.append('x')


            time_str = ''
            for t in times:
                if isinstance(t, str):
                    time_str += f'{t:4}'

                else:
                    time_str += f' {int(t * 1000):4}'

            logging.info(f"{str(device['name']):24} @ {str(device['host']):24}: {time_str}")

        self.wait(10.0)

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
