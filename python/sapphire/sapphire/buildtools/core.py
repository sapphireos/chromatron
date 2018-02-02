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
import urllib
import tarfile
import zipfile
from datetime import datetime
import hashlib
import zipfile
import ConfigParser
from pprint import pprint
import firmware_package

import settings

from intelhex import IntelHex

import global_settings

from sapphire.devices.sapphiredata import KVMetaField
from sapphire.common import util
from catbus import catbus_string_hash
from fnvhash import fnv1a_32


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
    
SETTINGS_DIR = os.path.dirname(os.path.abspath(global_settings.__file__))
SETTINGS_FILE = 'settings.json'
PROJECTS_FILE_NAME = 'projects.json'
PROJECTS_FILE_PATH = firmware_package.data_dir()
PROJECTS_FILE = os.path.join(PROJECTS_FILE_PATH, PROJECTS_FILE_NAME)
BUILD_CONFIGS_DIR = os.path.join(os.getcwd(), 'configurations')
BUILD_CONFIGS_FWID_FILE = os.path.join(BUILD_CONFIGS_DIR, 'fwid.json')
BASE_DIR = os.getcwd()
MASTER_HASH_DB_FILE = os.path.join(PROJECTS_FILE_PATH, 'kv_hashes.json')
TOOLS_DIR = os.path.join(PROJECTS_FILE_PATH, 'tools')

def get_build_package_dir():
    return firmware_package.firmware_package_dir('build')


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
        urllib.URLopener().retrieve(tool[0] + tool[1], tool[1])

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
        output += line

    if output != '':
        if not tofile and tolog:
            logging.warn("\n" + output)

        elif tofile:
           f = open(tofile, 'w')
           f.write(output)
           f.close()

    p.wait()

    return output


def get_builder(target_dir, target_type, build_loader=False, fnv_hash=True):
    builder = Builder(target_dir, target_type, fnv_hash=fnv_hash)

    modes = {"os": OSBuilder, "loader": LoaderBuilder, "app": AppBuilder, "lib": LibBuilder, "exe": ExeBuilder}

    return modes[builder.settings["BUILD_TYPE"]](target_dir, target_type, build_loader=build_loader, fnv_hash=fnv_hash)

