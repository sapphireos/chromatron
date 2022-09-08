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

"""Device
"""

from elysianfields import *
from catbus import get_type_name, Client, CATBUS_MAIN_PORT, NoResponseFromHost, ProtocolErrorException
from catbus.link import *

from . import sapphiredata
from . import channel
from ..common import catbus_string_hash

import time
import sys
import datetime
import types
from datetime import datetime, timedelta

from pprint import pprint
import hashlib
import binascii
from collections import UserDict 
import json
import base64
import traceback
import crcmod

from sapphire.buildtools import firmware_package
from sapphire.buildtools.firmware_package import FirmwarePackage
from sapphire.buildtools.core import CHROMATRON_ESP_UPGRADE_FWID
import sapphire.common.util

import fnmatch

crc_func = crcmod.predefined.mkCrcFun('crc-aug-ccitt')


NTP_EPOCH = datetime(1900, 1, 1)


# do not change these!!!
FILE_TRANSFER_LEN   = 512
MAX_KV_DATA_LEN     = 512

MAX_LATENCIES       = 128

# Key value groups
KV_GROUP_NULL               = 0
KV_GROUP_NULL1              = 254
KV_GROUP_SYS_CFG            = 1
KV_GROUP_SYS_INFO           = 2
KV_GROUP_SYS_STATS          = 3
KV_GROUP_APP_BASE           = 32

KV_GROUP_ALL                = 255

kv_groups = {
    "kv_group_null":        KV_GROUP_NULL,
    "kv_group_null_1":      KV_GROUP_NULL1,
    "kv_group_sys_cfg":     KV_GROUP_SYS_CFG,
    "kv_group_sys_info":    KV_GROUP_SYS_INFO,
    "kv_group_sys_stats":   KV_GROUP_SYS_STATS,
    "kv_group_all":         KV_GROUP_ALL,
}

sys_groups = [KV_GROUP_SYS_CFG, KV_GROUP_SYS_INFO, KV_GROUP_SYS_STATS]


KV_ID_ALL                   = 255

# Key value flags
CATBUS_FLAGS_READ_ONLY      = 0x01
CATBUS_FLAGS_PERSIST        = 0x04
CATBUS_FLAGS_DYNAMIC        = 0x08

# warning flags
SYS_WARN_MEM_FULL           = 0x0001
SYS_WARN_NETMSG_FULL        = 0x0002
SYS_WARN_FLASHFS_FAIL       = 0x0004
SYS_WARN_FLASHFS_HARD_ERROR = 0x0008
SYS_WARN_CONFIG_FULL        = 0x0010
SYS_WARN_CONFIG_WRITE_FAIL  = 0x0020
SYS_WARN_EVENT_LOG_OVERFLOW = 0x0040
SYS_WARN_MISSING_KV_INDEX   = 0x0080
SYS_WARN_SYSTEM_ERROR       = 0x8000

def decode_warnings(flags):
    warnings = list()

    if flags & SYS_WARN_MEM_FULL:
        warnings.append("mem_full")

    if flags & SYS_WARN_NETMSG_FULL:
        warnings.append("netmsg_full")

    if flags & SYS_WARN_FLASHFS_FAIL:
        warnings.append("flashfs_fail")

    if flags & SYS_WARN_FLASHFS_HARD_ERROR:
        warnings.append("flashfs_hard_error")

    if flags & SYS_WARN_CONFIG_FULL:
        warnings.append("config_full")

    if flags & SYS_WARN_CONFIG_WRITE_FAIL:
        warnings.append("config_write_fail")

    if flags & SYS_WARN_EVENT_LOG_OVERFLOW:
        warnings.append("event_log_overflow")

    if flags & SYS_WARN_MISSING_KV_INDEX:
        warnings.append("missing_kv_index")

    if flags & SYS_WARN_SYSTEM_ERROR:
        warnings.append("system_error")

    return warnings

SYS_REBOOT_NORMAL           = 1
SYS_REBOOT_SAFE             = -2
SYS_REBOOT_NOAPP            = -3 # not yet supported
SYS_REBOOT_LOADFW           = -4
SYS_REBOOT_FORMATFS         = -5
SYS_REBOOT_RECOVERY         = -6
SYS_REBOOT_RESET_CONFIG     = -7


CLI_PREFIX = 'cli_'

DeviceStatus = set(['unknown', 'offline', 'online', 'reboot'])


class DeviceCommsErrorException(Exception):
    pass

class DeviceUnreachableException(DeviceCommsErrorException):
    pass

class DeviceMetaErrorException(Exception):
    pass

class ReadOnlyKeyException(DeviceMetaErrorException):
    pass

class UnrecognizedKeyException(DeviceMetaErrorException):
    pass

class InvalidDataTypeException(DeviceMetaErrorException):
    pass

class InvalidDeviceIDException(DeviceMetaErrorException):
    pass

class NotASapphireDevice(Exception):
    pass

