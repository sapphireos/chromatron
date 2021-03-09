# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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

from datetime import datetime, timedelta
import logging
import logging.handlers
import socket
import os
import colorlog
from functools import wraps

EPOCH = datetime.utcfromtimestamp(0)
NTP_EPOCH = datetime(1900, 1, 1)


def synchronized(f):
    """Synchronization decorator for methods"""
    @wraps(f)
    def wrapper(self, *args, **kwargs):
        with self._lock:
            return f(self, *args, **kwargs)
    return wrapper


def memoize(f):
    """ Memoization decorator for a function taking a single argument """
    class memodict(dict):
        def __missing__(self, key):
            ret = self[key] = f(key)
            return ret
    return memodict().__getitem__



def iso_to_datetime(iso):
    tries = ["%Y-%m-%dT%H:%M:%S.%f",
             "%Y-%m-%dT%H:%M:%S",
             "%Y-%m-%dT%H:%M:%S.%fZ",
             "%Y-%m-%dT%H:%M:%SZ",
             "%Y-%m-%d %H:%M:%S.%f",
             "%Y-%m-%d %H:%M:%S",
             "%Y-%m-%d %H:%M:%S.%fZ",
             "%Y-%m-%d %H:%M:%SZ"]

    for trie in tries:
        try:
            return datetime.strptime(iso, trie)

        except ValueError:
            pass

    raise ValueError

def coerce_to_list(obj):
    if isinstance(obj ,str):
        return [obj]

    return obj

def unix_time(dt):
    delta = dt - EPOCH
    return delta.total_seconds()


def unix_time_millis(dt):
    return unix_time(dt) * 1000.0

def ts_to_datetime(ts):
    try:
        return datetime.utcfromtimestamp(ts)

    except TypeError:
        # already a datetime object
        return ts

def convert_string_to_more_specific_type(value):
    # try to convert to a more specific type
    if value.lower() == 'true':
        return True

    elif value.lower() == 'false':
        return False

    else:
        try:
            return int(value)

        except ValueError:
            try:
                return float(value)

            except ValueError:
                # leave as string
                return value

def coerce_value_to_target_type(value, target):
    if isinstance(target, type(value)):
        return value

    elif isinstance(target, bool):
        return bool(value)

    elif isinstance(target, int):
        return int(value)

    elif isinstance(target, float):
        return float(value)

    elif isinstance(target, int):
        return int(value)

    elif isinstance(target, str):
        return str(value)

    return value

def now():
    return datetime.utcnow()

def datetime_to_ntp(dt):
    time_diff = dt - NTP_EPOCH
    seconds = int(time_diff.total_seconds())
    fraction = int((time_diff.total_seconds() - seconds) * (pow(2, 32) - 1))

    return seconds, fraction

def ntp_to_datetime(seconds, fraction):
    fraction = float(fraction) / (pow(2, 32) - 1)
    seconds = float(seconds) + fraction

    delta = timedelta(seconds=seconds)

    return delta + NTP_EPOCH

def datetime_to_microseconds(dt):
    return int((dt - NTP_EPOCH).total_seconds() * 1000000)

def microseconds_to_datetime(ms):
    return timedelta(microseconds=ms) + NTP_EPOCH

def hostname():
    socket.gethostname()

def ip():
    return socket.gethostbyname(socket.gethostname())

def linear_interp(x, x0, x1, y0, y1):
    if x > x1:
        x = x1

    elif x < x0:
        x = x0

    y = y0 + (y1 - y0)*((x - x0) / (x1 - x0))

    return y

def coerce_string_to_list(s):
    if isinstance(s, str):
        return [s]

    return s

def is_wildcard(key):
        if '*' in key or \
           '?' in key or \
           '[' in key:
           return True

        return False


logging_initalized = False

def setup_basic_logging(console=True, filename=None, level=logging.DEBUG):
    global logging_initalized

    if logging_initalized:
        return

    logging_initalized = True

    import colored_traceback
    colored_traceback.add_hook()

    dt_format = '%Y-%m-%dT%H:%M:%S'

    root = colorlog.getLogger('')
    root.setLevel(level)

    if console:
        handler = colorlog.StreamHandler()
        handler.setLevel(level)
        formatter = colorlog.ColoredFormatter('%(log_color)s%(levelname)s %(blue)s%(asctime)s.%(msecs)03d %(yellow)s[%(thread)d] %(purple)s%(module)s %(white)s%(message)s', 
                                                datefmt=dt_format,
                                                log_colors={
                                                    'DEBUG':    'cyan',
                                                    'INFO':     'green',
                                                    'WARNING':  'yellow',
                                                    'ERROR':    'red',
                                                    'CRITICAL': 'red,bg_white',
                                                })
        handler.setFormatter(formatter)
        root.addHandler(handler)

    if filename:
        # get path
        path, name = os.path.split(filename)

        if len(path) > 0:
            # if directory does not exist
            if not os.path.exists(path):
                os.makedirs(path)

        handler = logging.handlers.RotatingFileHandler(filename, maxBytes=32*1048576, backupCount=4)
        handler.setLevel(level)
        formatter = logging.Formatter('%(levelname)s %(asctime)s.%(msecs)03d %(module)s %(message)s', datefmt=dt_format)
        handler.setFormatter(formatter)
        root.addHandler(handler)

    if console:
        logging.debug("Console logging enabled")

    # create a network logger
    # socket_handler = logging.handlers.SocketHandler('localhost',
    #                     logging.handlers.DEFAULT_TCP_LOGGING_PORT)

    # root.addHandler(socket_handler)
