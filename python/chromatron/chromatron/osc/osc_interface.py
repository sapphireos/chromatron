# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
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

import logging

from sapphire.keyvalue import KVService
from sapphire.common.ribbon import Ribbon

import select
import OSC
import time

LISTEN_PORT = 8000


class OSCInterface(Ribbon):
    def initialize(self, listen_port=None):

        # set up OSC server
        self.osc_server = OSC.OSCServer(("0.0.0.0", listen_port))
        self.osc_server.addMsgHandler('default', self.msg_handler)

        # set up OSC client
        self.osc_client = OSC.OSCClient()
        # self.osc_client.connect(target)

        self.kv = KVService(name='osc')

        self.ping = 0


    def msg_handler(self, addr, tags, stuff, source):
        try:
            # convert to 16 bit integer range
            value = int(stuff[0] * 65535)

        except IndexError:
            value = None

        addr = addr.lstrip('/')

        if addr == "ping":
            self.ping += 1
            value = self.ping

        self.kv[addr] = value
        
        # print addr, tags, stuff, source

    def send(self, addr, value):
        msg = OSC.OSCMessage(addr)
        msg.append(value)

        self.osc_client.send(msg)

    def run(self):
        try:
            self.osc_server.serve_forever()

        # The OSC server will raise a "Bad file descriptor" error when closed,
        # so we catch and ignore that here so we don't get an exception printed
        # to the console.
        except select.error:
            pass

        except Exception:
            raise

    def stop(self):
        self.osc_server.close()

        super(OSCInterface, self).stop()



if __name__ == "__main__":

    OSCInterface(listen_port=LISTEN_PORT)

    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

