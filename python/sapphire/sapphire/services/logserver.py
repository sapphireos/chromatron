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
from ..common import util, Ribbon

import logging
import logging_loki

LOKI_SERVER = "http://localhost:3100"

class LokiHandler(Ribbon):
    def initialize(self, settings={}):
        super().initialize()
        self.name = 'lokihandler'
        self.settings = settings

        loki_handler = logging_loki.LokiHandler(
            url=f"{LOKI_SERVER}/loki/api/v1/push", 
            tags={"application": "chromatron-logserver"},
            # auth=("username", "password"),
            version="1",
        )

        self.logger = logging.getLogger('loki')
        self.logger.addHandler(loki_handler)

        self.logger.info("Loki handler started")

        # for reference:
        # self.logger.info(
        #     "Loki handler started", 
        #     extra={"tags": {"service": "lokihandler"}},
        # )
    
    def loop(self):
        msg = self.recv_msg()

        print(msg)

    def clean_up(self):
        super().clean_up()        

        self.logger.info("Loki handler stopped")


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


    def on_connect(self, host, device_id=None):
        print("connect", host, device_id)

    def on_disconnect(self, host):
        print("disconnect", host)


def main():
    util.setup_basic_logging(console=True)

    settings = {}
    try:
        with open('settings.json', 'r') as f:
            settings = json.loads(f.read())

    except FileNotFoundError:
        pass

    logserver = LogServer(settings=settings)
    loki = LokiHandler(settings=settings)

    wait_for_signal()

    logserver.stop()
    loki.stop()

    logserver.join()
    loki.join()



if __name__ == '__main__':
    main()
