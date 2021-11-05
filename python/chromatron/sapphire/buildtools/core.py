#
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
#

import os
import sys
import shutil
import logging
import json
import multiprocessing
import subprocess
import shlex
import uuid
import crcmod
import struct
import argparse
import hashlib
import urllib.request, urllib.parse, urllib.error
import tarfile
import zipfile
from datetime import datetime
import configparser
from pprint import pprint
from . import firmware_package
from .firmware_package import FirmwarePackage, get_build_package_dir, get_firmware_package, FirmwarePackageNotFound
from . import esptool
from io import StringIO

from . import settings
from . import serial_monitor

from intelhex import IntelHex

from sapphire.devices.sapphiredata import KVMetaField, KVMetaFieldWidePtr
from sapphire.common import util, catbus_string_hash
from fnvhash import fnv1a_32


CHROMATRON_ESP_UPGRADE_FWID = '4b2e4ce5-1f41-494e-8edd-d748c7c81dcb'.replace('-', '')


class SettingsParseException(Exception):
    pass

class ProjectNotFoundException(Exception):
    pass

class AppZipNotFound(Exception):
    pass


KV_PREFIX = '__KV__'
lowercase = list('abcdefghijklmnopqrstuvwxyz')
uppercase = list('ABCDEFGHIJKLMNOPQRSTUVWXYZ')
digits = list('1234567890')
KV_CHARS = lowercase
KV_CHARS.extend(uppercase)
KV_CHARS.extend(['_'])
KV_CHARS.extend(digits)
# KV_HEADER = '_kv_hashes.h'
    
SETTINGS_DIR                = os.path.dirname(os.path.abspath(settings.__file__))
SETTINGS_FILE               = 'settings.json'
PROJECTS_FILE_NAME          = 'projects.json'
PROJECTS_FILE_PATH          = firmware_package.data_dir()
PROJECTS_FILE               = os.path.join(PROJECTS_FILE_PATH, PROJECTS_FILE_NAME)
BUILD_CONFIGS_DIR           = os.path.join(os.getcwd(), 'configurations')
BUILD_CONFIGS_FWID_FILE     = os.path.join(BUILD_CONFIGS_DIR, 'fwid.json')
BASE_DIR                    = os.getcwd()
TARGETS_DIR                 = os.path.join(BASE_DIR, 'targets')
BOARDS_FILE                 = os.path.join(TARGETS_DIR, 'boards.json')
MASTER_HASH_DB_FILE         = os.path.join(PROJECTS_FILE_PATH, 'kv_hashes.json')
TOOLS_DIR                   = os.path.join(PROJECTS_FILE_PATH, 'tools')
LIB_INIT_FILENAME           = '__libs.c'
FX_COMPILER_COMMAND         = 'chromatron compile'


tools_linux = [
    ('http://downloads.arduino.cc/tools/', 'avrdude-6.0.1-arduino5-x86_64-pc-linux-gnu.tar.bz2'),
    ('http://downloads.arduino.cc/tools/', 'avr-gcc-4.9.2-atmel3.5.3-arduino2-x86_64-pc-linux-gnu.tar.bz2')
]

tools_darwin = [
    ('http://downloads.arduino.cc/tools/', 'avrdude-6.0.1-arduino5-i386-apple-darwin11.tar.bz2'),
    ('http://downloads.arduino.cc/tools/', 'avr-gcc-4.9.2-atmel3.5.3-arduino2-i386-apple-darwin11.tar.bz2')
]

tools_windows = [
    ('http://downloads.arduino.cc/tools/', 'avrdude-6.0.1-arduino5-i686-mingw32.zip'),
    ('http://downloads.arduino.cc/tools/', 'avr-gcc-4.9.2-atmel3.5.3-arduino2-i686-mingw32.zip')
]

build_tool_archives = {
    'linux': tools_linux,
    'linux2': tools_linux,
    'darwin': tools_darwin,
    'win32': tools_windows
}

def get_tools():
    try:
        os.makedirs(TOOLS_DIR)

    except OSError:
        pass # already exists

    cwd = os.getcwd()

    os.chdir(TOOLS_DIR)

    for tool in build_tool_archives[sys.platform]:
        urllib.request.URLopener().retrieve(tool[0] + tool[1], tool[1])

        if tool[1].endswith('.tar.bz2'):
            tar = tarfile.open(tool[1])
            tar.extractall()
            tar.close()

        elif tool[1].endswith('.zip'):
            zf = zipfile.ZipFile(tool[1])
            zf.extractall()
            zf.close()

        else:
            raise Exception("no decompressor available")

    os.chdir(cwd)

