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

import util
from ribbon import Ribbon

import json
import logging
import signal
import sys
import os
import time
import Queue


def load_settings(filename='settings.json'):
    settings = {}

    try:
        with open(filename, 'r') as f:
            settings = json.loads(f.read())

    except IOError:
        pass

    return settings


class App(object):
    def __init__(self,
                 name=None,
                 enable_logging=True,
                 max_log_entries=256,
                 host=None):

        self.settings = load_settings()

        if enable_logging:
            log_filename = None

            if name:
                log_filename = '%s.log' % (name)

                if 'LOG_HOME' in self.settings:
                    log_filename = os.path.join(self.settings['LOG_HOME'], log_filename)

            util.setup_basic_logging(filename=log_filename)


        signal.signal(signal.SIGTERM, self.sigterm_handler)

        try:
            self.version = self.settings['VERSION']

        except KeyError:
            self.version = '0.0.0'

        self.process_id = os.getpid()
        self.hostname = util.hostname()
        self.ip = util.ip()
        self.command = sys.argv[0]

        self.log_entries = []
        self.max_log_entries = max_log_entries

        if name:
            self.name = name

        else:
            self.name = "%d@%s" % (self.process_id, self.hostname)

        logging.info("Starting app: %s" % (self.name))
        logging.info("v: %s" % (self.version))
        logging.info("Process ID: %d" % (self.process_id))

    def to_dict(self):
        d = {
            'name': self.name,
            'command': self.command,
            'process_id': self.process_id,
            'hostname': self.hostname,
            'ip': self.ip,
        }

        return d

    def sigterm_handler(self, signum, frame):
        logging.info("Received SIGTERM")
        sys.exit()

    def stop(self):
        # shutdown all running ribbons
        Ribbon.shutdown()

        logging.info("Shutdown complete")

    def run(self):
        try:
            while True:
                try:
                    pass

                except Exception as e:
                    logging.exception("App: unexpected exception: %s" % (e))

                time.sleep(1.0)

        except KeyboardInterrupt:
            logging.info("Received KeyboardInterrupt")

        except SystemExit:
            pass

        try:
            logging.debug("Shutting down...")

            self.stop()

        except Exception as e:
            logging.exception("App: unexpected exception on shutdown: %s" % (e))


class test(Ribbon):
    def loop(self):
        self.wait(5.0)
        logging.info("stuff")


if __name__ == "__main__":
    app = App()

    r = test()

    app.run()