class KVKey(object):
    def __init__(self,
                 device=None,
                 id=None,
                 flags=None,
                 type=None,
                 value=None,
                 key=None):

        self._device = device

        self.id = id
        self.type = type
        self._value = value
        self.key = key

        # decode flags to strings
        self.flags = list()

        if flags & CATBUS_FLAGS_READ_ONLY:
            self.flags.append("read_only")

        if flags & CATBUS_FLAGS_PERSIST:
            self.flags.append("persist")

        if flags & CATBUS_FLAGS_DYNAMIC:
            self.flags.append("dynamic")

    def __str__(self):
        flags = ''

        if "persist" in self.flags:
            flags += 'P'
        else:
            flags += ' '

        if "read_only" in self.flags:
            flags += 'R'
        else:
            flags += ' '

        if "dynamic" in self.flags:
            flags += 'D'
        else:
            flags += ' '

        s = "%32s %6s %8s %s" % (self.key, flags, get_type_name(self.type), str(self._value))
        return s

    def is_readonly(self):
        return "read_only" in self.flags

    def get_value(self):
        # if value hasn't been initialized, we load from the device
        if self._value == None:
            # internal meta data is auto-updated, so we can toss the return value
            self._device.get_key(self.key)

        return self._value

    def set_value(self, value):
        self._value = value
        self._device.set_key(self.key, self._value)

    value = property(get_value, set_value)


class KVMeta(UserDict):
    def __init__(self):
        super().__init__()
        
    def __setitem__(self, key, value):
        self.data[key] = value
        value.key = key