class Builder(object):
    def __init__(self, target_dir, target_type=None, build_loader=False, fnv_hash=True):
        self.target_dir = target_dir
        self.target_type = target_type

        self.build_loader = build_loader

        try:
            self.settings = self.get_settings()

        except AttributeError:
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
            self.target_type = app_settings["TARGET"]

        self.settings_dir = os.path.join(BASE_DIR, 'targets', self.target_type)

        settings = self._get_default_settings()

        # override defaults with app settings
        for k, v in app_settings.iteritems():
            if k == "LIBRARIES":
                settings[k].extend(v)

            else:
                settings[k] = v

        if "CC" not in settings:
            settings["CC"] = os.path.join(TOOLS_DIR, 'avr', 'bin', 'avr-gcc')

        if "BINTOOLS" not in settings:
            settings["BINTOOLS"] = os.path.join(TOOLS_DIR, 'avr', 'bin')

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

    def list_source(self):
        source_files = []

        for d in self.source_dirs:
            for f in os.listdir(d):
                if f.endswith('.c'):
                    fpath = os.path.join(d, f)
                    # prevent duplicates
                    if fpath not in source_files:
                        source_files.append(fpath)

        return source_files

    def list_headers(self):
        source_files = []

        for d in self.source_dirs:
            for f in os.listdir(d):
                if f.endswith('.h'):
                    fpath = os.path.join(d, f)
                    # prevent duplicates
                    if fpath not in source_files:
                        source_files.append(fpath)

        return source_files

    def create_source_hash(self):
        source_files = self.list_source()
        hashed_files = {}

        for f in source_files:
            m = hashlib.sha256()
            m.update(open(f).read())
            hashed_files[f] = m.hexdigest()

        return hashed_files

    def update_source_hash_file(self):
        hash_file = open("source_hash.json", 'w+')

        hash_file.write(json.dumps(self.create_source_hash()))
        hash_file.close()

    def get_source_hash_file(self):
        try:
            hash_file = open("source_hash.json")

        except IOError:
            return {}

        return json.loads(hash_file.read())

    def get_buildnumber(self):
        try:
            f = open(os.path.join(self.target_dir, self.settings["BUILD_NUMBER_FILE"]), 'r')
            build_number = f.read()
            f.close()

        except IOError:
            f = open(os.path.join(self.target_dir, self.settings["BUILD_NUMBER_FILE"]), 'w')
            build_number = "0"
            f.write(build_number)
            f.close()

        return int(build_number)

    def set_buildnumber(self, value):
        f = open(os.path.join(self.target_dir, self.settings["BUILD_NUMBER_FILE"]), 'w')
        f.write("%d" % (value))
        f.close()

    buildnumber = property(get_buildnumber, set_buildnumber)

    def get_version(self):
        s = "%s.%04d" % (str(self.settings["PROJ_VERSION"]), self.buildnumber)
        return s

    version = property(get_version)

    def scan_file_for_kv(self, filename):
        with open(filename, 'r') as f:
            data = f.read()

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
            b = get_project_builder(include, target=self.target_type)

            files.extend(b.list_source())
            files.extend(b.list_headers())

        hashes = {}

        for f in files:
            hashes.update(self.scan_file_for_kv(f))

        # with open(os.path.join(self.target_dir, KV_HEADER), 'w+') as f:
        #     f.write('/*THIS FILE IS AUTOMATICALLY GENERATED!*/\n')
        #     f.write('/*DO NOT EDIT THIS FILE!*/\n')
        #     f.write('#ifndef _KV_HASHES_H_\n')
        #     f.write('#define _KV_HASHES_H_\n')
        #     f.write('#include <inttypes.h>\n')
        #     f.write('#include "catbus_common.h"\n')

        #     for k, v in hashes.iteritems():
        #         f.write('#define %s ((catbus_hash_t32)%s)\n' % (k, v))
    
        #     f.write('#endif\n')
        
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

        try:
            os.remove(os.path.join(self.target_dir, 'manifest.txt'))

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
        logging.info("Building %s for target: %s" % (self.settings["PROJ_NAME"], self.target_type))

        self.pre_process()
        self.compile()
        self.link()
        self.post_process()

        save_project_info(self.proj_name, self.target_dir)

    def pre_process(self):
        # inc build number
        self.buildnumber += 1
            
        # get KV hashes and add to defines
        hashes = self.create_kv_hashes()

        for k, v in hashes.iteritems():
            self.defines.append('%s=((catbus_hash_t32)%s)' % (k, v))

        # self.settings["C_FLAGS"].append('-fdollars-in-identifiers')

    def get_libraries(self, current_libs={}):
        libs = current_libs

        for lib in self.libraries:
            if lib in current_libs:
                continue

            libs[lib] = get_project_builder(lib, target=self.target_type)

            libs.update(libs[lib].get_libraries(current_libs=libs))

        return libs

    def build_libs(self):
        libs = self.get_libraries()

        for lib, builder in libs.iteritems():
            logging.info("Building library %s" % (lib))
        
            builder.clean()
            builder.build()

    def compile(self):
        logging.info("Compiling %s" % (self.proj_name))

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
            cmd += ' -c %s ' % (compile_path)

            for include in self.includes:
                include_dir = get_project_builder(include, target=self.target_type).target_dir

                cmd += '-I%s ' % (include_dir)

            for include in self.libraries:
                include_dir = get_project_builder(include, target=self.target_type).target_dir

                cmd += '-I%s ' % (include_dir)
                cmd += '-D%s ' % (include.upper())

            # add extra source dirs to include
            for source_dir in self.source_dirs:
                cmd += '-I%s ' % (source_dir)

            for define in self.defines:
                cmd += '-D%s ' % (define)

            for flag in self.settings["C_FLAGS"]:
                cmd += flag + ' '

            cmd += '%(DEP_DIR)/%(SOURCE_FNAME).o.d' + ' '
            cmd += '-o ' + '%(OBJ_DIR)/%(SOURCE_FNAME).o' + ' '

            cmd = cmd.replace('%(OBJ_DIR)', self.settings["OBJ_DIR"])
            cmd = cmd.replace('%(SOURCE_FNAME)', source_fname)
            cmd = cmd.replace('%(DEP_DIR)', self.settings["DEP_DIR"])

            # replace windows path separators with unix
            cmd = cmd.replace('\\', '/')

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

        self.app_builder = get_project_builder(self.build_config['BASE_PROJ'], target=self.target_type)
            
        # change app's FWID to the config FWID
        self.app_builder.settings['FWID'] = self.fwid
        
        self.app_builder.settings['PROJ_NAME'] = self.build_config['PROJ_NAME']
        self.app_builder.settings['PROJ_VERSION'] = self.build_config['PROJ_VERSION']
        self.app_builder.settings['FULL_NAME'] = self.build_config['FULL_NAME']


        self.lib_builders = []
        for lib in self.build_config['LIBS']:
            builder = get_project_builder(lib, target=self.target_type)

            self.lib_builders.append(builder)


    def build(self):
        logging.info("Building configuration %s for target: %s" % (self.proj_name, self.target_type))

        includes = []
        inits = []

        # build libraries
        for builder in self.lib_builders:

            builder.clean()
            builder.build()
            
            includes.extend(builder.settings["LIB_INCLUDES"])
            inits.append(builder.settings["LIB_INIT"])
            
            self.app_builder.libraries.append(builder.proj_name)


        lib_init_filename = os.path.join(self.app_builder.target_dir, '__libs.c')

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

        # self.app_builder.build_libs()
        self.app_builder.clean()
        try:
            self.app_builder.build()

        except AppZipNotFound: # not a problem in config-driven builds
            pass

        # make sure we have the firmware package dir
        try:
            os.makedirs(get_build_package_dir())

        except OSError:
            pass

        # copy firmware zip
        shutil.copy(os.path.join(self.app_builder.target_dir, '%s.zip' % (self.proj_name)), get_build_package_dir())

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

    # def link(self):
    #     logging.info("Linking %s" % (self.proj_name))
    #
    #     # save working dir
    #     cwd = os.getcwd()
    #
    #     # change to target dir
    #     os.chdir(self.target_dir)
    #
    #     # build command string
    #     cmd = self.settings["AR"] + ' '
    #
    #     for flag in self.settings["AR_FLAGS"]:
    #         cmd += flag + ' '
    #
    #     cmd += self.proj_name + '.a' + ' '
    #
    #     obj_dir = self.settings["OBJ_DIR"]
    #
    #     for source in self.list_source():
    #         source_path, source_fname = os.path.split(source)
    #         cmd += obj_dir + '/' + source_fname + '.o' + ' '
    #
    #     # replace windows path separators with unix
    #     cmd = cmd.replace('\\', '/')
    #
    #     runcmd(cmd)
    #
    #     # change back to working dir
    #     os.chdir(cwd)


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

        cmd += '%(DEP_DIR)/%(SOURCE_FNAME).o.d' + ' '

        # save working dir
        cwd = os.getcwd()

        # change to target dir
        os.chdir(self.target_dir)

        obj_dir = self.settings["OBJ_DIR"]

        # source object files
        for source in self.list_source():
            source_path, source_fname = os.path.split(source)
            cmd += obj_dir + '/' + source_fname + '.o' + ' '

        # linker flags
        for flag in self.settings["LINK_FLAGS"]:
            cmd += flag + ' '

        # included libraries
        for lib in self.libraries:
            lib_project = get_project_builder(lib, target=self.target_type)
            # source object files
            for source in lib_project.list_source():
                source_path, source_fname = os.path.split(source)
                cmd += lib_project.target_dir + '/' + obj_dir + '/' + source_fname + '.o' + ' '

            # lib_dir = get_project_builder(lib, target=self.target_type).target_dir
            #
            # cmd += '%s ' % (lib_dir + '/' + lib + '.a')

        # include all libraries again, so GCC can handle cross dependencies
        # for lib in self.libraries:
        #     lib_dir = get_project_builder(lib, target=self.target_type).target_dir
        #
        #     cmd += '%s ' % (lib_dir + '/' + lib + '.a')

        cmd = cmd.replace('%(OBJ_DIR)', obj_dir)
        cmd = cmd.replace('%(DEP_DIR)', self.settings["DEP_DIR"])
        # cmd = cmd.replace('%(SOURCE_FNAME)', self.proj_name)
        cmd = cmd.replace("%(LINKER_SCRIPT)", os.path.join(self.settings_dir, "linker.x"))
        cmd = cmd.replace("%(APP_NAME)", self.settings["PROJ_NAME"])
        cmd = cmd.replace("%(TARGET_DIR)", self.target_dir)

        # replace windows path separators with unix
        cmd = cmd.replace('\\', '/')

        runcmd(cmd)

        logging.info("Generating output files")

        # enclose in quotes so we can handle spaces in the command filepath
        bintools = '"' + self.settings["BINTOOLS"] + '"'
        runcmd(os.path.join(bintools, 'avr-objcopy -O ihex -R .eeprom main.elf main.hex'))
        runcmd(os.path.join(bintools, 'avr-size -B main.elf'))
        runcmd(os.path.join(bintools, 'avr-objdump -h -S -l main.elf'), tofile='main.lss')
        runcmd(os.path.join(bintools, 'avr-nm -n main.elf'), tofile='main.sym')

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
        ih.merge(IntelHex(hex2))

        ih.write_hex_file(target)

    def post_process(self):
        logging.info("Processing binaries")

        fw_info_fmt = '<I16s128s16s128s16s'

        fw_info_addr = int(self.settings["FW_INFO_ADDR"], 16)

        # save original dir
        cwd = os.getcwd()

        # change to target dir
        os.chdir(self.target_dir)

        ih = IntelHex('main.hex')

        fwid = uuid.UUID('{' + self.settings["FWID"] + '}')

        # get KV meta start
        kv_meta_addr = fw_info_addr + struct.calcsize(fw_info_fmt)
        kv_meta_len = KVMetaField().size()

        bindata = ih.tobinstr()

        kv_meta_data = []

        while True:
            kv_meta = KVMetaField().unpack(bindata[kv_meta_addr:kv_meta_addr + kv_meta_len])

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
            hash32 = fnv1a_32(str(kv.param_name))

            if hash32 in kv_meta_by_hash:
                raise Exception("Hash collision!")

            kv_meta_by_hash[hash32] = (kv, index)

            index += 1

        # sort indexes
        sorted_hashes = sorted(kv_meta_by_hash.keys())

        # create binary look up table
        kv_index = ''
        for a in sorted_hashes:
            kv_index += struct.pack('<LB', a, kv_meta_by_hash[a][1])
                
        # write to end of hex file
        ih.puts(ih.maxaddr() + 1, kv_index)

        size = ih.maxaddr() - ih.minaddr() + 1

        # get os info
        try:
            os_project = get_project_builder(self.settings["OS_PROJECT"], target=self.target_type)
        except KeyError:
            os_project = ""

        # create firmware info structure
        fw_info = struct.pack(fw_info_fmt,
                                size,
                                fwid.bytes,
                                os_project.proj_name,
                                os_project.version,
                                str(self.settings['PROJ_NAME']),
                                self.version)

        # insert fw info into hex
        ih.puts(fw_info_addr, fw_info)

        # compute crc
        crc_func = crcmod.predefined.mkCrcFun('crc-aug-ccitt')

        crc = crc_func(ih.tobinstr())

        logging.info("size: %d" % (size))
        logging.info("fwid: %s" % (fwid))
        logging.info("fwinfo: %x" % (fw_info_addr))
        logging.info("kv index len: %d" % (len(kv_index)))
        logging.info("crc: 0x%x" % (crc))
        logging.info("os name: %s" % (os_project.proj_name))
        logging.info("os version: %s" % (os_project.version))
        logging.info("app name: %s" % (self.settings['PROJ_NAME']))
        logging.info("app version: %s" % (self.version))

        ih.puts(ih.maxaddr() + 1, struct.pack('>H', crc))

        ih.write_hex_file('main.hex')
        ih.tobinfile('firmware.bin')

        # get loader info
        loader_project = get_project_builder(self.settings["LOADER_PROJECT"], target=self.target_type)

        # create loader image
        loader_hex = os.path.join(loader_project.target_dir, "main.hex")
        self.merge_hex('main.hex', loader_hex, 'loader_image.hex')

        # create sha256 of binary
        sha256 = hashlib.sha256(ih.tobinstr())

        # create manifest file
        data = {
            'name': self.settings['FULL_NAME'],
            'timestamp': datetime.utcnow().isoformat(),
            'sha256': sha256.hexdigest(),
            'fwid': self.settings['FWID'],
            'version': self.version
        }

        with open('manifest.txt', 'w+') as f:
            f.write(json.dumps(data))

        # create firmware zip file
        zf = zipfile.ZipFile('chromatron_main_fw.zip', 'w')
        zf.write('manifest.txt')
        zf.write('firmware.bin')
        zf.close()

        # create second, project specific zip
        # we'll remove the first zip after
        # we update the firmware tools
        zf = zipfile.ZipFile('%s.zip' % (self.settings['PROJ_NAME']), 'w')
        zf.write('manifest.txt')
        zf.write('firmware.bin')
        zf.close()


        # change back to original dir
        os.chdir(cwd)

        logging.info("Package dir: %s" % (get_build_package_dir()))

        # make sure we have the firmware package dir
        try:
            os.makedirs(get_build_package_dir())

        except OSError:
            pass


        # copy firmware zip
        try:
            shutil.copy(os.path.join(self.target_dir, '%s.zip' % (self.proj_name)), get_build_package_dir())

        except IOError:
            raise AppZipNotFound

        # update build date
        with open(os.path.join(get_build_package_dir(), firmware_package.PUBLISHED_AT_FILENAME), 'w') as f:
            f.write(util.now().isoformat())


