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
from catbus import CatbusService, Client, CATBUS_MAIN_PORT, query_tags
from sapphire.protocols.msgflow import MsgFlowReceiver
from sapphire.common import util, run_all, Ribbon
from sapphire.devices.sapphiredata import DatalogData

import logging

from influxdb import InfluxDBClient

DIRECTORY_UPDATE_INTERVAL = 8.0

class Datalogger(MsgFlowReceiver):
    def __init__(self, influx_server='omnomnom.local'):
        super().__init__(service='datalogger')
        
        self.kv = CatbusService(name='datalogger', visible=True, tags=[])
        self.influx = InfluxDBClient(influx_server, 8086, 'root', 'root', 'chromatron')

        self.directory = {}

        self.start_timer(DIRECTORY_UPDATE_INTERVAL, self.update_directory)

    def clean_up(self):
        self.kv.stop()
        super().clean_up()

    def update_directory(self):
        c = Client()
        self.directory = c.get_directory()

    def on_receive(self, host, data):
        timestamp = datetime.utcnow()

        unpacked_data = DatalogData().unpack(data)
        # print(host, unpacked_data)

        host = (host[0], CATBUS_MAIN_PORT)

        # note this will only work with services running on 
        # port 44632, generally only devices, not Python servers.
        try:
            info = self.directory[str(host)]

        except KeyError:
            return

        key = self.kv._server.resolve_hash(unpacked_data.data.meta.hash, host)
        value = unpacked_data.data.value
        print(key, value)

        tags = {'name': info['name'],
                'location': info['location']}

        json_body = [
            {
                "measurement": key,
                "tags": tags,
                "time": timestamp.isoformat(),
                "fields": {
                    "value": value
                }
            }
        ]

        self.influx.write_points(json_body)



def main():
    util.setup_basic_logging(console=True, level=logging.INFO)

    d = Datalogger()

    run_all()


if __name__ == '__main__':
    main()