class Device(object):
    def __init__(self,
                 host=None,
                 port=CATBUS_MAIN_PORT,
                 device_id=None,
                 comm_channel=None):

        super(Device, self).__init__()

        self.host = host
        self.port = port
        self.firmware_id = None
        self.firmware_name = ""
        self.os_name = ""
        self.firmware_version = ""
        self.os_version = ""
        self.device_id = device_id
        self.name = "<anon@%s>" % (host)

        self.device_status = 'offline'

        self.object_id = str(self.device_id)

        self._keys = KVMeta()
        self._firmware_info_hash = None

        self._channel = comm_channel

        # if no channel is specified, create one
        if self._channel is None:
            try:
                self._channel = channel.createSerialChannel(self.host)
                self.channel_type = self._channel.channel_type

            except channel.ChannelInvalidHostException:
                raise DeviceUnreachableException

            except channel.NotSerialChannel:
                self.channel_type = 'network'

            except channel.InvalidChannel:
                raise
        
        self._client = None
        self._bridge = None

        if self.channel_type == 'network':
            self._client = Client()
            self._client.connect((self.host, self.port))

        elif self.channel_type == 'serial_udp':
            self._client = Client()
            # set window to 1, so messages will ping pong over the bridge.
            self._client.set_window(1, 1)
            self._bridge = channel.UDPSerialBridge(self._channel, CATBUS_MAIN_PORT)
    
            self._client.connect(('localhost', self._bridge.port))


    def __str__(self):
        return "Device:%s@%s" % (self.name, self.host)


    # TODO this should move to the console
    def who(self):
        try:
            name = self.name

        except KeyError:
            name = ''

        if len(name) == 0:
            name = "<anon@%s>" % self.host

        s = "%24s IP:%15s FW:%24s Ver:%8s OS:%8s" % \
            (name,
             self.host,
             self.firmware_name,
             self.firmware_version,
             self.os_version)

        return s

    def to_dict(self):
        d = dict()

        for k, v in self._keys.items():
            d[k] = v.value

        d["firmware_name"] = self.firmware_name

        return d

    def scan(self, get_all=True):    
        self.get_kv_meta()

        # check if this is a chromatron device (running sapphireos)
        try:
            if self.get_key('os_name') != 'sapphire':
                raise NotASapphireDevice()

        except KeyError:
            raise NotASapphireDevice()                

        if get_all:
            self.get_all_kv()

        self.device_id = self.get_key('device_id')            
        self.name = self.get_key('meta_tag_name')

        self.get_firmware_info()

        return self

    def get_kv_meta(self):    
        kvmeta = self._client.get_meta()

        # reset keys
        self._keys = KVMeta()

        # load keys into meta data
        for key, meta in kvmeta.items():
            self._keys[key] = KVKey(key=key, device=self, id=meta.hash, flags=meta.flags, type=meta.type)

    def get_all_kv(self):
        keys = [key for key in self._keys]

        return self.get_kv(*keys)

    def set_key(self, param, value):
        self.set_kv(**{param: value})

    def set_kv(self, **kwargs):
        params = []

        keys = {}
        filter_unchanged = False
        if '_filter_unchanged' in kwargs:
            filter_unchanged = kwargs['_filter_unchanged']
            del kwargs['_filter_unchanged']

        data = {}

        # iterate over keys and create requests for them
        for key in kwargs:
            if filter_unchanged:
                # filter out keys which are not changing
                if kwargs[key] == self._keys[key].value:
                    continue

            # check if key is set to read only
            if 'read_only' in self._keys[key].flags:
                raise ReadOnlyKeyException(key)

            if isinstance(kwargs[key], str):
                # we do not support unicode, and things break if leaks into the system
                kwargs[key] = str(kwargs[key])
            
            data[key] = kwargs[key]
        
        self._client.set_keys(**data)

    def get_key(self, param):
        return self.get_kv(param)[param]

    def get_kv(self, *args):
        expanded_keys = []
        responses = {}

        if len(self._keys) > 0:
            # iterate over keys and create requests for them
            for key in args:
                expanded_keys.extend(fnmatch.filter(list(self._keys.keys()), key))

        else:
            expanded_keys = args

        responses = self._client.get_keys(expanded_keys)
    
        if len(self._keys) > 0:
            for k, v in responses.items():
                # update internal meta data
                self._keys[k]._value = v

        return responses

    def lookup_hash(self, key):
        value = self._client.lookup_hash(key)[key]

        if value == key:
            raise KeyError(key)

        return value

    def set_security_key(self, key_id, key):
        raise NotImplementedError
        
    def echo(self):
        try:
            return self._client.ping()

        except NoResponseFromHost:
            raise DeviceUnreachableException

    def reboot(self):    
        return self.set_key('reboot', SYS_REBOOT_NORMAL)

    def wait(self, timeout=60.0):    
        start = time.time()

        while (time.time() - start) < timeout:
            try:
                self.echo()

                return

            except DeviceUnreachableException:
                pass

        raise DeviceUnreachableException

    def safe_mode(self):
        return self.set_key('reboot', SYS_REBOOT_SAFE)

    def reboot_and_load_fw(self):
        return self.set_key('reboot', SYS_REBOOT_LOADFW)

    def format_fs(self):
        return self.set_key('reboot', SYS_REBOOT_FORMATFS)

    def reboot_recovery(self):
        return self.set_key('reboot', SYS_REBOOT_RECOVERY)

    def reset_config(self):
        return self.set_key('reboot', SYS_REBOOT_RESET_CONFIG)

    def delete_file(self, filename):
        self._client.delete_file(filename)

    def get_file(self, filename, progress=None):
        data = self._client.read_file(filename, progress=progress)

        if progress:
            progress(len(data))

        return data

    def put_file(self, filename, data, progress=None, use_percent=False):    
        data = self._client.write_file(filename, data, progress=progress, use_percent=use_percent)

        if progress:
            if use_percent:
                progress(100, filename=filename)

            else:
                progress(len(data), filename=filename)

    def list_files_raw(self):
        data = self.get_file("fileinfo")

        fileinfo = sapphiredata.FileInfoArray()

        fileinfo.unpack(data)

        return fileinfo

    def list_files(self):
        return self._client.list_files()

    def check_file(self, filename, data):
        filehash = catbus_string_hash(data)
    
        if self._client.check_file(filename)['hash'] != filehash:
            raise IOError("Firmware image does not match!")

    def load_firmware(self, firmware_id=None, progress=None, verify=True, release='build', use_percent=False):
        fw_info = self.get_firmware_info()  

        if firmware_id == None:
            firmware_id = fw_info.firmware_id
        
        fw = firmware_package.get_firmware_package(firmware_id, release=release)

        if fw_info.board not in fw.manifest['targets']:
            # board not found, or board not listed (from 2.x firmware without that information)
            hw_type = self.get_key('hw_type')
            if hw_type in ['Chromatron']:
                if fw.FWID == CHROMATRON_ESP_UPGRADE_FWID:
                    fw_info.board = 'chromatron_classic_upgrade'

                else:
                    raise Exception("Must perform ESP8266 upgrade!")

            elif hw_type in ['ChromatronESP8266']:
                fw_info.board = 'chromatron_classic'

        for filename, data in fw.images[fw_info.board].items():
            # delete old firmware
            self.delete_file(filename)

            filehash = catbus_string_hash(data)
            
            # load firmware image
            self.put_file(filename, data, progress=progress, use_percent=use_percent)
        
            if verify:
                self.check_file(filename, data)

        # reboot to loader
        self.reboot_and_load_fw()

    def get_firmware_info(self):
        try:
            data = self.get_file("fwinfo")
        except IOError: # file not found
            return None

        # create hash of firmware info
        h = hashlib.new('sha256')
        h.update(data)
        self._firmware_info_hash = h.hexdigest()

        # unpack
        fw_info = sapphiredata.FirmwareInfoField()
        fw_info.unpack(data)

        # update state
        self.firmware_id        = fw_info.firmware_id
        self.firmware_name      = fw_info.firmware_name
        self.firmware_version   = fw_info.firmware_version
        self.os_name            = fw_info.os_name
        self.os_version         = fw_info.os_version

        # check board ID
        if fw_info.board == '':
            try:
                hw_type = self.get_key('hw_type')
                if hw_type in ['Chromatron']:
                    fw_info.board = 'chromatron_legacy'

            except KeyError:
                fw_info.board = 'chromatron_classic_upgrade'

        return fw_info

    def get_gc_info(self):
        data = self.get_file("gc_data")

        gc_array = ArrayField(_field=Uint32Field).unpack(data)

        return {"sector_erase_counts": gc_array}

    def get_thread_info(self):
        data = self.get_file("threadinfo")

        info = sapphiredata.ThreadInfoArray()
        info.unpack(data)

        return info

    def get_thread_info_dump(self):
        data = self.get_file("threadinfo.dump")

        info = sapphiredata.ThreadInfoArray()
        info.unpack(data)

        return info

    def get_handle_info(self):
        data = self.get_file("handleinfo")

        info = sapphiredata.HandleInfoArray()
        info.unpack(data)

        return info

    def get_dns_info(self):
        data = self.get_file("dns_cache")
        info = sapphiredata.DnsCacheArray()
        info.unpack(data)

        return info

    def get_service_info(self):
        data = self.get_file("serviceinfo")
        info = sapphiredata.ServiceInfoArray()
        info.unpack(data)

        return info

    def get_service(self, service_id, group):
        services = self.get_service_info()

        for s in services:
            if s.id == service_id and s.group == group:
                return s

        return None

    def get_datalog_config(self):
        data = self.get_file("datalog_config")

        info = sapphiredata.DatalogEntryArray()
        info.unpack(data)

        return info

    def get_batt_records(self):
        data = self.get_file("batt_recorder")

        info = sapphiredata.BattRecordDataArray()
        info.unpack(data)

        return info


    # helper for testing mostly
    def wait_service(self, service_id, group, state=None, timeout=30.0):
        start = time.time()
        while time.time() - start < timeout:
            time.sleep(0.2)
            s = self.get_service(service_id, group)

            if s is None:
                continue

            if s.state == 0:
                continue

            if state is not None:
                if s.state != state:
                    continue

            return s

        return None

    def get_event_log(self):
        data = self.get_file("event_log")
        info = sapphiredata.EventArray()
        info.unpack(data)

        return info

    def get_link_info(self):
        data = self.get_file("link_info")
        info = sapphiredata.LinkInfoArray()
        info.unpack(data)

        return info

    def get_link_consumer_info(self):
        data = self.get_file("link_consumers")
        info = sapphiredata.LinkConsumerInfoArray()
        info.unpack(data)

        return info

    def get_link_producer_info(self):
        data = self.get_file("link_producers")
        info = sapphiredata.LinkProducerInfoArray()
        info.unpack(data)

        return info


    ##########################
    # Command Line Interface
    ##########################
    def get_cli(self):
        return [f.replace(CLI_PREFIX, '', 1) for f in dir(self)
                if f.startswith(CLI_PREFIX)
                and not f.startswith('_')
                and isinstance(self.__getattribute__(f), types.MethodType)]

    def cli_scan(self, line):
        self.scan()

        return "Done"

    def cli_echo(self, line):
        start = time.time()

        self.echo()

        elapsed = time.time() - start

        return "(%d ms)" % (elapsed * 1000)

    def cli_reboot(self, line):
        self.reboot()
        return "Rebooting"

    def cli_safemode(self, line):
        self.safe_mode()
        return "Rebooting into safe mode"

    def cli_formatfs(self, line):
        self.format_fs()

        return "Formatting file system and rebooting..."

    def cli_rm(self, line):
        self.delete_file(line)

        return "Removed: %s" % (line)

    def cli_ls(self, line):    
        fileinfo = self.list_files()

        s = "\n"

        # sort between disk and virtual files
        disk_files = sorted([f for f in list(fileinfo.values()) if (f['flags'] & 1) == 0], key=lambda a: a['filename'])
        vfiles = sorted([f for f in list(fileinfo.values()) if (f['flags'] & 1) != 0], key=lambda a: a['filename'])

        # iterate over file listing
        for f in disk_files:
            v = ''

            if f['flags'] == 1:
                v = 'V'

            s += "%1s %6d %s\n" % \
                (v,
                 f['size'],
                 f['filename'])

        for f in vfiles:
            v = ''

            if f['flags'] == 1:
                v = 'V'

            s += "%1s %6d %s\n" % \
                (v,
                 f['size'],
                 f['filename'])

        return s

    def cli_cat(self, line):
        data = self.get_file(line)

        s = "\n" + data.decode('utf-8', errors='ignore')

        return s

    def cli_getfile(self, line):
        def progress(length):
            sys.stdout.write("\rReading: %5d bytes" % (length))
            sys.stdout.flush()

        print("")

        data = self.get_file(line, progress=progress)

        f = open(line, 'wb')
        f.write(data)
        f.close()

        return ""

    def cli_putfile(self, line):
        def progress(length, filename=None):
            sys.stdout.write("\rWrite: %5d bytes" % (length))
            sys.stdout.flush()

        f = open(line, 'rb')
        data = f.read()
        f.close()

        print("")

        self.put_file(line, data, progress=progress)

        return ""

    def cli_loadfw(self, line):
        def progress(length, filename=None):
            if filename != None:
                sys.stdout.write("\rWrite: %16s %5d bytes" % (filename, length))
            else:
                sys.stdout.write("\rWrite: %5d bytes" % (length))
            sys.stdout.flush()

        if line == "":
            fw = None
        else:
            fw = line

        try:
            self.load_firmware(firmware_id=fw, progress=progress)

        except firmware_package.FirmwarePackageNotFound:
            print("Firmware not found")
            return

        print("")

        return "Rebooting..."

    def cli_rebootloadfw(self, line):
        self.reboot_and_load_fw()

        print("")

        return "Rebooting with load firmware command..."

    def cli_fwinfo(self, line):
        fwinfo = self.get_firmware_info()

        s = "App:%24s Ver:%s OSVer:%s ID:%s Size:%d" % \
            (fwinfo.firmware_name,
             fwinfo.firmware_version,
             fwinfo.os_version,
             fwinfo.firmware_id,
             fwinfo.firmware_length)

        return s

    def cli_hwinfo(self, line):
        fwinfo = self.get_firmware_info()

        s = "%s" % \
            (fwinfo.board)

        return s

    def cli_gcinfo(self, line):
        info = self.get_gc_info()

        i = 0
        sectors = []

        for sector in info["sector_erase_counts"]:
            sectors.append(sector)

        s = "Erases:%7d Least:%6d Most:%6d" % (sum(sectors), min(sectors), max(sectors))

        if line == "all":

            i = 0
            s += "\nSector erase counts:\n"

            for sector in sectors:
                i += 1

                s += "%6d " % (sector)

                if (i % 8) == 0:
                    s += "\n"

        return s

    def cli_threadinfo(self, line):
        if line == 'dump':
            info = self.get_thread_info_dump()

        else:
            info = self.get_thread_info()

        s = "\nAddr      Line  Flags  Data         Time     Runs   Avg  CPU Name\n"

        total_run_time = 0
        for n in info:
            total_run_time += n.run_time

        for n in info:

            flags = ''

            if n.flags & 1:
                flags += 'W'
            else:
                flags += '-'

            if n.flags & 2:
                flags += 'Y'
            else:
                flags += '-'

            if n.flags & 4:
                flags += 'S'
            else:
                flags += '-'

            if n.flags & 8:
                flags += 'I'
            else:
                flags += '-'

            if n.flags & 16:
                flags += 'A'
            else:
                flags += '-'

            try:
                avg_time = n.run_time / n.runs

            except ZeroDivisionError:
                avg_time = 0

            cpu_usage = (n.run_time / float(total_run_time)) * 100.0

            s += "%8x  %4d   %5s %4d %12d %8d %5d %4.1f %s\n" % \
                (n.addr,
                 n.line,
                 flags,
                 n.data_size,
                 n.run_time,
                 n.runs,
                 avg_time,
                 cpu_usage,
                 n.name)

        return s

    def cli_handleinfo(self, line):
        info = self.get_handle_info()

        s = "\nType             Size\n"

        memtypes = {
            0: "unknown",
            1: "netmsg",
            5: "thread",
            6: "file",
            7: "socket",
            8: "socket_buffer",
            10: "log_buffer",
            11: "cmd_buffer",
            12: "cmd_reply_buffer",
            13: "file_handle",
            14: "fs_blocks",
            15: "kv_link",
            16: "kv_send",
            17: "kv_rx_cache",
            18: "kv_db_entry",
            19: "subscribed_keys",
            20: "vm_data",
            21: "cron_job",
            22: "election",
            23: "dns_query",
            24: "msgflow",
            25: "msgflow_arq_buf",
            26: "service",
            27: "link_consumer",
            28: "link_producer",
            29: "link_remote",
            30: "kv_opt",
        }

        total_size = 0
        handles = 0
        counts = {}

        for n in info:
            if n.handle_size == 0:
                continue

            if n.handle_type in memtypes:
                type_str = memtypes[n.handle_type]

            else:
                type_str = "unknown"

            if type_str not in counts:
                counts[type_str] = [0, 0]

            counts[type_str][0] += 1
            counts[type_str][1] += n.handle_size

            s += "%16s %d\n" % (type_str, n.handle_size)

            handles += 1
            total_size += n.handle_size

        s += "\n"

        for n in counts:
            s += "%16s %4d %4d\n" % (n, counts[n][0], counts[n][1])

        s += "\n"
        s += "Handles: %d\n" % (handles)
        s += "Total size: %d\n" % (total_size)

        return s

    def cli_dnsinfo(self, line):
        dnsinfo = self.get_dns_info()

        s = "\n"

        # iterate over DNS cache entries, filtering out the empty ones
        for d in [d for d in dnsinfo if d.status != 0]:
            if d.status == 0:
                status = "empty"
            elif d.status == 1:
                status = "valid"
            elif d.status == 2:
                status = "invalid"
            elif d.status == 3:
                status = "resolving"
            else:
                status = "unknown"

            s += "IP:%15s Status:%8s TTL:%8d Query:%s" % \
                (d.ip,
                 status,
                 d.ttl,
                 d.query)

        return s

    def cli_serviceinfo(self, line):
        try:
            serviceinfo = self.get_service_info()

        except IOError:
            return "No services found"

        states = {
                0: 'listen',
                1: 'connected',
                2: 'server',
            }
        
        s = "\nService          Group               IP           Port  Priority    Uptime    Timeout | State\n"

        # iterate over service cache entries
        for e in serviceinfo:
            if states[e.state] == 'server':
                uptime = e.local_uptime
                port = e.local_port
            else:
                uptime = e.server_uptime
                port = e.server_port

            try:
                service_id = self.lookup_hash(e.id)

            except KeyError:
                service_id = f'{e.id:x}'

            try:
                group_id = self.lookup_hash(e.group)

            except KeyError:
                group_id = f'{e.group:x}'

            s += "%16s %16s %15s %5d %3d     %7d     %3d         %-10s\n" % \
                (service_id,
                 group_id,
                 e.server_ip,
                 port,
                 e.local_priority,
                 uptime,
                 e.timeout,
                 states[e.state])

        return s

    def cli_linkinfo(self, line):
        s = '\n'

        try:
            linkinfo = self.get_link_info()

            if len(linkinfo) == 0:
                raise IOError

            s += 'Links:\n'
            s += 'Source           Dest             Mode Agg  Rate Hash             Query\n'

            for info in sorted(linkinfo, key=lambda x: x.hash):
                try:
                    source = self.lookup_hash(info.source_key)

                except KeyError:
                    source = f'{info.source_key:x}'                

                try:
                    dest = self.lookup_hash(info.dest_key)

                except KeyError:
                    dest = f'{info.dest_key:x}'       

                if info.mode == LINK_MODE_SEND:
                    mode = "send"
                
                elif info.mode == LINK_MODE_RECV:
                    mode = "recv"

                elif info.mode == LINK_MODE_SYNC:
                    mode = "sync"

                else:
                    mode = "????"

                if info.aggregation == LINK_AGG_ANY:
                    agg = "any"

                elif info.aggregation == LINK_AGG_MIN:
                    agg = "min"

                elif info.aggregation == LINK_AGG_MAX:
                    agg = "max"

                elif info.aggregation == LINK_AGG_SUM:
                    agg = "sum"

                elif info.aggregation == LINK_AGG_AVG:
                    agg = "avg"

                else:
                    agg = "???"


                query_s = ''
                for q in info.query:
                    try:
                        v = self.lookup_hash(q)

                        if v is None:
                            continue

                    except KeyError:
                        v = f'{q:x}'

                    query_s += f'{v} '

                s += "%16s %16s %4s %3s %5d %16x %s\n" % \
                    (source,
                     dest,
                     mode,
                     agg,
                     info.rate,
                     info.hash,
                     query_s)

        except IOError:
            pass

        
        try:
            linkinfo = self.get_link_producer_info()

            if len(linkinfo) == 0:
                raise IOError

            s += 'Producers:\n'
            s += 'Source                Leader: IP Port   Rate Timeout Hash\n'

            for info in sorted(linkinfo, key=lambda x: x.link_hash):
                try:
                    source = self.lookup_hash(info.source_key)

                except KeyError:
                    source = f'{info.source_key:x}'                
                
                s += "%16s %15s %5d %5d %5d   %16x\n" % \
                    (source,
                     info.leader_ip,
                     info.leader_port,
                     info.rate,
                     info.timeout,
                     info.link_hash)

        except IOError:
            pass

        try:
            linkinfo = self.get_link_consumer_info()

            if len(linkinfo) == 0:
                raise IOError

            s += 'Consumers:\n'
            s += 'Hash                IP           Port  Timeout\n'
            
            for info in sorted(linkinfo, key=lambda x: x.link_hash):
                s += "%16x %15s %5d %5d\n" % \
                    (info.link_hash,
                     info.ip,
                     info.port,
                     info.timeout)
                

        except IOError:
            pass
        
        return s
    

    def cli_eventlog(self, line):
        events = sorted(self.get_event_log(), key=lambda evt: evt.timestamp)

        if len(line) == 0:
            line = '*'

        eventlog = [event for event in events if fnmatch.fnmatch(event.event_str, line)]

        s = "\n"

        last_timestamp = 0

        # iterate over events
        for evt in eventlog:
            if evt.event_str in ['timer_alarm_miss',
                                 'cpu_wake',
                                 'func_enter',
                                 'func_exit',
                                 'thread_id']:

                if evt.event_str in ['thread_id', 'func_enter', 'func_exit']:
                    evt.param *= 2 # convert to byte address

                param_str = "0x%5x" % (evt.param)

            else:
                if evt.event_str in ['stack_pointer']:
                    evt.param -= 512
                    evt.param = 16384 - evt.param

                param_str = "%7d" % (evt.param)


            s += "%32s: %s    @ %16s +%16s\n" % \
                (evt.event_str,
                 param_str,
                 timedelta(microseconds=evt.timestamp),
                 timedelta(microseconds=(evt.timestamp - last_timestamp)))

            last_timestamp = evt.timestamp

        return s

    def cli_clearlogs(self, line):
        for filename in ["log.txt", "threadinfo.dump", "event_log", "error_log.txt"]:
            try:
                self.delete_file(filename)

            except IOError:
                pass


    def cli_getkey(self, line):
        if line == "":
            params = self.get_kv('*')

        else:
            params = self.get_kv(line)

        if isinstance(params, dict):
            s = "\nName                             Flags  Type     Value\n"

            for k in sorted(params.keys()):
                s += "%s\n" % (self._keys[k])

        else:
            s = "%s = %s" % (line, params)

        return s

    def cli_setkey(self, line):
        tokens = line.split(' ', 1)
        param = tokens[0]
        value = tokens[1]
        
        # check for special blank value (for resetting strings to empty)
        if str(value) == '__null__':
            value = ''

        value_tok = value.split(' ')

        if len(value_tok) == 1:
            self.set_key(param, value)
            
        else:
            self.set_key(param, value_tok)

        new_param = self.get_key(param)

        return "%s set to: %s" % (param, new_param)


    def cli_resetcfg(self, line):
        print("")
        print("DANGER ZONE! Are you sure you want to do this?")
        print("Type 'yes' if you are sure.")

        yes = input()

        if yes == "yes":
            self.reset_config()

            return "Configuration reset"

        return "No changes made"

    def cli_systime(self, line):
        t = self.get_key("sys_time")
        dt = timedelta(seconds=int(t / 1000))

        return "%11d ms (%s)" % (t, str(dt))

    def cli_status(self, line):
        params = self.get_kv("sys_mode", "mem_peak_usage", "fs_free_space", "sys_time", "supply_voltage")

        if params["sys_mode"] == 0:
            mode = "normal "

        elif params["sys_mode"] == 1:
            mode = "safe   "

        else:
            mode = "unknown"

        s = "Mode:%s PeakMem:%4d DiskSpace:%6d SysTime:%11d Volts:%3.2f" % (mode,
                                                                            params["mem_peak_usage"],
                                                                            params["fs_free_space"],
                                                                            params["sys_time"],
                                                                            params["supply_voltage"] / 1000.0)

        return s

    def cli_warnings(self, line):
        params = self.get_key("sys_warnings")

        warnings = decode_warnings(params)

        s = ''
        for w in warnings:
            s += w + " "

        if len(warnings) == 0:
            s = "OK"

        return s

    def cli_diskinfo(self, line):
        params = self.get_kv("fs_free_space", "fs_total_space", "fs_disk_files", "fs_max_disk_files", "fs_virtual_files", "fs_max_virtual_files")

        s = "Free:%6d Total:%6d Files:%2d / %2d Vfiles:%2d / %2d" % \
            (params["fs_free_space"],
             params["fs_total_space"],
             params["fs_disk_files"],
             params["fs_max_disk_files"],
             params["fs_virtual_files"],
             params["fs_max_virtual_files"])

        return s

    def cli_cpuinfo(self, line):
        params = self.get_kv("thread_task_time", "thread_sleep_time", "thread_peak", "thread_loops", "thread_run_time")

        # convert all params to floats
        params = {k: float(v) for (k, v) in params.items()}

        if params["thread_run_time"] == 0:
            loop_rate = 0
            cpu_usage = 0

        else:
            loop_rate = params["thread_loops"] / (params["thread_run_time"] / 1000.0)
            cpu_usage = (params["thread_task_time"] / params["thread_run_time"]) * 100.0

        s = "CPU:%4.1f%% Tsk:%8d Slp:%8d Ohd:%8d MaxT:%3d Loops:%8d @ %5d/sec" % \
            (cpu_usage,
             params["thread_task_time"],
             params["thread_sleep_time"],
             params["thread_run_time"] - (params["thread_task_time"] + params["thread_sleep_time"]),
             params["thread_peak"],
             params["thread_loops"],
             loop_rate)

        return s

    def cli_irqinfo(self, line):
        params = self.get_kv("sys_time_us", "irq_time", "irq_longest_time", "irq_longest_addr")

        # convert all params to floats
        params = {k: float(v) for (k, v) in params.items()}

        irq_usage = (params["irq_time"] / params["sys_time_us"]) * 100.0

        s = "IRQ:%2.1f%% Longest:%6d uS Addr:0x%05x" % \
            (irq_usage,
             params["irq_longest_time"],
             params["irq_longest_addr"])

        return s


    def cli_meminfo(self, line):
        params = self.get_kv("mem_handles", "mem_max_handles", "mem_stack", "mem_max_stack", "mem_free_space", "mem_peak_usage", "mem_heap_size", "mem_used", "mem_dirty")

        s = "Handles:%3d/%3d Stack:%4d/%4d Free:%4d Used:%4d Dirty: %4d Peak:%4d Heap:%4d" % \
             (params["mem_handles"],
              params["mem_max_handles"],
              params["mem_stack"],
              params["mem_max_stack"],
              params["mem_free_space"],
              params["mem_used"],
              params["mem_dirty"],
              params["mem_peak_usage"],
              params["mem_heap_size"])

        return s

    def cli_loaderinfo(self, line):
        params = self.get_kv("loader_status", "loader_version_minor", "loader_version_major")

        s = "Loader version: %s.%s Status: %d" % \
            (chr(params["loader_version_major"]),
             chr(params["loader_version_minor"]),
             params["loader_status"])

        return s

    def cli_ntptime(self, line):
        ntp_seconds = self.get_key("ntp_seconds")

        ntp_now = timedelta(seconds=ntp_seconds) + NTP_EPOCH

        return ntp_now.isoformat()


    def cli_ntpset(self, line):
        ntp_seconds = self.get_key("ntp_seconds")

        if ntp_seconds != 0:
            print('NTP clock has already been set.')
            print('Are you sure you wish to proceed?')

            yes = input()

            if yes != "yes":     
                return 'No changes made'

        now = sapphire.common.util.now()
        ntp_seconds, ntp_fraction = sapphire.common.util.datetime_to_ntp(now)

        self.set_key('ntp_seconds', ntp_seconds)

        fractionless_now = sapphire.common.util.ntp_to_datetime(ntp_seconds, 0)
        print(f'NTP time set to: {fractionless_now}')

    # def cli_loadwifi(self, line):
    #     def progress(length, filename=None):
    #         sys.stdout.write("\rWrite: %5d bytes" % (length))
    #         sys.stdout.flush()

    #     filename = 'wifi_firmware.bin'
    #     with open(filename, 'rb') as f:
    #         data = f.read()

    #     # verify first byte (quick sanity check)
    #     if data[0] != 0xE9:
    #         print("Invalid firmware image!")
    #         return

    #     # compute firmware length, minus the md5 at the end
    #     # md5 is always 16 bytes
    #     fw_len = len(data) - 16

    #     # get md5 from file
    #     file_md5 = data[fw_len:]
    #     md5_digest = hashlib.md5(data[:fw_len]).digest()

    #     if file_md5 != md5_digest:
    #         print("Invalid firmware image!")
    #         return

    #     # preprocessing should already be done by build script,
    #     # but leaving this here for now in case we need it.

    #     # # we override bytes 2 and 3 in the ESP8266 image
    #     # data[2] = 0
    #     # data[3] = 0
    #     #
    #     # # convert back to string
    #     # data = ''.join(map(chr, data))
    #     #
    #     # # need to pad to sector length
    #     # padding_len = 4096 - (len(data) % 4096)
    #     # data += (chr(0xff) * padding_len)
    #     #
    #     # fw_len = len(data)
    #     #
    #     # md5_digest = hashlib.md5(data).digest()
    #     # data += md5_digest
    #     #
    #     # with open('wifi_firmware_padded.bin', 'w') as f:
    #     #     f.write(data)

        
    #     print("\nErasing old firmware...")
    #     try:
    #         self.delete_file(filename)

    #     except IOError: # file not found
    #         pass

    #     # calculate crc of file data
    #     filehash = catbus_string_hash(data)

    #     print("\nLoading firmware image")

    #     try:
    #         self.put_file(filename, data, progress=progress)

    #     except Exception as e:
    #         print(type(e), e)
    #         raise

    #     # print("\nVerifying firmware image...")
    #     # self.check_file(filename, data)

    #     print("Setting firmware length...")
    #     self.set_key('wifi_fw_len', fw_len)

    #     print("Setting MD5...")
    #     self.set_key('wifi_md5', binascii.hexlify(md5_digest))

    #     print("Starting wifi firmware flasher...")

    #     self.reboot()

    #     time.sleep(5.0)

    #     for i in range(20):
    #         try:
    #             self.echo('')

    #         except DeviceUnreachableException:
    #             time.sleep(1.0)


    #     self.echo('')

    #     print("Firmware load complete")

    def cli_loadvm(self, line):
        with open('vm.bin', 'rb') as f:
            data = f.read()

        try:
            self.delete_file('vm.bin')

        except IOError:
            pass

        self.put_file('vm.bin', data)

        self.set_key('vm_run', True)
        self.set_key('vm_reset', True)

    def cli_getfileid(self, line):
        file_id = self.get_file_id(line)

        return f"File ID: {file_id}" 

    def cli_datalog_show(self, line):
        try:
            data = self.get_datalog_config()

        except OSError:
            return

        s = '\nKey                     Rate (ms)\n'

        for item in data:
            s += f'{self._client.lookup_hash(item.hash)[item.hash]:20}   {item.rate}\n'

        return s

    def cli_datalog_reset(self, line):
        # get a copy of the datalog config file.
        # we can put that file back if we didn't want to reset!
        try:
            self.get_datalog_config()

        except OSError:
            return

        # delete the file
        self.delete_file('datalog_config')

        return "Datalogger configuration reset"

    def cli_datalog_delete(self, line):
        try:
            data = self.get_datalog_config()

        except OSError:
            # file not found
            return

        data_hash = catbus_string_hash(line)

        array = sapphiredata.DatalogEntryArray()

        for item in data:
            if item.hash != data_hash:
                array.append(item)

        self.put_file('datalog_config', array.pack())

    def cli_datalog_add(self, line):
        try:
            data = self.get_datalog_config()

        except OSError:
            # file not found
            data = sapphiredata.DatalogEntryArray()

        tokens = line.split()

        data_hash = catbus_string_hash(tokens[0])
        rate = int(tokens[1])

        update = False
        for item in data:
            if item.hash == data_hash:
                # update
                item.rate = rate
                update = True
                break

        if not update:
            entry = sapphiredata.DatalogEntry(hash=data_hash, rate=rate)

            data.append(entry)

        self.put_file('datalog_config', data.pack())

    def cli_batt_recorder_info(self, line):
        try:
            data = self.get_batt_records()

        except OSError:
            # file not found
            return

        data_sets = {}
        rates = {}

        record_id = None
        for item in data:

            if isinstance(item, sapphiredata.BattRecordStart):
                record_id = item.record_id

                data_sets[record_id] = []
                rates[record_id] = item.rate * 10

            elif record_id is not None:
                data_sets[record_id].append(item)


        # get remaining data
        for item in data:
            if isinstance(item, sapphiredata.BattRecordStart):
                break

            data_sets[record_id].append(item)

        
        print('')
        print('ID     Status      Volts Power   Temp')

        # for item in data:
        for record_id, data in data_sets.items():
            print(f'Record ID: {record_id} Rate: {rates[record_id]}')
            
            for item in data:
                record_type = item.flags

                volts = 2500 + item.volts * 8
                power = item.pix_power * 64
                temp = item.temp

                status = 'idle'

                if record_type == sapphiredata.BATT_RECORD_TYPE_BLANK:
                    continue

                elif record_type == sapphiredata.BATT_RECORD_TYPE_DISCHARGE:
                    status = 'discharge'

                elif record_type == sapphiredata.BATT_RECORD_TYPE_CHARGE:
                    status = 'charge'

                print(f'{record_id:5}  {status:9}   {volts:4}  {power:5} {temp:4}')

            print('')

        return ''

def createDevice(**kwargs):
    return Device(**kwargs)