class LoaderBuilder(HexBuilder):
    def __init__(self, *args, **kwargs):
        super(LoaderBuilder, self).__init__(*args, **kwargs)

        # try:
        #     self.libraries.insert(0, self.settings["OS_PROJECT"])
        # except KeyError:
        #     pass

        for lib in self.libraries:
            self.includes.append(lib)

        self.libraries = list()

        try:
            self.includes.append(self.settings["OS_PROJECT"])
        except KeyError:
            pass

        self.settings["LINK_FLAGS"].append("-Wl,--section-start=.text=%s" % (self.settings["LOADER_ADDRESS"]))


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
            lib_proj = get_project_builder(lib, target=self.target_type)

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

        # included libraries
        # for lib in self.libraries:
            # lib_proj = get_project_builder(lib, target=self.target_type)

            # lib_dir = get_project_builder(lib, target=self.target_type).target_dir

            # cmd += '%s ' % (lib_dir + '/' + lib + '.a')

        # include all libraries again, so GCC can handle cross dependencies
        # for lib in self.libraries:
            # lib_dir = get_project_builder(lib, target=self.target_type).target_dir

            # cmd += '%s ' % (lib_dir + '/' + lib + '.a')

        cmd = cmd.replace('%(OBJ_DIR)', obj_dir)
        cmd = cmd.replace('%(DEP_DIR)', self.settings["DEP_DIR"])
        # cmd = cmd.replace('%(SOURCE_FNAME)', self.proj_name)
        cmd = cmd.replace("%(LINKER_SCRIPT)", os.path.join(self.settings_dir, "linker.x"))
        cmd = cmd.replace("%(APP_NAME)", self.settings["PROJ_NAME"])
        cmd = cmd.replace("%(TARGET_DIR)", self.target_dir)

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
        filepath = os.path.join(BUILD_CONFIGS_DIR, filename)
        
        configparser = ConfigParser.SafeConfigParser()

        try:
            configparser.read(filepath)

        except ConfigParser.MissingSectionHeaderError:
            continue

        for section in configparser.sections():
            try:
                proj_name       = section
                proj_version    = configparser.get(section, 'proj_version')
                base_proj       = configparser.get(section, 'base_proj')

                try:
                    full_name   = configparser.get(section, 'full_name')

                except ConfigParser.NoOptionError:
                    full_name   = proj_name                

                try:
                    libs        = configparser.get(section, 'libs').split(',')

                except ConfigParser.NoOptionError:
                    libs = []

                # trim whitespace
                libs = [lib.strip() for lib in libs]

                if proj_name in build_configs:
                    raise KeyError("Duplicate project name in build config: %s" % (proj_name))

                # try to get firmware ID
                # create if not found
                if proj_name not in fwids:
                    fwids[proj_name] = str(uuid.uuid4())

                    print "Created FWID %s for project: %s" % (fwids[proj_name], proj_name)
                    
                    # save to file
                    with open(BUILD_CONFIGS_FWID_FILE, 'w+') as f:
                        f.write(json.dumps(fwids))

                build_configs[proj_name] = {'PROJ_NAME': proj_name,
                                            'PROJ_VERSION': proj_version,
                                            'FULL_NAME': full_name,
                                            'FWID': fwids[proj_name],
                                            'BASE_PROJ': base_proj,
                                            'LIBS': libs}

            except ConfigParser.NoOptionError:
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

