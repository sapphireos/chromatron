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
from catbus import CatbusService, Client
from sapphire.common.ribbon import wait_for_signal, Ribbon

class TimeZoneService(Ribbon):
    def initialize(self, settings={}):
        self.name = 'timezoneservice'
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

        self.directory = client.get_directory()

    def loop(self):
        self.wait(600.0)
        self.directory = client.get_directory()



    def clean_up(self):
        self.kv.stop()
        self.kv.wait()


def main():
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
