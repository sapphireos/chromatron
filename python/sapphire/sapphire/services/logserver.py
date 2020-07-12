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
from catbus import CatbusService
from ..common.ribbon import wait_for_signal
from ..protocols.msgflow import MsgFlowReceiver
from ..protocols.zeroconf_service import ZeroconfService
from ..common import util


class LogServer(MsgFlowReceiver):
    def initialize(self, settings={}):
        super().initialize(name='logserver', service='logserver')
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

        self.zeroconf = ZeroconfService("_logserver._tcp.local.", "Chromatron LogServer", port=12345)


    def clean_up(self):
        self.zeroconf.stop()
        self.kv.stop()

        super().clean_up()

    def on_receive(self, host, data):
        print(host, data)



def main():
    util.setup_basic_logging(console=True)

    settings = {}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    l = LogServer(settings=settings)

    wait_for_signal()

    l.stop()
    l.join(timeout=None)


if __name__ == '__main__':
    main()
