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

import time
import threading
import logging
import inspect
from Queue import Queue, Empty

class MsgQueueEmptyException(Exception):
    pass

class PositionalArgumentsNotSupported(Exception):
    pass


class Ribbon(threading.Thread):

    __lock = threading.Lock()
    __ribbons = []

    @classmethod
    def shutdown(cls, timeout=10.0):
        logging.info("Ribbon: shutting down all ribbons")

        with cls.__lock:
            for r in cls.__ribbons:
                r.stop()

        for i in xrange(int(timeout)):
            alive = False

            with cls.__lock:
                for r in cls.__ribbons:
                    if r.is_alive():
                        alive = True
                        break

            if alive:
                time.sleep(1.0)

            else:
                break

        with cls.__lock:
            for r in cls.__ribbons:
                if r.is_alive():
                    logging.error("Ribbon: %s failed to shut down" % (r.name))

    @classmethod
    def count(cls):
        return len(cls.__ribbons)

    def __init__(self,
                 name=None,
                 initialize_func=None,
                 clean_up_func=None,
                 loop_func=None,
                 auto_start=True,
                 queue=None,
                 **kwargs):
        super(Ribbon, self).__init__()

        if name:
            self.name = name

        self._stop_event = threading.Event()

        if initialize_func:
            self.initialize = initialize_func

        if clean_up_func:
            self.clean_up = clean_up_func

        if loop_func:
            self.loop = loop_func

        if queue:
            self.queue = queue

        else:
            self.queue = Queue()

        self.daemon = True

        with self.__lock:
            self.__ribbons.append(self)

        self._kwargs = kwargs

        try:
            # build dict of default kwargs for method
            argspec = inspect.getargspec(self.initialize)

            method_args = argspec.args[1:]
            defaults = argspec.defaults
            method_kwargs = {}
            for i in xrange(len(method_args)):
                try:
                    method_kwargs[method_args[i]] = defaults[i]

                except IndexError:
                    # this occurs if we pass a non-keyword argument
                    raise PositionalArgumentsNotSupported

            # now, override method kwargs
            for k, v in self._kwargs.iteritems():
                if k in method_kwargs:
                    method_kwargs[k] = v

            self.initialize(**method_kwargs)

            if auto_start:
                self.start()

        except Exception as e:
            logging.exception("Ribbon: %s failed to initialize with: %s" % (self.name, e))
            raise

    def start(self):
        super(Ribbon, self).start()

    def initialize(self, **kwargs):
        pass

    def clean_up(self):
        pass

    def loop(self):
        time.sleep(1.0)

    def run(self):
        # append thread ID to name
        self.name += '.%d' % (self.ident)

        logging.info("Ribbon: %s started" % (self.name))

        while not self._stop_event.is_set():
            try:
                self.loop()

            except Exception as e:
                logging.exception("Ribbon: %s unexpected exception: %s" % (self.name, e))
                time.sleep(2.0)


        try:
            self.clean_up()

        except Exception as e:
            logging.exception("Ribbon: %s failed to clean up with: %s" % (self.name, e))

        with self.__lock:
            self.__ribbons.remove(self)

        logging.info("Ribbon: %s stopped" % (self.name))

    def wait(self, timeout):
        self._stop_event.wait(timeout)

    def join(self, timeout=10.0):
        super(Ribbon, self).join(timeout)

        if self.is_alive():
            logging.info("Ribbon: %s join timed out" % (self.name))

    def stop(self):
        if not self._stop_event.is_set():
            logging.info("Ribbon: %s shutting down" % (self.name))

            self._stop_event.set()

    def post_msg(self, msg):
        self.queue.put(msg)

    def recv_msg(self, timeout=None):
        if timeout == None:
            q_timeout = 4.0
        else:
            q_timeout = timeout

        while not self._stop_event.is_set():
            try:
                return self.queue.get(timeout=q_timeout)

            except Empty:
                if timeout:
                    raise MsgQueueEmptyException

    def recv_all_msgs(self, timeout=None):
        if timeout == None:
            q_timeout = 4.0
        else:
            q_timeout = timeout

        msgs = []

        while not self._stop_event.is_set():
            try:
                msgs = [self.queue.get(timeout=q_timeout)]
                break

            except Empty:
                if timeout:
                    raise MsgQueueEmptyException

        qsize = self.queue.qsize()

        for i in xrange(qsize):
            msgs.append(self.queue.get())

        return msgs

def wait_for_signal():
    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

