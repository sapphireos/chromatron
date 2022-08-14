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
from datetime import datetime
from catbus import CatbusService, Directory, Client
from sapphire.protocols.msgflow import MsgFlowReceiver
from sapphire.common import util, run_all, Ribbon

from queue import Queue

import logging
import logging_loki

LOG_LEVEL = {
    'debug':    logging.DEBUG,
    'info':     logging.INFO,
    'warn':     logging.WARNING,
    'error':    logging.ERROR,
    'critical': logging.CRITICAL,
}

DIRECTORY_UPDATE_INTERVAL = 8.0

LOKI_SERVER = "http://localhost:3100"
LOGSERVER_PORT = None


class LokiHandler(Ribbon):
    def __init__(self):
        super().__init__(name='loki_handler')

        loki_handler = logging_loki.LokiHandler(
            url=f"{LOKI_SERVER}/loki/api/v1/push", 
            tags={"application": "chromatron-logserver"},
            # auth=("username", "password"),
            version="1",
        )

        self.logger = logging.getLogger('loki')
        self.logger.addHandler(loki_handler)

        self.logger.info("Loki handler started")

        device_handler = logging_loki.LokiHandler(
            url=f"{LOKI_SERVER}/loki/api/v1/push", 
            tags={"application": "chromatron-logserver"},
            # auth=("username", "password"),
            version="1",
        )

        device_handler.setLevel(logging.DEBUG)
        formatter = logging.Formatter('%(levelname)s %(message)s')
        device_handler.setFormatter(formatter)

        self.device_logger = logging.getLogger('chromatron')
        self.device_logger.addHandler(device_handler)

        self.q = Queue()

        self.start()
    
    def post_msg(self, msg):
        self.q.put(msg)

    def _process(self):
        msg = self.q.get()

        if msg is None:
            return

        host    = msg[0]
        info    = msg[1]
        now     = msg[2]
        log     = msg[3]

        # get log level from log message
        tokens      = log.split(':')
        level       = tokens[0].strip()
        sys_time    = tokens[1].strip()
        source_file = tokens[2].strip()
        source_line = tokens[3].strip()
        log_msg     = tokens[3].strip()

        location = ''
        # early versions of the catbus directory don't have location
        if 'location' in info:
            location = info['location']


        # !!! NOTE
        # all Loki tags must be strings!
        # if you get an error 400, check for that!

        tags = {
            'device_id':    str(info['device_id']),
            'name':         info['name'],
            'host':         host[0],
            'location':     location,
            # 'sys_time':     sys_time,
            'level':        level,
            'source_file':  source_file,
            # 'source_line':  source_line,
        }

        full_log_msg = f"{now.isoformat(timespec='milliseconds')} {info['device_id']:18} {host[0]:15} {info['name']:16} = {log}"

        self.device_logger.log(LOG_LEVEL[level], full_log_msg, extra={'tags': tags})

class LogServer(MsgFlowReceiver):
    def __init__(self):
        super().__init__(service='logserver', port=LOGSERVER_PORT)
        
        self.kv = CatbusService(name='logserver', visible=True, tags=[])

        self.loki = LokiHandler()

        self.update_directory()

        self.start_timer(DIRECTORY_UPDATE_INTERVAL, self.update_directory)

    def update_directory(self):
        c = Client()
        self.directory = c.get_directory()

        self._last_directory_update = time.time()

    def clean_up(self):
        self.loki.stop()
        
        self.kv.stop()
        
        super().clean_up()
        
        self.loki.join()

    def lookup_by_host(self, host):
        for device_id, info in self.directory.items():
            if host[0] == info['host'][0]:
                info['device_id'] = device_id
                return info

    def on_receive(self, host, data):
        # tz_offset = time.localtime().tm_gmtoff

        # get timestamp
        now = datetime.now()
        # utc_now = datetime.utcnow()

        log = data.decode('ascii')
        info = self.lookup_by_host(host)

        if info is None:
            logging.warning(f"Directory information not available for host: {host}")
            return

        self.loki.post_msg((host, info, now, log))

    def on_connect(self, host, device_id=None):
        self.update_directory()

        self.loki.logger.info(f"Connected: {host[0]}:{host[1]} from {device_id}")
        
    def on_disconnect(self, host):
        self.loki.logger.info(f"Disconnected: {host[0]}:{host[1]}")


def main():
    util.setup_basic_logging(console=True, level=logging.DEBUG)

    logserver = LogServer()

    run_all()


if __name__ == '__main__':
    main()
