#
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
#

import sys
import os
import json
import logging

app_dir = os.getcwd()

def get_app_dir():
    return app_dir

_log_levels = {"debug": logging.DEBUG,
               "info": logging.INFO,
               "warning": logging.WARNING,
               "error": logging.ERROR,
               "critical": logging.CRITICAL}
    
##################
# DEFAULT CONFIG #
##################
LOG_FILENAME = os.path.splitext(os.path.split(sys.argv[0])[1])[0] + ".log"
LOG_PATH = get_app_dir()
LOG_LEVEL = "info"
LOG_QUIET = False


###################
# INTERNAL CONFIG #
###################
_SETTINGS_PATH = None
_LOG_FILE_PATH = None
_LOG_LEVEL = logging.INFO
_INITIALIZED = False


def load_config(path=None, override_settings=dict()):
    if not path:
        # try to automatically load a config file

        # start with CWD
        try:
            load_config(path="sapphire.conf", override_settings=override_settings)

        except IOError:
            
            # then try app directory
            try:
                load_config(os.path.join(get_app_dir(), "sapphire.conf"))

            except IOError:
                pass

    else:
        f = open(path, 'r')

        global _SETTINGS_PATH
        _SETTINGS_PATH = path

        try:
            config = json.loads(f.read())
            
            mod_dict = globals()
            for k, v in config.iteritems():
                mod_dict[k] = v

        except ValueError:
            raise

        f.close()

    # apply override settings
    mod_dict = globals()

    for k, v in override_settings.iteritems():
        mod_dict[k] = v


def init_logging():
    # set up logging
    dt_format = '%Y-%m-%dT%H:%M:%S'

    global _LOG_FILE_PATH
    _LOG_FILE_PATH = os.path.join(LOG_PATH, LOG_FILENAME)

    global _LOG_LEVEL
    _LOG_LEVEL = _log_levels[LOG_LEVEL]



    # set up file handler
    logging.basicConfig(filename=_LOG_FILE_PATH, 
                        format='%(asctime)s >>> %(levelname)s %(message)s', 
                        datefmt=dt_format, 
                        level=_LOG_LEVEL)

    global LOG_QUIET
    if not LOG_QUIET:
        # add a console handler to print to the console
        console = logging.StreamHandler()
        console.setLevel(_LOG_LEVEL)
        formatter = logging.Formatter('%(levelname)s %(asctime)s %(message)s', datefmt=dt_format)
        console.setFormatter(formatter)
        logging.getLogger('').addHandler(console)


def init(override_settings=dict()):
    global _INITIALIZED
    
    if _INITIALIZED:
        return

    try:
        load_config(override_settings=override_settings)
        init_logging()

        global _SETTINGS_PATH

        if _SETTINGS_PATH:
            logging.info("Loaded config from: %s" % (_SETTINGS_PATH))

        else:
            logging.info("Loaded default config")

        logging.info("Log level: '%s' to file: %s" % (LOG_LEVEL, _LOG_FILE_PATH))

    except ValueError as e:
        init_logging()

        logging.error("Parse error in config file: %s" % (e))

    _INITIALIZED = True




