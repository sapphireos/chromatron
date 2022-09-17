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

import time
import threading
import logging
import inspect
from .util import synchronized

class RibbonShutdown(Exception):
    pass

class Ribbon(threading.Thread):
    _global_lock = threading.Lock()
    _ribbon_id = 0
    _ribbons = []

    def __init__(self, name='ribbon', auto_start=False, suppress_logs=False):
        super().__init__()

        self._lock = threading.RLock()
        self._stop_event = threading.Event()

        self.name = name + f'.{self._ribbon_id}'

        with Ribbon._global_lock:
            Ribbon._ribbon_id += 1
            Ribbon._ribbons.append(self)

        self.daemon = True
        self._suppress_logs = suppress_logs

        if not self._suppress_logs:
            logging.debug(f"Started Ribbon {self.name}")

        if auto_start:
            self.start()

    def __str__(self):
        return f'Ribbon: {self.name}'
        
    def _process(self):
        time.sleep(1.0)

    def run(self):
        while not self._stop_event.is_set():
            try:
                self._process()

            except RibbonShutdown:
                break

            except Exception as e:
                logging.exception("Unhandled exception in {self.name}: {e}")

        self.clean_up()
        
        if not self._suppress_logs:
            logging.debug(f"Ribbon {self.name}: shut down complete")

    def wait(self, interval):
        self._stop_event.wait(interval)

        if self._stop_event.is_set():
            raise RibbonShutdown # use an exception to make sure nothing happens in process

    @synchronized
    def _shutdown(self):
        if self._stop_event.is_set():
            return
      
        self._stop_event.set()

    @synchronized
    def stop(self):
        self._shutdown()
        
        if not self._suppress_logs:
            logging.debug(f"Ribbon {self.name}: shutting down")
        
    def clean_up(self):
        pass

def stop_all():
    logging.info("Stopping all...")
    with Ribbon._global_lock:
        for s in Ribbon._ribbons:
            s.stop()

        for s in Ribbon._ribbons:
            s.join()

def run_all():
    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

    stop_all()