def get_project_builder(proj_name=None, fwid=None, target=None, build_loader=False):
    projects = get_project_list()

    try:
        if proj_name:
            return get_builder(projects[proj_name], target, build_loader=build_loader)
            # return Builder(projects[proj_name], target_type=target)

        elif fwid:
            for k, v in projects.iteritems():
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
            return ConfigBuilder(build_config=configs[proj_name], target_type=target)

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
    parser.add_argument("--dir", "-d", action="append", help="directories to build")
    parser.add_argument("--loader",action="store_true", help="build bootloader")
    parser.add_argument("--list", "-l", action="store_true", help="list projects")
    parser.add_argument("--list_configs", action="store_true", help="list build configurations")
    parser.add_argument("--reset", action="store_true", help="reset project listing")
    parser.add_argument("--discover", action="store_true", help="discover projects")
    parser.add_argument("--create", help="create a new template project")
    parser.add_argument("--unlink", help="unlink a project")
    parser.add_argument("--make_release", action="store_true", help="make current build a release package")
    parser.add_argument("--install_build_tools", action="store_true", help="install build tools")

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

        release_name = raw_input("Enter release name: ")

        # check if we already have this release
        if release_name in releases:
            print "This release already exists."

            overwrite = raw_input("Enter 'overwrite' to overwrite this release: ")

            if overwrite != 'overwrite':
                print "Release cancelled"
                return

            # erase original release
            shutil.rmtree(firmware_package.firmware_package_dir(release=release_name), True)

        # copy build package to new name
        shutil.copytree(get_build_package_dir(), firmware_package.firmware_package_dir(release=release_name))

        shutil.make_archive(os.path.join(firmware_package.firmware_package_dir(), release_name), 'zip', firmware_package.firmware_package_dir(release=release_name))

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
            print "%20s %s" % (key, projects[key])

        return

    if args["list_configs"]:
        pprint(get_build_configs())
        return

    # check if reset
    if args["reset"]:
        os.remove(get_project_info_file())
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
                    proj_settings = json.loads(f.read())

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

        print "Found %d projects" % (len(projects))
        print "Saved project file: %s" % (PROJECTS_FILE)


        return

    # check if creating a new project
    if args["create"]:
        make_project(args["create"])
        return

    # check if unlinking a new project
    if args["unlink"]:
        remove_project_info(args["unlink"])
        return

    # check if setting target
    if args["target"]:
        target_type = args["target"]

    else:
        target_type = "chromatron"

    if args["loader"]:
        build_loader = True

    else:
        build_loader = False


    if args["build_all"]:
        configs = get_build_configs()
        
        args["project"] = configs.keys()

    # check if project is given
    if args["project"]:
        for p in args["project"]:
            logging.info("Project: %s" % (p))

            # check if project is in the projects list
            try:
                builder = get_project_builder(p, target=target_type)
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

            builder = get_builder(target_dir, target_type, build_loader=build_loader)

            # init logging
            builder.init_logging()

            # run clean
            builder.clean()

            if not args["clean"]:
                builder.build()

    # build in current directory
    else:
        builder = get_builder(target_dir, target_type, build_loader=build_loader)

        # init logging
        builder.init_logging()

        # run clean
        builder.clean()

        if not args["clean"]:
            builder.build()



if __name__ == "__main__":
    main()
    