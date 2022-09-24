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
from datetime import datetime, timedelta
from catbus import CatbusService, Client, CATBUS_MAIN_PORT, query_tags, CatbusData, CatbusMeta, get_type_id, catbus_string_hash
from sapphire.protocols.msgflow import MsgFlowReceiver, MsgflowClient
from sapphire.common import util, run_all, Ribbon
from elysianfields import *

import logging

from influxdb import InfluxDBClient


DATALOG_MAGIC = 0x41544144
DATALOG_FLAGS_NTP_SYNC = 0x01

"""

Version notes:

v1 is the most basic protocol and is no longer used (but should still work)
v2 is a device format optimized to hold a small time series in a single packet.
v3 is a flexible single point format that contains name and location tags.


"""


class DatalogHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="magic"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="flags"),
                  ArrayField(_name="padding", _field=Uint8Field, _length=2)]

        super().__init__(_fields=fields, **kwargs)

class DatalogDataV3(StructField):
    def __init__(self, **kwargs):
        fields = [NTPTimestampField(_name="ntp_timestamp"),
                  CatbusData(_name="data"),
                  String32Field(_name="key"),
                  String32Field(_name="name"),
                  String32Field(_name="location")]

        super().__init__(_fields=fields, **kwargs)


class DatalogMetaV2(StructField):
    def __init__(self, **kwargs):
        fields = [NTPTimestampField(_name="ntp_base")]

        super().__init__(_fields=fields, **kwargs)

class DatalogDataV2(StructField):
    def __init__(self, **kwargs):
        fields = [Int32Field(_name="ntp_offset"),
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
            # print(key, value)

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
            
            # print(meta, header)
            # print(ntp_base, timestamp)

            delta = abs(timestamp - ntp_base)

            # check delta for validity
            if delta.total_seconds() > 600.0: # more than 10 minutes apart
                logging.error(f'Timestamp mismatch: {timestamp} {ntp_base}')

                return

            data = data[meta.size():] # slice buffer

            item_count = 0

            points = []

            # extract chunks
            while len(data) > 0:
                chunk = DatalogDataV2().unpack(data)

                # print(chunk)

                # sanity check ntp offset:
                if chunk.ntp_offset > 600000: # 10 minutes is pretty reasonable
                    logging.error(f'Invalid ntp offset: {chunk.ntp_offset}')

                    return

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
                # print(ntp_timestamp, key, value)
                # logging.info(f'{key:20}: {value:8} @ {ntp_timestamp}')

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

            # print(f'received {item_count} items')

        elif header.version == 3:            
            # slice past header
            data = data[header.size():]

            msg = DatalogDataV3().unpack(data)
            value = msg.data.value

            ntp_timestamp = util.ntp_to_datetime(msg.ntp_timestamp.seconds, msg.ntp_timestamp.fraction)

            tags = {'name': msg.name,
                    'location': msg.location}

            json_body = {
                "measurement": msg.key,
                "tags": tags,
                "time": ntp_timestamp.isoformat(),
                "fields": {
                    "value": value
                }
            }

            self.influx.write_points([json_body])

        else:
            logging.warning(f"Unknown message version: {header.version}")


class DataloggerClient(MsgflowClient):
    def __init__(self):
        super().__init__("datalogger")

        self.start()

    def log(self, name, location, key, data):
        now = util.now()

        header = DatalogHeader(magic=DATALOG_MAGIC, version=3)

        hashed_key = catbus_string_hash(key)

        if isinstance(data, int):
            data_type = get_type_id('int64')

        elif isinstance(data, float):
            data_type = get_type_id('float')

        else:
            raise Exception('invalid type')

        catbus_meta = CatbusMeta(hash=hashed_key, type=data_type)

        catbus_data = CatbusData(meta=catbus_meta, value=data)

        seconds, fraction = util.datetime_to_ntp(now)
        ntp_timestamp = NTPTimestampField(seconds=seconds, fraction=fraction)
            
        v3 = DatalogDataV3(ntp_timestamp=ntp_timestamp, data=catbus_data, key=key, name=name, location=location)

        self.send(header.pack() + v3.pack())


def main():
    util.setup_basic_logging(console=True, level=logging.INFO)

    d = Datalogger()
    # client = DataloggerClient()


    # while not client.connected:
    #     time.sleep(0.1)

    # client.log('name', 'location', 'stuff', 123)

    run_all()


if __name__ == '__main__':
    main()
