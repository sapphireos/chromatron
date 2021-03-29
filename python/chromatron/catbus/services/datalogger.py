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
from datetime import datetime
from catbus import CatbusService, Client, CATBUS_MAIN_PORT
from sapphire.protocols.msgflow import MsgFlowReceiver
from sapphire.common import util, run_all, Ribbon
import logging

from influxdb import InfluxDBClient

from queue import Queue


class Datalogger(Ribbon):
    def __init__(self, influx_server='omnomnom.local'):
        super().__init__()

        self.kv = CatbusService(name='datalogger', visible=True, tags=[])
        self.client = Client()
        self.influx = InfluxDBClient(influx_server, 8086, 'root', 'root', 'chromatron')

        self.keys = ['wifi_rssi']

        self.directory = None

        self.start()

    def received_data(self, key, data, host):
        timestamp = datetime.utcnow()
        host = (host[0], CATBUS_MAIN_PORT)

        # note this will only work with services running on 
        # port 44632, generally only devices, not Python servers.

        info = self.directory[str(host)]

        tags = {'name': info['name'],
                'location': info['location']}

        json_body = [
            {
                "measurement": key,
                "tags": tags,
                "time": timestamp.isoformat(),
                "fields": {
                    "value": data
                }
            }
        ]

        self.influx.write_points(json_body)

    def _process(self):
        self.directory = self.client.get_directory()

        if self.directory is None:
            self.wait(10.0)
            return

        for dev in self.directory.values():
            host = dev['host'][0]
            
            for k in self.keys:
                self.kv.subscribe(k, host, rate=1000, callback=self.received_data)


        self.wait(10.0)

def main():
    util.setup_basic_logging(console=True, level=logging.INFO)

    d = Datalogger()

    run_all()


if __name__ == '__main__':
    main()