def runcmd(cmd, tofile=False, tolog=True):
    logging.debug(cmd)

    try:
        p = subprocess.Popen(shlex.split(cmd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    except OSError as e:
        logging.error("Error running command: %s" % (cmd))
        raise

    output = ''

    for line in p.stdout.readlines():
        output += line.decode('utf-8')

    if output != '':
        if not tofile and tolog:
            logging.warn("\n" + output)

        elif tofile:
           f = open(tofile, 'w')
           f.write(output)
           f.close()

    p.wait()

    return output


def get_builder(target_dir, board_type, build_loader=False, fnv_hash=True, **kwargs):
    builder = Builder(target_dir, board_type, fnv_hash=fnv_hash, **kwargs)

    modes = {"os": OSBuilder, 
             "loader": LoaderBuilder, 
             "arm_loader": ARMLoaderBuilder, 
             "esp8266_loader": ESP8266LoaderBuilder,
             "esp32_loader": ESP32LoaderBuilder,
             "app": AppBuilder, 
             "lib": LibBuilder, 
             "exe": ExeBuilder}

    if builder.settings["BUILD_TYPE"] not in modes:
        raise Exception("Unknown build type: %s" % (builder.settings["BUILD_TYPE"]))


    return modes[builder.settings["BUILD_TYPE"]](target_dir, board_type, build_loader=build_loader, fnv_hash=fnv_hash)

class Builder(object):
    def __init__(self, target_dir, board_type=None, build_loader=False, fnv_hash=True, defines=[]):
        self.target_dir = target_dir
        self.board_type = board_type

        self.build_loader = build_loader

        try:
            with open(BOARDS_FILE, 'r') as f:
                boards = json.loads(f.read())

        except FileNotFoundError:
            print("No board targets found!")
            boards = {}

        try:
            self.board = boards[self.board_type]

        except KeyError:
            self.board = {}

        try:
            self.settings = self.get_settings()

        except (AttributeError, FileNotFoundError):
            self.settings = {}

        try:
            self.proj_name = str(self.settings["PROJ_NAME"]) # convert from unicode

        except KeyError:
            self.proj_name = None

        try:
            self.fwid = self.settings["FWID"]

        except KeyError:
            self.fwid = None

        if fnv_hash:
            self.settings["HASH"] = "fnv"

        else:
            self.settings["HASH"] = "kv"            

        try:
            self.libraries = self.settings["LIBRARIES"]
        except KeyError:
            self.libraries = list()

        try:
            self.includes = self.settings["INCLUDES"]
        except KeyError:
            self.includes = list()

        try:
            self.defines = self.settings["DEFINES"]
        except KeyError:
            self.defines = list()

        self.defines.extend(defines)

        # load defines from board settings
        if 'defines' in self.board:
            self.defines.extend(self.board['defines'])

        if self.build_loader:
            self.defines.append("BOOTLOADER")

        try:
            self.source_dirs = self.settings["EXTRA_SOURCE_DIRS"]
        except KeyError:
            self.source_dirs = list()

        # check if recursive build
        try:
            if bool(self.settings["RECURSIVE_BUILD"]):
                for root, dirs, files in os.walk(self.target_dir):
                    self.source_dirs.append(root)

        except KeyError:
            pass

        # add target dir
        self.source_dirs.append(self.target_dir)

        # skip dirs
        try:
            self.skip_dirs = [os.path.join(self.target_dir, a) for a in self.settings["SKIP_DIRS"]]
            
        except KeyError:
            self.skip_dirs = []

    def get_tools(self):
        tools = build_tool_archives[sys.platform]

    def init_logging(self):
        try:
            os.remove(self.settings["LOG_FILENAME"])

        except:
            pass

        file_logger = logging.FileHandler(filename=self.settings["LOG_FILENAME"])
        file_logger.setLevel(logging.DEBUG)
        formatter = logging.Formatter('%(asctime)s >>> %(levelname)s %(message)s', datefmt='%m/%d/%Y %I:%M:%S %p')
        file_logger.setFormatter(formatter)
        logging.getLogger('').addHandler(file_logger)

    def get_settings(self):
        app_settings = self._get_local_settings()

        # check if app settings override target type
        if "TARGET" in app_settings:
            self.board_type = app_settings["TARGET"]

        try:
            self.settings_dir = os.path.join(TARGETS_DIR, self.board["target"])

        except KeyError:
            self.settings_dir = self.target_dir

        settings = self._get_default_settings()

        # override defaults with app settings
        for k, v in app_settings.items():
            if k == "LIBRARIES":
                settings[k].extend(v)

            else:
                settings[k] = v

        if "LINKER_SCRIPT" not in settings:
            settings["LINKER_SCRIPT"] = "linker.x"

        if "TOOLCHAIN" not in settings:
            settings["TOOLCHAIN"] = "AVR"

        if settings["TOOLCHAIN"] == "AVR":
            if "CC" not in settings:
                settings["CC"] = os.path.join(TOOLS_DIR, 'avr', 'bin', 'avr-gcc')

            if "BINTOOLS" not in settings:
                settings["BINTOOLS"] = os.path.join(TOOLS_DIR, 'avr', 'bin')

        elif settings["TOOLCHAIN"] == "ARM":
            if "CC" not in settings:
                settings["CC"] = os.path.join(TOOLS_DIR, 'arm', 'bin', 'arm-none-eabi-gcc')

            if "BINTOOLS" not in settings:
                settings["BINTOOLS"] = os.path.join(TOOLS_DIR, 'arm', 'bin')

        elif settings["TOOLCHAIN"] == "XTENSA":
            if "CC" not in settings:
                settings["CC"] = os.path.join(TOOLS_DIR, 'xtensa', 'bin', 'xtensa-lx106-elf-gcc')

            if "BINTOOLS" not in settings:
                settings["BINTOOLS"] = os.path.join(TOOLS_DIR, 'xtensa', 'bin')

        elif settings["TOOLCHAIN"] == "ESP32":
            if "CC" not in settings:
                settings["CC"] = os.path.join(TOOLS_DIR, 'xtensa-esp32-elf', 'bin', 'xtensa-esp32-elf-gcc')

            if "BINTOOLS" not in settings:
                settings["BINTOOLS"] = os.path.join(TOOLS_DIR, 'xtensa-esp32-elf', 'bin')

        else:
            raise SettingsParseException("Unknown toolchain")

        if 'FULL_NAME' not in settings:
            try:
                settings['FULL_NAME'] = settings['PROJ_NAME']

            except KeyError:
                settings['FULL_NAME'] = 'unknown'

        return settings

    def _get_default_settings(self):
        try:
            return json.loads(open(os.path.join(self.settings_dir, SETTINGS_FILE)).read())

        except ValueError:
            raise SettingsParseException

    def _get_local_settings(self):
        # load app settings file
        try:
            f = open(os.path.join(self.target_dir, SETTINGS_FILE))

        # file does not exist
        except IOError:
            return dict()

        try:
            # parse settings
            app_settings = json.loads(f.read())

        except ValueError:
            raise SettingsParseException

        return app_settings

    # wrap os.listdir so we can handle errors.
    # sometimes if we do a clean right before a build, we get directories in the source_dirs
    # list that have just been deleted.
    def listdir(self, d):
        try:
            return os.listdir(d)

        except OSError:
            return []

    def list_fx_source(self):
        fx_files = []

        for d in self.source_dirs:
            for f in self.listdir(d):
                if f.endswith('.fx'):
                    fpath = os.path.join(d, f)
                    # prevent duplicates
                    if fpath not in fx_files:
                        fx_files.append(fpath)
            
        return fx_files

    def list_source(self):
        source_files = []

        for d in self.source_dirs:
            for f in self.listdir(d):
                if f.endswith('.c') or f.endswith('.cpp') or f.endswith('.s') or f.endswith('.S'):
                    fpath = os.path.join(d, f)
                    # prevent duplicates
                    if fpath not in source_files:
                        source_files.append(fpath)
            
        return source_files

    def list_headers(self):
        source_files = []

        for d in self.source_dirs:
            for f in self.listdir(d):
                if f.endswith('.h'):
                    fpath = os.path.join(d, f)
                    # prevent duplicates
                    if fpath not in source_files:
                        source_files.append(fpath)

        return source_files

    def create_source_hash(self):
        source_files = self.list_source()
        hashed_files = {}

        for fname in source_files:
            m = hashlib.sha256()
            with open(fname, 'rb') as f:
                m.update(f.read())
            hashed_files[fname] = m.hexdigest()

        return hashed_files

    def update_source_hash_file(self):
        hash_file = open("source_hash.json", 'w+')

        hash_file.write(json.dumps(self.create_source_hash()))
        hash_file.close()

    def get_source_hash_file(self):
        try:
            hash_data = open("source_hash.json", 'r').read()

        except IOError:
            return {}

        try:
            return json.loads(hash_data)

        except json.decoder.JSONDecodeError:
            return {}

    def get_buildnumber(self):
        return runcmd('git rev-parse --short HEAD', tolog=False).strip()


    def set_buildnumber(self, value):
        pass
       
    buildnumber = property(get_buildnumber, set_buildnumber)

    def get_version(self):
        s = "%s.%s" % (str(self.settings["PROJ_VERSION"]), self.buildnumber)
        return s

    version = property(get_version)

    def scan_file_for_kv(self, filename):
        try:
            with open(filename, 'r') as f:
                data = f.read()

        except UnicodeDecodeError:
            try:
                with open(filename, 'r', encoding='latin1') as f:
                    data = f.read()

            except UnicodeDecodeError:
                print(filename)
                raise

        hashes = {}

        # search for prefix
        index = data.find(KV_PREFIX)

        while index >= 0:

            # scan to end
            terminal_index = index + len(KV_PREFIX)

            while terminal_index < len(data):
                if data[terminal_index] not in KV_CHARS:
                    break

                terminal_index += 1

            kv_string = data[index + len(KV_PREFIX):terminal_index]
            define_string = KV_PREFIX + kv_string

            hashes[define_string] = catbus_string_hash(kv_string)

            index = data.find(KV_PREFIX, index + 1)

        return hashes

    def create_kv_hashes(self):
        files = self.list_source()
        files.extend(self.list_headers())

        # process source in all included projects as well
        for include in self.includes:
            b = get_project_builder(include, target=self.board_type)

            files.extend(b.list_source())
            files.extend(b.list_headers())

        for lib in self.libraries:
            b = get_project_builder(lib, target=self.board_type)

            files.extend(b.list_source())
            files.extend(b.list_headers())

        hashes = {}

        for f in files:
            hashes.update(self.scan_file_for_kv(f))
        
        # update hashes in master file
        db_hashes = {}

        try:
            with open(MASTER_HASH_DB_FILE, 'r') as f:
                try:
                    db_hashes.update(json.loads(f.read()))

                except ValueError:
                    pass

        except IOError:
            pass

        db_hashes.update(hashes)

        with open(MASTER_HASH_DB_FILE, 'w+') as f:
            f.write(json.dumps(db_hashes, indent=4, separators=(',', ': '))) 


        return hashes

    def clean(self):
        logging.info('Cleaning %s' % (self.target_dir))

        try:
            os.remove(os.path.join(self.target_dir, 'source_hash.json'))

        except OSError:
            pass

        for d in self.source_dirs:
            try:
                os.remove(os.path.join(d, LIB_INIT_FILENAME))

            except OSError:
                pass

        try:
            os.remove(os.path.join(self.target_dir, firmware_package.MANIFEST_FILENAME))

        except OSError:
            pass

        for f in os.listdir(self.target_dir):
            for filetype in self.settings["CLEAN_FILES"]:
                if f.lower().endswith(filetype):
                    try:
                        os.remove(os.path.join(self.target_dir, f))

                        logging.info('> Removing %s' % (f))

                    except:
                        raise

            if f in self.settings["CLEAN_DIRS"]:
                try:
                    shutil.rmtree(os.path.join(self.target_dir, f), True)
                    logging.info('> Removing %s' % (f))

                except:
                    raise

    def build(self):
        logging.info("Building %s for board: %s target: %s" % (self.settings["PROJ_NAME"], self.board_type, self.board["target"]))
        logging.info("Toolchain: %s" % (self.settings["TOOLCHAIN"]))

        self.pre_process()
        self.compile()
        self.link()
        self.post_process()

        save_project_info(self.proj_name, self.target_dir)

    def compile_fx(self):
        fx_files = self.list_fx_source()

        current_dir = os.getcwd()

        for fx in fx_files:
            d, f = os.path.split(fx)
            output_file = f + 'b'

            os.chdir(d)

            logging.info("Compiling FX: %s" % (f))            

            cmd = f'{FX_COMPILER_COMMAND} {f}'
            os.system(cmd)

            # convert to C array
            with open(output_file, 'rb') as fxb:
                data = fxb.read()

            with open(f'{f}.carray', 'w') as fxb:
                for b in data:
                    fxb.write(f'{hex(b)},\n')

        os.chdir(current_dir)

    def pre_process(self):
        # get KV hashes and add to defines
        hashes = self.create_kv_hashes()

        for k, v in hashes.items():
            self.defines.append('%s=((catbus_hash_t32)%s)' % (k, v))

        # compile source FX files
        self.compile_fx()

    def get_libraries(self, current_libs={}):
        libs = current_libs

        for lib in self.libraries:
            if lib in current_libs:
                continue

            libs[lib] = get_project_builder(lib, target=self.board_type)

            libs.update(libs[lib].get_libraries(current_libs=libs))

        return libs

    def build_libs(self):
        libs = self.get_libraries()

        for lib, builder in libs.items():
            logging.info("Building library %s" % (lib))
        
            builder.clean()
            builder.build()

    def compile(self):
        logging.info("Compiling %s" % (self.proj_name))

        # add colored diagnostic messages:
        self.settings["C_FLAGS"].append("-fdiagnostics-color=always")

        # save working dir
        cwd = os.getcwd()

        # change to target dir
        os.chdir(self.target_dir)

        try:
            os.mkdir(self.settings["OBJ_DIR"])

        except OSError:
            pass

        try:
            os.mkdir(self.settings["DEP_DIR"])

        except OSError:
            pass

        multicore = True

        if multicore:
            pool = multiprocessing.Pool()

        current_hash = self.create_source_hash()
        last_hash = self.get_source_hash_file()

        source_files = self.list_source()

        filtered_source_files = [f for f in source_files if f not in last_hash or last_hash[f] != current_hash[f]]

        for source_file in filtered_source_files:

            # get dir and filename components
            source_path, source_fname = os.path.split(source_file)

            # this is a bit of a hack to get somewhat sane source file
            # names in the __FILE__ macro, but still work with
            # the recursive build option.
            compile_path = source_file

            if source_path == os.getcwd():
                compile_path = source_fname

            logging.info("Compiling file %s" % (compile_path))

            # build command string
            cmd = '"' + self.settings["CC"] + '"'
                        
            if source_file.endswith('.s'):
                cmd += " -x assembler-with-cpp"

            cmd += ' -c %s ' % (compile_path)

            for include in self.includes:
                include_dir = get_project_builder(include, target=self.board_type).target_dir

                cmd += '-I%s ' % (include_dir)

            for include in self.libraries:
                lib_proj = get_project_builder(include, target=self.board_type)
                include_dirs = [lib_proj.target_dir]
                include_dirs.extend(lib_proj.source_dirs)

                # process directories to skip
                skips = []
                for d in include_dirs:
                    for skip in self.skip_dirs:
                        if skip in d:
                            skips.append(d)

                include_dirs = [a for a in include_dirs if a not in skips]

                cmd += '-D%s ' % (include.upper())

                for i in include_dirs:
                    cmd += '-I%s ' % (i)

            # add extra source dirs to include
            for source_dir in self.source_dirs:
                cmd += '-I%s ' % (source_dir)

            for define in self.defines:

                cmd += '-D%s ' % (define)

            for flag in self.settings["C_FLAGS"]:
                cmd += flag + ' '

            if self.settings["TOOLCHAIN"] not in  ["XTENSA", "ESP32"]:
                cmd += '%(DEP_DIR)/%(SOURCE_FNAME).o.d' + ' '

            cmd += '-o ' + '%(OBJ_DIR)/%(SOURCE_FNAME).o' + ' '

            cmd = cmd.replace('%(OBJ_DIR)', self.settings["OBJ_DIR"])
            cmd = cmd.replace('%(SOURCE_FNAME)', source_fname)
            cmd = cmd.replace('%(BASE_DIR)', BASE_DIR)

            if self.settings["TOOLCHAIN"] not in  ["XTENSA", "ESP32"]:
                cmd = cmd.replace('%(DEP_DIR)', self.settings["DEP_DIR"])

            # replace windows path separators with unix
            # cmd = cmd.replace('\\', '/')

            if multicore:
                pool.apply_async(runcmd, args = (cmd, ))

            else:
                runcmd(cmd)

        if multicore:
            pool.close()
            pool.join()

        # update hash
        self.update_source_hash_file()

        # change back to working dir
        os.chdir(cwd)

    def link(self):
        pass

    def post_process(self):
        pass


class ConfigBuilder(Builder):
    def __init__(self, *args, **kwargs):
        self.build_config = kwargs['build_config']
        del kwargs['build_config']

        super(ConfigBuilder, self).__init__(os.getcwd(), **kwargs)

        self.proj_name = self.build_config['PROJ_NAME']
        self.fwid = self.build_config['FWID']

        self.app_builder = get_project_builder(self.build_config['BASE_PROJ'], target=self.board_type)
            
        # change app's FWID to the config FWID
        self.app_builder.settings['FWID'] = self.fwid
        
        self.app_builder.settings['PROJ_NAME'] = self.build_config['PROJ_NAME']
        self.app_builder.settings['PROJ_VERSION'] = self.build_config['PROJ_VERSION']
        self.app_builder.settings['FULL_NAME'] = self.build_config['FULL_NAME']

        self.lib_builders = []
        for lib in self.build_config['LIBS']:
            builder = get_project_builder(lib, target=self.board_type)

            self.lib_builders.append(builder)

    def clean(self):
        for builder in self.lib_builders:
            builder.clean()
        
        super(ConfigBuilder, self).clean()

    def build(self):
        logging.info("Building configuration %s for target: %s" % (self.proj_name, self.board_type))

        includes = []
        inits = []

        # build libraries
        for builder in self.lib_builders:
            builder.build()
            
            includes.extend(builder.settings["LIB_INCLUDES"])
            inits.append(builder.settings["LIB_INIT"])
            
            self.app_builder.libraries.append(builder.proj_name)


        self.app_builder.clean()

        lib_init_filename = os.path.join(self.app_builder.target_dir, LIB_INIT_FILENAME)

        lib_init = """
#include "sapphire.h"
"""
    
        for include in includes:
            lib_init += '\n#include "%s"\n' % (include)

        lib_init += """

void libs_v_init( void ){

"""

        for init in inits:
            if len(init) > 0:
                lib_init += '\n%s();\n' % (init)

        lib_init += '\n}\n'

        # create lib init file
        with open(lib_init_filename, 'w+') as f:
            f.write(lib_init)
        
        try:
            self.app_builder.build()

        except AppZipNotFound: # not a problem in config-driven builds
            pass

        # make sure we have the firmware package dir
        try:
            os.makedirs(get_build_package_dir())

        except OSError:
            pass

        # update build date
        with open(os.path.join(get_build_package_dir(), firmware_package.PUBLISHED_AT_FILENAME), 'w') as f:
            f.write(util.now().isoformat())


class LibBuilder(Builder):
    def __init__(self, *args, **kwargs):
        super(LibBuilder, self).__init__(*args, **kwargs)

        try:
            self.includes.append(self.settings["OS_PROJECT"])
        except KeyError:
            pass

class OSBuilder(LibBuilder):
    def __init__(self, *args, **kwargs):
        super(OSBuilder, self).__init__(*args, **kwargs)

class HexBuilder(Builder):
    def __init__(self, *args, **kwargs):
        super(HexBuilder, self).__init__(*args, **kwargs)

    def link(self):
        logging.info("Linking %s" % (self.proj_name))

        # build command string
        # enclose in quotes so we can handle spaces in the command filepath
        cmd = '"' + self.settings["CC"] + '"'
        cmd += ' '

        for flag in self.settings["C_FLAGS"]:
            cmd += flag + ' '

        if self.settings["TOOLCHAIN"] not in ["XTENSA", "ESP32"]:
            cmd += '%(DEP_DIR)/%(SOURCE_FNAME).o.d' + ' '

        # save working dir
        cwd = os.getcwd()

        # change to target dir
        os.chdir(self.target_dir)

        obj_dir = self.settings["OBJ_DIR"]

        source_files = self.list_source()

        # source object files
        for source in source_files:
            source_path, source_fname = os.path.split(source)
            cmd += obj_dir + '/' + source_fname + '.o' + ' '

        libraries = self.get_libraries()
        # included libraries
        for lib in libraries:
            lib_project = get_project_builder(lib, target=self.board_type)

            # source object files
            for source in lib_project.list_source():
                source_path, source_fname = os.path.split(source)
                cmd += lib_project.target_dir + '/' + obj_dir + '/' + source_fname + '.o' + ' '

        lib_s = ' '.join(libraries)
        logging.info("Libraries: %s" % (lib_s))

        # linker flags
        for flag in self.settings["LINK_FLAGS"]:
            cmd += flag + ' '

        cmd = cmd.replace('%(OBJ_DIR)', obj_dir)
        if self.settings["TOOLCHAIN"] not in ["XTENSA", "ESP32"]:
            cmd = cmd.replace('%(DEP_DIR)', self.settings["DEP_DIR"])
        # cmd = cmd.replace('%(SOURCE_FNAME)', self.proj_name)
        cmd = cmd.replace("%(LINKER_SCRIPT)", os.path.join(self.settings_dir, self.settings["LINKER_SCRIPT"]))
        cmd = cmd.replace("%(APP_NAME)", self.settings["PROJ_NAME"])
        cmd = cmd.replace("%(TARGET_DIR)", self.target_dir)
        cmd = cmd.replace('%(BASE_DIR)', BASE_DIR)

        # replace windows path separators with unix
        cmd = cmd.replace('\\', '/')

        runcmd(cmd)

        logging.info("Generating output files")


        # enclose in quotes so we can handle spaces in the command filepath
        bintools = '"' + self.settings["BINTOOLS"] + '"'

        if self.settings["TOOLCHAIN"] == "AVR":
            runcmd(os.path.join(bintools, 'avr-objcopy -O ihex -R .eeprom main.elf main.hex'))
            runcmd(os.path.join(bintools, 'avr-size -B main.elf'))
            runcmd(os.path.join(bintools, 'avr-objdump -h -S -l main.elf'), tofile='main.lss')
            runcmd(os.path.join(bintools, 'avr-nm -n main.elf'), tofile='main.sym')

        elif self.settings["TOOLCHAIN"] == "ARM":
            runcmd(os.path.join(bintools, 'arm-none-eabi-objcopy -O ihex -R .eeprom main.elf main.hex'))
            runcmd(os.path.join(bintools, 'arm-none-eabi-size -B main.elf'))
            runcmd(os.path.join(bintools, 'arm-none-eabi-objdump -h -S -l main.elf'), tofile='main.lss')
            runcmd(os.path.join(bintools, 'arm-none-eabi-nm -n main.elf'), tofile='main.sym')

        elif self.settings["TOOLCHAIN"] == "XTENSA":
            runcmd(os.path.join(bintools, 'xtensa-lx106-elf-size -B main.elf'))
            runcmd(os.path.join(bintools, 'xtensa-lx106-elf-nm -n main.elf'), tofile='main.sym')            
            # runcmd(os.path.join(bintools, 'xtensa-lx106-elf-objdump -h -S -l main.elf'), tofile='main.lss')

        elif self.settings["TOOLCHAIN"] == "ESP32":
            runcmd(os.path.join(bintools, 'xtensa-esp32-elf-size -B main.elf'))
            runcmd(os.path.join(bintools, 'xtensa-esp32-elf-nm -n main.elf'), tofile='main.sym')            
            # runcmd(os.path.join(bintools, 'xtensa-esp32-elf-objdump -h -S -l main.elf'), tofile='main.lss')
            
        else:
            raise Exception("Unsupported toolchain")

        # convert to bin
        if self.settings["TOOLCHAIN"] == "ESP32":
            # esptool.main('--chip esp32 elf2image --flash_mode dio --flash_freq 40m --flash_size 4MB --elf-sha256-offset 0xb0 -o main.bin main.elf'.split())
            esptool.main('--chip esp32 elf2image --flash_mode dio --flash_freq 40m --flash_size 4MB -o main.bin main.elf'.split())
            ih = IntelHex()
            ih.loadbin('main.bin', offset=0x00000)
            ih.write_hex_file('main.hex')

        elif self.settings["TOOLCHAIN"] == "XTENSA":
            esptool.main('elf2image -o image -ff 40m -fm dio -fs 4MB main.elf'.split())

            # create binary image
            # often, the ESP8266 loads in two images, one at 0x00000000, and one at 0x00010000.
            # we're just going to combine the two so we just have the one image.
            ih = IntelHex()
            
            ih.loadbin('image0x00000.bin', offset=0x00000)
            try:
                # non bootloader builds will load irom to 0x10000
                ih.loadbin('image0x10000.bin', offset=0x10000)

            except IOError:
                # bootloader builds will load irom to 0x12000
                # however, the entire binary file is offset. so we don't
                # need the extra 0x2000 length here.
                try:
                    ih.loadbin('image0x12000.bin', offset=0x10000)

                except IOError:
                    # sometimes we might not have this file at all
                    pass

            ih.tobinfile('main.bin')
            ih.write_hex_file('main.hex')

        # change back to working dir
        os.chdir(cwd)


class AppBuilder(HexBuilder):
    def __init__(self, *args, **kwargs):
        super(AppBuilder, self).__init__(*args, **kwargs)

        # insert OS first
        try:
            self.libraries.insert(0, self.settings["OS_PROJECT"])
        except KeyError:
            pass

    def merge_hex(self, hex1, hex2, target):
        ih = IntelHex(hex1)
        ih2 = IntelHex(hex2)

        ih.merge(ih2, overlap='replace')

        ih.write_hex_file(target)

    def post_process(self):
        logging.info("Processing binaries")

        fw_info_fmt = '<I16s128s16s128s16s32s'

        # save original dir
        cwd = os.getcwd()

        # change to target dir
        os.chdir(self.target_dir)
    

        if self.settings["TOOLCHAIN"] == "ESP32":
            with open("main.bin", 'rb') as f:
                original_image = bytearray(f.read())

            # iterate segment headers
            # image header at offsset 0
            # offset 1 contains segment count
            # first starts at offset 24
            segment_count = original_image[1]
            offset = 24

            while segment_count > 0:
                segment_count -= 1
                segment_header = struct.unpack('<LL', original_image[offset:offset + 8])
                offset += 8
                segment_length = segment_header[1]

                if segment_header[0] == int(self.settings["FW_INFO_ADDR"], 16):
                    fw_info_addr = offset

                offset += segment_length

        else:
            fw_info_addr = int(self.settings["FW_INFO_ADDR"], 16)

        ih = IntelHex('main.hex')

        starting_offset = ih.minaddr()

        fwid = uuid.UUID('{' + self.settings["FWID"] + '}')

        # get KV meta start
        kv_meta_addr = fw_info_addr + struct.calcsize(fw_info_fmt)

        if self.settings['TOOLCHAIN'] in ['ARM', 'XTENSA', 'ESP32']:
            kv_meta_len = KVMetaFieldWidePtr().size()

        else:
            kv_meta_len = KVMetaField().size()

        bindata = ih.tobinstr()

        kv_meta_data = []
        
        while True:
            addr = kv_meta_addr - starting_offset
            kv_meta_s = bindata[addr:addr + kv_meta_len]

            try:
                if self.settings['TOOLCHAIN'] in ['ARM', 'XTENSA', 'ESP32']:
                    kv_meta = KVMetaFieldWidePtr().unpack(kv_meta_s)

                else:
                    kv_meta = KVMetaField().unpack(kv_meta_s)

            except UnicodeDecodeError:
                logging.error("KV unpacking failed at: 0x%0x" % (addr))
                raise

            # compute hash and repack into binary
            kv_meta.hash = catbus_string_hash(str(kv_meta.param_name))

            ih.puts(kv_meta_addr, kv_meta.pack())

            kv_meta_addr += kv_meta_len

            if kv_meta.param_name == "kvstart":
                continue

            elif kv_meta.param_name == "kvend":
                break

            else:
                kv_meta_data.append(kv_meta)

        kv_meta_by_hash = {}

        logging.info("Hash type: FNV1A_32")

        # create lookups by 32 bit hash
        index = 0
        for kv in kv_meta_data:
            hash32 = fnv1a_32(kv.param_name.encode('utf-8'))

            if hash32 in kv_meta_by_hash:
                raise Exception("Hash collision! %s 0x%lx" % (kv.param_name, hash32))

            kv_meta_by_hash[hash32] = (kv, index)

            index += 1

        # sort indexes
        sorted_hashes = sorted(kv_meta_by_hash.keys())

        # create binary look up table
        kv_index = bytes()
        for a in sorted_hashes:
            kv_index += struct.pack('<LB', a, kv_meta_by_hash[a][1])

        # get temp size with index and 16 bit crc
        temp_size = ih.maxaddr() - ih.minaddr() + 1 + len(kv_index) + 2

        # pad to 32 bits, taking into account 16 bit crc
        padding = 4 - temp_size % 4
        ih.puts(ih.maxaddr() + 1, '\0' * padding)
                
        # write index to end of hex file
        kv_index_addr = ih.maxaddr() + 1
        ih.puts(kv_index_addr, kv_index)

        size = ih.maxaddr() - ih.minaddr() + 1

        # get os info
        try:
            os_project = get_project_builder(self.settings["OS_PROJECT"], target=self.board_type)
        except KeyError:
            os_project = ""

        # create firmware info structure
        fw_info = struct.pack(fw_info_fmt,
                                size,
                                fwid.bytes,
                                os_project.proj_name.encode('utf-8'),
                                os_project.version.encode('utf-8'),
                                self.settings['PROJ_NAME'].encode('utf-8'),
                                self.version.encode('utf-8'),
                                self.board_type.encode('utf-8'))

        # insert fw info into hex
        ih.puts(fw_info_addr, fw_info)

        # compute crc
        crc_func = crcmod.predefined.mkCrcFun('crc-aug-ccitt')

        if self.settings["TOOLCHAIN"] != "ESP32":
            crc = crc_func(ih.tobinstr())
            ih.puts(ih.maxaddr() + 1, struct.pack('>H', crc))

        logging.info("size: %d" % (size))
        logging.info("fwid: %s" % (fwid))
        logging.info("fwinfo: %x" % (fw_info_addr))
        logging.info("kv index len: %d" % (len(kv_index)))
        logging.info("kv index addr: 0x%0x" % (kv_index_addr))
        logging.info("os name: %s" % (os_project.proj_name))
        logging.info("os version: %s" % (os_project.version))
        logging.info("app name: %s" % (self.settings['PROJ_NAME']))
        logging.info("app version: %s" % (self.version))

        if self.settings["TOOLCHAIN"] != "ESP32":
            logging.info("crc: 0x%x" % (crc))

            size = ih.maxaddr() - ih.minaddr() + 1
            assert size % 4 == 0

        ih.write_hex_file('main.hex')
        ih.tobinfile('firmware.bin')

        if self.settings["TOOLCHAIN"] == "ESP32":
            with open("firmware.bin", 'rb') as f:
                firmware_image = bytearray(f.read())

            checksum_location = (len(original_image) - 32) - 1
            # print(hex(checksum_location))
            offset = (16 - checksum_location % 16)
            
            if checksum_location == 0:
                checksum_location += 16

            else:
                checksum_location += offset

            assert checksum_location % 16 == 0

            checksum_location -= 1
            # print(hex(checksum_location))

            # disable SHA256 hash check in bootloader
            firmware_image[0x17] = 0

            # save file
            with open("firmware.bin", 'wb') as f:
                f.write(firmware_image)


            # now ask esptool for the correct checksum, since the actual algorithm isn't documented and I
            # don't feel like reverse engineering it.            
            
            # redirect stdout
            backup_stdout = sys.stdout
            sys.stdout = StringIO()

            # run esptool to get image info
            esptool.main('--chip esp32 image_info firmware.bin'.split())

            # capture stdout
            image_info = sys.stdout.getvalue()
                
            # restore stdout
            sys.stdout = backup_stdout

            # print(image_info)

            # get checksum
            
            for line in image_info.split('\n'):
                if not line.startswith('Checksum'):
                    continue

                
                if line.find('invalid') >= 0:
                    line = line.replace('(', '')
                    line = line.replace(')', '')
                    checksum = int(line.split()[5], 16)
                    
                else:
                    checksum = int(line.split()[1], 16)

                break

            firmware_image[checksum_location] = checksum

            logging.info("ESP32 checksum: 0x%02x" % (checksum))

            crc = crc_func(firmware_image)
            logging.info("crc: 0x%x" % (crc))
            firmware_image += bytes(struct.pack('>H', crc))

            with open("firmware.bin", 'wb') as f:
                f.write(firmware_image)

        # get loader info
        try:
            loader_project = get_project_builder(self.settings["LOADER_PROJECT"], target=self.board_type)

            if self.settings["TOOLCHAIN"] == "XTENSA":
                # create loader image
                with open(os.path.join(loader_project.target_dir, "main.bin"), 'rb') as f:
                    loader_image = f.read()

                # sanity check
                if loader_image[0] != 0xE9:
                    raise Exception("invalid esp bootloader image")

                bootloader_size = int(loader_project.settings["FW_START_OFFSET"], 16)

                # pad bootloader
                loader_image += bytes((bootloader_size - len(loader_image) % bootloader_size) * [0])

                with open("firmware.bin", 'rb') as f:
                    firmware_image = f.read()

                if firmware_image[0] != 0xE9:
                    raise Exception("invalid esp firmware image")

                combined_image = loader_image + firmware_image

                # need to pad to sector length
                combined_image += bytes((4096 - (len(combined_image) % 4096)) * [0xff])

                # write a combined image suitable for esptool
                with open("esptool_image.bin", 'wb') as f:
                    f.write(combined_image)

                if 'extra_files' in self.board and 'wifi_firmware.bin' in self.board['extra_files']:
                    # append MD5
                    md5 = hashlib.md5(combined_image)
                    combined_image += md5.digest()

                    # prepend length (not counting the length field itself or the MD5 - the actual FW length)
                    combined_image = struct.pack('<L', len(combined_image) - 16) + combined_image

                    with open("wifi_firmware.bin", 'wb') as f:
                        f.write(combined_image)

            else:
                # create loader image
                loader_hex = os.path.join(loader_project.target_dir, "main.hex")
                try:
                    self.merge_hex('main.hex', loader_hex, 'loader_image.hex')
                
                except IOError:
                    logging.info("Loader image not found, cannot create loader_image.hex")

                except Exception as e:
                    logging.exception(e)

        except KeyError:
            logging.info("Loader project not found, cannot create loader_image.hex")


        # create firmware package
        try:
            package = get_firmware_package(self.settings['PROJ_NAME'])
        except FirmwarePackageNotFound:
            package = FirmwarePackage(self.settings['PROJ_NAME'])

        package.FWID = self.settings['FWID']

        if package.FWID.replace('-', '') == CHROMATRON_ESP_UPGRADE_FWID:
            print("Special handling for ESP8266 upgrade on Chromatron Classic")
            coproc_package = get_firmware_package('coprocessor')

            try:
                coproc_image = coproc_package.images['coprocessor']['firmware.bin']

            except KeyError:
                logging.error("Must build coprocessor firmware first!")
                sys.exit(-1)

            coproc_version = coproc_package.manifest['targets']['coprocessor']['firmware.bin']['version']
            
            package.add_image('firmware.bin', coproc_image, self.board_type, coproc_version)

        else:
            # open firmware.bin from filesystem
            with open("firmware.bin", 'rb') as f:
                firmware_image = bytearray(f.read())

            package.add_image('firmware.bin',firmware_image, self.board_type, self.version)
            
        if 'extra_files' in self.board:
            for file in self.board['extra_files']:
                package.add_image(file, None, self.board_type, self.version)

        package.save()

        logging.info("Package dir: %s" % (get_build_package_dir()))


class LoaderBuilder(HexBuilder):
    def __init__(self, *args, **kwargs):
        super(LoaderBuilder, self).__init__(*args, **kwargs)

        try:
            self.libraries.insert(0, self.settings["OS_PROJECT"])
        except KeyError:
            pass

        for lib in self.libraries:
            self.includes.append(lib)

        self.libraries = list()

        try:
            self.includes.append(self.settings["OS_PROJECT"])
        except KeyError:
            pass

        if "LOADER_ADDRESS" in self.settings:
            self.settings["LINK_FLAGS"].append("-Wl,--section-start=.text=%s" % (self.settings["LOADER_ADDRESS"]))

class ARMLoaderBuilder(HexBuilder):
    def __init__(self, *args, **kwargs):
        super(ARMLoaderBuilder, self).__init__(*args, **kwargs)

        try:
            self.libraries.insert(0, self.settings["OS_PROJECT"])
        except KeyError:
            pass

        try:
            self.includes.append(self.settings["OS_PROJECT"])
        except KeyError:
            pass

class ESP8266LoaderBuilder(ARMLoaderBuilder):
    def __init__(self, *args, **kwargs):
        super(ESP8266LoaderBuilder, self).__init__(*args, **kwargs)

class ESP32LoaderBuilder(ARMLoaderBuilder):
    def __init__(self, *args, **kwargs):
        super(ESP32LoaderBuilder, self).__init__(*args, **kwargs)

class ExeBuilder(Builder):
    def __init__(self, *args, **kwargs):
        super(ExeBuilder, self).__init__(*args, **kwargs)

        # insert OS first
        try:
            self.libraries.insert(0, self.settings["OS_PROJECT"])
        except KeyError:
            pass

    def list_source(self):
        source_files = []

        for lib in self.libraries:
            lib_proj = get_project_builder(lib, target=self.board_type)

            source_files.extend(lib_proj.list_source())

        for d in self.source_dirs:
            for f in os.listdir(d):
                if f.endswith('.c'):
                    fpath = os.path.join(d, f)
                    # prevent duplicates
                    if fpath not in source_files:
                        source_files.append(fpath)

        return source_files

    def link(self):
        logging.info("Linking %s" % (self.proj_name))

        # build command string
        cmd = '"' + self.settings["CC"] + '"'
        cmd += ' '

        for flag in self.settings["C_FLAGS"]:
            cmd += flag + ' '

        cmd += '%(DEP_DIR)/%(SOURCE_FNAME).o.d' + ' '

        # save working dir
        cwd = os.getcwd()

        # change to target dir
        os.chdir(self.target_dir)

        obj_dir = self.settings["OBJ_DIR"]

        source_files = self.list_source()

        # source object files
        for source in source_files:
            source_path, source_fname = os.path.split(source)
            cmd += obj_dir + '/' + source_fname + '.o' + ' '

        # linker flags
        for flag in self.settings["LINK_FLAGS"]:
            cmd += flag + ' '

        cmd = cmd.replace('%(OBJ_DIR)', obj_dir)
        cmd = cmd.replace('%(DEP_DIR)', self.settings["DEP_DIR"])
        # cmd = cmd.replace('%(SOURCE_FNAME)', self.proj_name)
        cmd = cmd.replace("%(LINKER_SCRIPT)", os.path.join(self.settings_dir, self.settings["LINKER_SCRIPT"]))
        cmd = cmd.replace("%(APP_NAME)", self.settings["PROJ_NAME"])
        cmd = cmd.replace("%(TARGET_DIR)", self.target_dir)
        cmd = cmd.replace('%(BASE_DIR)', BASE_DIR)

        # replace windows path separators with unix
        cmd = cmd.replace('\\', '/')

        runcmd(cmd)

        # change back to working dir
        os.chdir(cwd)


def get_project_info_dir():
    return settings.get_app_dir()

def get_project_info_file():
    return PROJECTS_FILE

def get_build_configs():
    build_configs = {}

    # open/create firmware ID file
    try:
        fwid_f = open(BUILD_CONFIGS_FWID_FILE, 'r')

    except IOError:
        # create file
        fwid_f = open(BUILD_CONFIGS_FWID_FILE, 'w+')
    
    try:
        fwids = json.loads(fwid_f.read())

    except ValueError:
        fwids = {}

    fwid_f.close()


    for filename in os.listdir(BUILD_CONFIGS_DIR):
        # skip hidden files
        if filename.startswith('.'):
            continue

        filepath = os.path.join(BUILD_CONFIGS_DIR, filename)
        
        parser = configparser.SafeConfigParser()

        try:
            parser.read(filepath)

        except configparser.MissingSectionHeaderError:
            continue

        for section in parser.sections():
            try:
                proj_name       = section
                proj_version    = parser.get(section, 'proj_version')
                base_proj       = parser.get(section, 'base_proj')

                try:
                    full_name   = parser.get(section, 'full_name')

                except configparser.NoOptionError:
                    full_name   = proj_name                

                try:
                    libs        = parser.get(section, 'libs').split(',')

                except configparser.NoOptionError:
                    libs = []

                # trim whitespace
                libs = [lib.strip() for lib in libs]

                if proj_name in build_configs:
                    raise KeyError("Duplicate project name in build config: %s" % (proj_name))

                # try to get firmware ID
                # create if not found
                if proj_name not in fwids:
                    fwids[proj_name] = str(uuid.uuid4())

                    print("Created FWID %s for project: %s" % (fwids[proj_name], proj_name))
                    
                    # save to file
                    with open(BUILD_CONFIGS_FWID_FILE, 'w+') as f:
                        f.write(json.dumps(fwids))

                build_configs[proj_name] = {'PROJ_NAME': proj_name,
                                            'PROJ_VERSION': proj_version,
                                            'FULL_NAME': full_name,
                                            'FWID': fwids[proj_name],
                                            'BASE_PROJ': base_proj,
                                            'LIBS': libs}

            except configparser.NoOptionError:
                pass


    return build_configs

def get_project_list():
    try:
        f = open(get_project_info_file(), 'r')
        data = f.read()
        f.close()

        return json.loads(data)

    except IOError:
        return dict()

    except ValueError:
        return dict()

def save_project_info(name, target_dir):
    data = get_project_list()
    data[name] = target_dir

    # getdata dir
    data_dir = get_project_info_dir()

    # check if data dir exists
    if not os.path.exists(data_dir):
        os.makedirs(data_dir)

    f = open(get_project_info_file(), 'w')
    f.truncate(0)
    f.write(json.dumps(data, indent=4, separators=(',', ': ')))
    f.close()

    logging.info("Saving project listing to: %s" % (data_dir))

def remove_project_info(project_name):
    data = get_project_list()

    # check if project exists
    if project_name not in data:
        raise ProjectNotFoundException

    del data[project_name]

    f = open(get_project_info_file(), 'w')
    f.truncate(0)
    f.write(json.dumps(data, indent=4, separators=(',', ': ')))
    f.close()

    logging.info("Removed %s from listing" % (project_name))

def get_project_builder(proj_name=None, fwid=None, target=None, build_loader=False, **kwargs):
    projects = get_project_list()

    try:
        if proj_name:
            return get_builder(projects[proj_name], target, build_loader=build_loader, **kwargs)
            # return Builder(projects[proj_name], board_type=target)

        elif fwid:
            for k, v in projects.items():
                try:
                    builder = get_project_builder(k, target=target)

                except ProjectNotFoundException:
                    pass

                if builder.fwid == fwid:
                    return builder

    except KeyError:
        if proj_name:
            # try to build from config
            configs = get_build_configs()

            if proj_name not in configs:
                raise ProjectNotFoundException(proj_name)

            # build from config
            return ConfigBuilder(build_config=configs[proj_name], board_type=target, **kwargs)

        else:
            raise ProjectNotFoundException(fwid)

    except Exception:
        raise


    raise ProjectNotFoundException

# make a new project in the current working dir
def make_project(proj_name):
    logging.info("Creating new project: %s" % (proj_name))

    # create new firmware ID
    fwid = str(uuid.uuid4())

    # set project dir name
    project_dir = proj_name

    # create project dir
    os.mkdir(project_dir)

    # get path to project template
    template_dir = os.path.join(SETTINGS_DIR, 'project_template')

    # copy project template
    for f in os.listdir(template_dir):
        shutil.copy(os.path.join(template_dir, f), project_dir)

    # open settings file
    with open(os.path.join(project_dir, SETTINGS_FILE), 'r+') as f:
        s = f.read()

        # replace template strings
        s = s.replace('%(PROJ_NAME)', proj_name)
        s = s.replace('%(FWID)', fwid)

        f.truncate(0)
        f.seek(0)
        f.write(s)

    logging.info("New project: '%s' created with firmware ID: %s" % (proj_name, fwid))

    save_project_info(proj_name, os.path.abspath(project_dir))

def get_fwid():
    logging.info("Creating FWID")

    return str(uuid.uuid4())


def main():
    # set global log level
    logger = logging.getLogger('')
    logger.setLevel(logging.DEBUG)

    # add a console handler to print anything INFO and above to the console
    console = logging.StreamHandler()
    console.setLevel(logging.INFO)
    formatter = logging.Formatter('%(levelname)s %(message)s')
    console.setFormatter(formatter)
    logging.getLogger('').addHandler(console)

    parser = argparse.ArgumentParser(description='SapphireMake')

    parser.add_argument("--clean", "-c", action="store_true", help="clean only")
    parser.add_argument("--project", "-p", action="append", help="projects to build")
    parser.add_argument("--build_all", action="store_true", help="build all config projects")
    parser.add_argument("--target", "-t", action="store", help="set build target")
    parser.add_argument("--def", "-D", action="append", help="pass define to compiler")
    parser.add_argument("--dir", "-d", action="append", help="directories to build")
    parser.add_argument("--loader",action="store_true", help="build bootloader")
    parser.add_argument("--list", "-l", action="store_true", help="list projects")
    parser.add_argument("--list_configs", action="store_true", help="list build configurations")
    parser.add_argument("--reset", action="store_true", help="reset project listing")
    parser.add_argument("--discover", action="store_true", help="discover projects")
    parser.add_argument("--create", help="create a new template project")
    parser.add_argument("--unlink", help="unlink a project")
    parser.add_argument("--fwid", action="store_true", help="create a firmware ID")
    parser.add_argument("--make_release", action="store_true", help="make current build a release package")
    parser.add_argument("--install_build_tools", action="store_true", help="install build tools")
    parser.add_argument("--load_esp32", action="store_true", help="Load to ESP32")
    parser.add_argument("--load_esp32_loader", action="store_true", help="Load bootloader to ESP32")
    parser.add_argument("--monitor", action="store_true", help="Run serial monitor")
    parser.add_argument("--port", action="store", default=None, help="Set serial port")

    args = vars(parser.parse_args())

    logging.info("SapphireMake")

    # set target dir to cwd
    target_dir = os.getcwd()

    logging.info("Target dir: %s" % (target_dir))

    # make sure projects dir exists
    try:
        os.makedirs(PROJECTS_FILE_PATH)

    except OSError:
        pass

    logging.info("Projects dir: %s" % (PROJECTS_FILE_PATH))

    # check if making a release
    if args["make_release"]:
        
        # get current packages
        releases = firmware_package.get_releases()

        release_name = input("Enter release name: ")

        # check if we already have this release
        if release_name in releases:
            print("This release already exists.")

            overwrite = input("Enter 'overwrite' to overwrite this release: ")

            if overwrite != 'overwrite':
                print("Release cancelled")
                return

            # erase original release
            shutil.rmtree(firmware_package.firmware_package_dir(release=release_name), True)

        # copy build package to new name
        shutil.copytree(get_build_package_dir(), firmware_package.firmware_package_dir(release=release_name))

        shutil.make_archive(firmware_package.firmware_package_dir(release=release_name), 'zip', firmware_package.firmware_package_dir(release=release_name))

        # erase build release
        shutil.rmtree(get_build_package_dir(), True)

        return

    # check if installing build tools
    if args["install_build_tools"]:
        get_tools()
        return

    # check if listing
    if args["list"]:
        projects = get_project_list()

        for key in projects:
            print("%20s %s" % (key, projects[key]))

        return

    if args["list_configs"]:
        pprint(get_build_configs())
        return

    # check if reset
    if args["reset"]:
        os.remove(get_project_info_file())
        return

    if args["fwid"]:
        print(get_fwid())
        return

    # check if discovering
    if args["discover"]:
        # load old project listing
        try:
            with open(get_project_info_file(), 'r') as f:
                projects = json.loads(f.read())

        except IOError:
            projects = {}

        for root, dirs, files in os.walk(os.getcwd()):
            if SETTINGS_FILE in files:
                # load settings file                
                with open(os.path.join(root, SETTINGS_FILE)) as f:
                    try:
                        proj_settings = json.loads(f.read())

                    except json.decoder.JSONDecodeError:
                        continue
                        
                    if 'FWID' not in proj_settings:
                        continue

                    if 'BUILD_TYPE' not in proj_settings:
                        continue

                    try:
                        proj_name = proj_settings['PROJ_NAME']

                        if proj_name == '%(PROJ_NAME)':
                            continue

                        projects[proj_name] = root

                    except KeyError:
                        pass

        with open(PROJECTS_FILE, 'w+') as f:
            f.write(json.dumps(projects))

        print("Found %d projects" % (len(projects)))
        print("Saved project file: %s" % (PROJECTS_FILE))


        return

    # check if creating a new project
    if args["create"]:
        make_project(args["create"])
        return

    # check if unlinking a new project
    if args["unlink"]:
        remove_project_info(args["unlink"])
        return

    # check if loading to an ESP32
    if args["load_esp32"]:
        # check if project is given
        if not args["project"]:
            print("Must specify project to load!")
            return

        proj = args["project"][0]

        try:
            package = get_firmware_package(proj)
        except FirmwarePackageNotFound:
            print(f"Could not find package for {proj}")

            return

        # redirect stdout to buffer so we can capture esptool output
        from io import StringIO
        old_stdout = sys.stdout
        temp_stdout = StringIO()
        sys.stdout = temp_stdout

        # get chip info
        esptool_cmd = f'--chip esp32 --baud 2000000 chip_id'
        if args['port'] is not None:
            esptool_cmd = f'--port {args["port"]} ' + esptool_cmd

        esptool.main(esptool_cmd.split())

        # restore stdout
        sys.stdout = old_stdout

        chip_info = temp_stdout.getvalue()

        # check if single or dual core chip
        single_core = False
        if chip_info.find('Single Core') >= 0:
            single_core = True

        target = 'esp32'
        if single_core:
            target = 'esp32_single'

            print("ESP32 single core")

        else:
            print("ESP32 dual core")

        # extract firmware image and write to file
        image = package.images[target]['firmware.bin']
        temp_filename = '__temp_firmware.bin'
        with open(temp_filename, 'wb') as f:
            f.write(image)

        esptool_cmd = f'--chip esp32 --baud 2000000 write_flash 0x10000 {temp_filename}'

        if args['port'] is not None:
            esptool_cmd = f'--port {args["port"]} ' + esptool_cmd

        esptool.main(esptool_cmd.split())

        # remove temp file
        try:
            os.remove(temp_filename)

        except FileNotFoundError:
            pass
        
        if not args["monitor"]:
            return

    elif args["load_esp32_loader"]:
        proj = 'loader_esp32'

        builder = get_project_builder(proj, build_loader=True, target='esp32_loader')

        os.chdir(builder.target_dir)

        esptool.main(f'--chip esp32 --baud 2000000 write_flash 0x8000 partition-table.espbin'.split())
        esptool.main(f'--chip esp32 --baud 2000000 write_flash 0x1000 main.bin'.split())

        if not args["monitor"]:
            return

    if args["monitor"]:
        serial_monitor.monitor(portname=args['port'])
        return

    # check if setting target
    if args["target"]:
        board_type = args["target"]

    else:
        board_type = "chromatron_classic"

    if args["loader"]:
        build_loader = True

    else:
        build_loader = False


    if args["build_all"]:
        configs = get_build_configs()
        
        args["project"] = list(configs.keys())

    if args['def']:
        defines = args['def']
    else:
        defines = []

    # check if project is given
    if args["project"]:
        for p in args["project"]:
            logging.info("Project: %s" % (p))

            # check if project is in the projects list
            try:
                builder = get_project_builder(p, build_loader=build_loader, target=board_type, defines=defines)
            except Exception:
                raise

            # init logging
            builder.init_logging()

            # run clean
            if args["clean"]:
                builder.clean()

            else:
                builder.build()

    # check if directory is given
    elif args["dir"]:
        for d in args["dir"]:
            target_dir = os.path.abspath(d)

            builder = get_builder(target_dir, board_type, build_loader=build_loader, defines=defines)

            # init logging
            builder.init_logging()

            # run clean
            builder.clean()

            if not args["clean"]:
                builder.build()

    # build in current directory
    else:
        builder = get_builder(target_dir, board_type, build_loader=build_loader, defines=defines)

        # init logging
        builder.init_logging()

        # run clean
        builder.clean()

        if not args["clean"]:
            builder.build()



if __name__ == "__main__":
    main()
    