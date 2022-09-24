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
import subprocess
from catbus import Client, NoResponseFromHost, CATBUS_MAIN_PORT
from catbus.services.datalogger import DataloggerClient
from sapphire.common import util, Ribbon, run_all

class ConnectionMonitor(Ribbon):
    def __init__(self, settings={}):
        super().__init__()

        self.name = 'connection_monitor'
        self.settings = settings
        
        self.datalogger = DataloggerClient()
        self.client = Client()
        self.directory = {}

        self.start()

    def icmp_ping(self, address):
        try:
            start = time.time()
            subprocess.check_output(['ping','-c1', address])
            elapsed = time.time() - start

            return elapsed

        except subprocess.CalledProcessError:
            return None

    def _process(self):
        self.directory = self.client.get_directory()

        if self.directory == None:
            self.wait(10.0)
            return

        logging.info(f"Pinging {len(self.directory)} devices")

        for device_id, device in self.directory.items():
            host = device['host']

            if host[1] != CATBUS_MAIN_PORT:
                continue

            name = device['name']

            if len(name)== 0:
                continue

            # location = device['location']
            location = ''

            icmp_ping = self.icmp_ping(host[0])

            self.client.connect(host)
            client_ping = self.client.ping()

            # print(host, client_ping, icmp_ping)

            if icmp_ping is None:
                icmp_ping = -1

            else:
                icmp_ping = int(icmp_ping * 1000)

            client_ping = int(client_ping * 1000)

            self.datalogger.log(name, location, 'icmp_ping', icmp_ping)
            self.datalogger.log(name, location, 'client_ping', client_ping)


            # N_PINGS = 5

            # times = []
            # for i in range(N_PINGS):
            #     try:
                    
            #         times.append(self.client.ping())

                    
            #     except NoResponseFromHost:
            #         times.append('x')


            # time_str = ''
            # for t in times:
            #     if isinstance(t, str):
            #         time_str += f'{t:4}'

            #     else:
            #         time_str += f' {int(t * 1000):4}'

            # logging.info(f"{str(device['name']):24} @ {str(device['host']):24}: {time_str}")

        self.wait(30.0)

    def clean_up(self):
        pass


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
