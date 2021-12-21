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
from datetime import datetime, timedelta
from catbus import CatbusService, Client, CATBUS_MAIN_PORT, query_tags, CatbusData, CatbusMeta
from sapphire.protocols.msgflow import MsgFlowReceiver
from sapphire.common import util, run_all, Ribbon
from elysianfields import *

import logging

from influxdb import InfluxDBClient


DATALOG_MAGIC = 0x41544144
DATALOG_FLAGS_NTP_SYNC = 0x01

class DatalogHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="magic"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="flags"),
                  ArrayField(_name="padding", _field=Uint8Field, _length=2)]

        super().__init__(_fields=fields, **kwargs)

class DatalogMetaV2(StructField):
    def __init__(self, **kwargs):
        fields = [NTPTimestampField(_name="ntp_base")]

        super().__init__(_fields=fields, **kwargs)

class DatalogDataV2(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="ntp_offset"),
                  CatbusData(_name="data")]

        super().__init__(_fields=fields, **kwargs)
    
class DatalogDataV1(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusData(_name="data")]

        super().__init__(_fields=fields, **kwargs)

class DatalogMessageV1(StructField):
    def __init__(self, **kwargs):
        fields = [DatalogHeader(_name="header"),
                  DatalogDataV1(_name="data")]

        super().__init__(_fields=fields, **kwargs)


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

        header = DatalogHeader().unpack(data)

        if header.magic != DATALOG_MAGIC:
            logging.warning("Invalid message received")

            return

        if header.version == 1:
            unpacked_data = DatalogMessageV1().unpack(data).data
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

        elif header.version == 2:            
            host = (host[0], CATBUS_MAIN_PORT)

            # print("V2")
            # print(header)

            # slice past header
            data = data[header.size():]

            # get meta
            meta = DatalogMetaV2().unpack(data)

            if (header.flags & DATALOG_FLAGS_NTP_SYNC) == 0:
                ntp_base = timestamp

            else:
                ntp_base = util.ntp_to_datetime(meta.ntp_base.seconds, meta.ntp_base.fraction)
                
            # print(meta)
            # print(ntp_base)

            data = data[meta.size():] # slice buffer

            item_count = 0

            points = []

            # extract chunks
            while len(data) > 0:
                chunk = DatalogDataV2().unpack(data)

                delta = timedelta(seconds=chunk.ntp_offset / 1000.0)

                ntp_timestamp = ntp_base + delta
                # print(ntp_timestamp)


                # note this will only work with services running on 
                # port 44632, generally only devices, not Python servers.
                try:
                    info = self.directory[str(host)]

                except KeyError:
                    return

                key = self.kv._server.resolve_hash(chunk.data.meta.hash, host)

                value = chunk.data.value
                print(ntp_timestamp, key, value)

                tags = {'name': info['name'],
                        'location': info['location']}

                json_body = {
                    "measurement": key,
                    "tags": tags,
                    "time": ntp_timestamp.isoformat(),
                    "fields": {
                        "value": value
                    }
                }

                points.append(json_body)

                data = data[chunk.size():]                

                item_count += 1

            self.influx.write_points(points)

            print(f'received {item_count} items')

        else:
            logging.warning(f"Unknown message version: {header.version}")


def main():
    util.setup_basic_logging(console=True, level=logging.INFO)

    d = Datalogger()

    run_all()


if __name__ == '__main__':
    main()
