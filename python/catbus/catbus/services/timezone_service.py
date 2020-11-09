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
import logging
from catbus import CatbusService, Client, NoResponseFromHost
from sapphire.common import util, wait_for_signal, Ribbon

class TimeZoneService(Ribbon):
    def initialize(self, settings={}):
        self.name = 'timezoneservice'
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

        self.client = Client()
        self.directory = {}

    def loop(self):
        self.directory = self.client.get_directory()

        if self.directory == None:
            self.wait(10.0)
            return

        current_tz_offset = time.localtime().tm_gmtoff / 60

        logging.info(f"Current TZ Offset: {current_tz_offset}")

        sys.exit(1)

        for device_id, device in self.directory.items():
            self.client.connect(device['host'])
            
            try:
                tz_offset = self.client.get_key('datetime_tz_offset')

            except KeyError:
                continue

            except NoResponseFromHost:
                logging.warn(f"No response from host: {device['host']}")
                continue
            
            if tz_offset != current_tz_offset:
                logging.info(f"Setting datetime_tz_offset on {device_id}@{device['host']} from {tz_offset} to {current_tz_offset}")            
                self.client.set_key('datetime_tz_offset', current_tz_offset)

        self.wait(600.0)

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

    tz_service = TimeZoneService(settings=settings)

    wait_for_signal()

    tz_service.stop()
    tz_service.join()

if __name__ == '__main__':
    main()
