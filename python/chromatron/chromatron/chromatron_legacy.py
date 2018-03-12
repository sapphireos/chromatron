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

import sys
import time
import os
import socket
import types
import threading
import json
from UserDict import DictMixin
import getpass
import zipfile
import hashlib
import pkg_resources
from filewatcher import Watcher
from sapphire.buildtools import firmware_package
import json

from sapphire.devices.legacydevice import Device, DeviceUnreachableException
from elysianfields import *
from sapphire.common.util import now

import streamer

import catbus
import code_gen

import click

try:
    VERSION = pkg_resources.get_distribution('chromatron').version
except pkg_resources.DistributionNotFound:
    VERSION = 'dev'

BACKUP_FILE = 'backup.json'

BACKUP_SETTINGS = [
    'catbus_data_port',
    'enable_led_quiet',
    'enable_sntp',
    'max_log_size',
    'gfx_balance_blue',
    'gfx_balance_green',
    'gfx_balance_red',
    'gfx_dimmer_curve',
    'gfx_frame_rate',
    'gfx_hsfade',
    'gfx_vfade',
    'gfx_interleave_x',
    'gfx_master_dimmer',
    'gfx_sub_dimmer',
    'gfx_sync_group',
    'gfx_transpose',
    'gfx_virtual_array_length',
    'gfx_virtual_array_start',
    'meta_tag_0',
    'meta_tag_1',
    'meta_tag_2',
    'meta_tag_3',
    'meta_tag_4',
    'meta_tag_5',
    'meta_tag_name',
    'meta_tag_location',
    'pix_apa102_dimmer',
    'pix_clock',
    'pix_count',
    'pix_dither',
    'pix_mode',
    'pix_rgb_order',
    'pix_size_x',
    'pix_size_y',
    'sntp_server',
    'sntp_sync_interval',
    'vm_run',
    'vm_prog',
    'vm_shutdown_prog',
    'vm_startup_prog',
    'wifi_ap_password',
    'wifi_ap_ssid',
    'wifi_enable_ap',
    'wifi_password',
    'wifi_ssid',
]


NAME_COLOR = 'cyan'
HOST_COLOR = 'magenta'
VAL_COLOR = 'white'
KEY_COLOR = 'blue'
ERROR_COLOR = 'red'
SUCCESS_COLOR = 'green'

CHROMATRON_SERVICE_NAME = "sapphire.device.chromatron"

CHROMATRON_WIFI_FWID = '7148b7b4-2526-487d-9a5b-bfc1d6b0259b'.replace('-', '')

F_PER = 32000000


led_modes = {
    'off': 0,
    'ws2801': 1,
    'apa102': 2,
    'ws2811': 3,
    'pixie': 4,
    'sk6812_rgbw': 5,
    'analog': 128
}

led_types = {
    'off': 'off',
    'pwm': 'analog',
    'ws2801': 'ws2801',
    'apa102': 'apa102',
    'ws2811': 'ws2811',
    'ws2812': 'ws2811',
    'ws2812b': 'ws2811',
    'pixie': 'pixie',
    'sk6812': 'ws2811',
    'sk6812_rgbw': 'sk6812_rgbw',
}

rgb_order = {
    'rgb': 0,
    'rbg': 1,
    'grb': 2,
    'bgr': 3,
    'brg': 4,
    'gbr': 5,
}

PIXEL_SETTINGS = [
    'pix_apa102_dimmer',
    'pix_clock',
    'pix_count',
    'pix_dither',
    'pix_mode',
    'pix_rgb_order',
    'pix_size_x',
    'pix_size_y',
]

# EXPOSED_KEYS = [
#     'gfx_param0',
#     'gfx_param1',
#     'gfx_param2',
#     'gfx_param3',
#     'gfx_param4',
#     'gfx_param5',
#     'gfx_param6',
#     'gfx_param7',
#     'pix_dither',
#     'pix_rgb_order',
#     'gfx_master_dimmer',
#     'gfx_sub_dimmer',
#     'gfx_hsfade',
#     'gfx_vfade',
#     'gfx_frame_rate',
#     'gfx_dimmer_curve',
#     'gfx_interleave_x',
# ]
#
# EXPOSED_KEYS_READONLY = [
#     'supply_voltage',
#     'vcc'
# ]


def get_package_fx_script(fname):
    return pkg_resources.resource_filename('chromatron', fname)

class FirmwarePackage(object):
    def __init__(self, filename='chromatron_fw_package.zip'):
        if filename:
            self.open(filename)

    def open(self, filename='chromatron_fw_package.zip'):
        self.filename = filename

        zf = zipfile.ZipFile(filename)
        zf.extractall('.firmware')
        zf.close()

        os.chdir('.firmware')

        zf = zipfile.ZipFile('chromatron_main_fw.zip')
        zf.extractall('chromatron_fw')
        zf.close()

        os.chdir('chromatron_fw')

        # read manifest
        with open('manifest.txt', 'r') as f:
            data = json.loads(f.read())

            self.ct_fw_timestamp = data['timestamp']
            self.ct_fw_version = data['version']
            self.ct_fw_sha256 = data['sha256']

        # read bin data
        with open('firmware.bin', 'rb') as f:
            self.ct_fw_data = f.read()

        os.chdir('..')

        zf = zipfile.ZipFile('chromatron_wifi_fw.zip')
        zf.extractall('chromatron_wifi_fw')
        zf.close()

        os.chdir('chromatron_wifi_fw')

        # read manifest
        with open('manifest.txt', 'r') as f:
            data = json.loads(f.read())

            self.ct_wifi_fw_timestamp = data['timestamp']
            self.ct_wifi_fw_version = data['version']
            self.ct_wifi_fw_md5 = data['md5']
            self.ct_wifi_fw_sha256 = data['sha256']

        # read bin data
        with open('wifi_firmware.bin', 'rb') as f:
            self.ct_wifi_fw_data = f.read()

        os.chdir('..')

        # verify data
        fw_sha256 = hashlib.sha256(self.ct_fw_data)
        assert fw_sha256.hexdigest() == self.ct_fw_sha256

        # note that wifi firmware has the MD5 appended,
        # so we need to remove that for the calculation
        wifi_fw_md5 = hashlib.md5(self.ct_wifi_fw_data[:-16])
        assert wifi_fw_md5.hexdigest() == self.ct_wifi_fw_md5

        # also verify the SHA256
        wifi_fw_sha256 = hashlib.sha256(self.ct_wifi_fw_data)
        assert wifi_fw_sha256.hexdigest() == self.ct_wifi_fw_sha256


    def __str__(self):
        s = 'AVR Firmware\nv%-10s\nSHA256: %s\nTimestamp: %s\n\nWifi Firmware:\nv%-10s\nSHA256: %s\nTimestamp: %s' % \
            (self.ct_fw_version,
             self.ct_fw_sha256,
             self.ct_fw_timestamp,
             self.ct_wifi_fw_version,
             self.ct_wifi_fw_sha256,
             self.ct_wifi_fw_timestamp)

        return s


# note - this is just a convenience wrapper around the
# underlying Device object.
class Chromatron(object):
    def __init__(self, host, init_scan=True, force_network=False):
        if (host == None) or (host == 'usb'):
            host = 'usb'

        else:
            try:
                socket.inet_aton(host)

            except (socket.error, TypeError):
                # try to find a matching device
                client = catbus.Client()
                matches = client.discover(host)

                if len(matches) > 1:
                    click.echo('Found more than 1 matching device!')
                    return

                host = matches.values()[0]['host'][0]


        self._device = Device(host=host)
        self.host = self._device.host

        self.client = catbus.Client()

        self.streamer = streamer.Streamer()

        self.force_network = force_network

        if init_scan:
            self.init_scan()


    def init_scan(self):
        self._device.scan(get_all=False)
        self._update_meta()
        self.firmware_version = self._device.firmware_version

        network_host = (self._device.get_key('ip'), self._device.get_key('catbus_data_port'))

        if ((network_host[0] != '0.0.0.0') and (self.host != 'usb')) or self.force_network:
            self.client.connect(network_host)

        try:
            pix_count = self.get_key('pix_count')

        except KeyError:
            pix_count = 0

        self.streamer.host = network_host[0]

        self.__dict__['hue'] = streamer.HueArray(name='hue', length=pix_count, streamer=self.streamer)
        self.__dict__['sat'] = streamer.PixelArray(name='sat', length=pix_count, value=1.0, streamer=self.streamer)
        self.__dict__['val'] = streamer.PixelArray(name='val', length=pix_count, value=0.0, streamer=self.streamer)

        self.__dict__['r'] = streamer.RGBArray(name='r', length=pix_count, value=0.0, streamer=self.streamer)
        self.__dict__['g'] = streamer.RGBArray(name='g', length=pix_count, value=0.0, streamer=self.streamer)
        self.__dict__['b'] = streamer.RGBArray(name='b', length=pix_count, value=0.0, streamer=self.streamer)


    def set_all_hsv(self, h=-1, s=-1, v=-1):
        """Set HSV on all pixels"""

        if h >= 0.0:
            self.hue = h

        if s >= 0.0:
            self.sat = s

        if v >= 0.0:
            self.val = v

        self.update_pixels()

    def set_all_rgb(self, r=-1, g=-1, b=-1):
        """Set RGB on all pixels"""

        if r >= 0.0:
            self.r = r

        if g >= 0.0:
            self.g = g

        if b >= 0.0:
            self.b = b

        self.update_pixels()

    def update_pixels(self, mode='hsv'):
        """Transfer pixel data to hardware"""
        
        if mode == 'rgb':
            self.streamer.update_rgb()

        else:
            self.streamer.update_hsv()

    def __str__(self):
        return "%32s@%16s" % (self.name, self.host)

    def to_dict(self):
        return self._device.to_dict()

    def _update_meta(self):
        self.device_id = self._device.device_id
        self.name = self._device.name

        if len(self.name) == 0:
            self.name = 'anonymous@%s' % (self.host)

    def __setattr__(self, name, value):
        if isinstance(name, streamer.PixelArray) or isinstance(value, streamer.PixelArray):
            # prevents breakage when doing in-place ops on HSV arrays from a DeviceGroup.
            return

        if name in ['hue', 'sat', 'val', 'r', 'g', 'b']:
            self.__dict__[name].set_all(value)
        
        else:
            super(Chromatron, self).__setattr__(name, value)


    # def __getattr__(self, name):
    #     if name in self.__dict__:
    #         return self.__dict__[name]
    #
    #     elif name in EXPOSED_KEYS:
    #         return self._device.get_key(name)
    #
    #     elif name in EXPOSED_KEYS_READONLY:
    #         return self._device.get_key(name)
    #
    #     else:
    #         raise KeyError
    #
    # def __setattr__(self, name, value):
    #     if name in self.__dict__:
    #         self.__dict__[name] = value
    #
    #     elif name in EXPOSED_KEYS:
    #         self._device.set_key(name, value)
    #
    #     else:
    #         raise KeyError

    @property
    def dimmer(self):
        try:
            return float(self.get_key('gfx_master_dimmer') / 65535.0)

        except KeyError:
            return 0.0

    @dimmer.setter
    def dimmer(self, value):
        if value < 0.0:
            value = 0.0

        elif value > 1.0:
            value = 1.0

        value = int(value * 65535)

        try:
            self.set_key('gfx_master_dimmer', value)

        except KeyError:
            pass

    @property
    def sub_dimmer(self):
        try:
            return float(self.get_key('gfx_sub_dimmer') / 65535.0)

        except KeyError:
            return 0.0

    @sub_dimmer.setter
    def sub_dimmer(self, value):
        if value < 0.0:
            value = 0.0

        elif value > 1.0:
            value = 1.0

        value = int(value * 65535)

        try:
            self.set_key('gfx_sub_dimmer', value)

        except KeyError:
            pass

    def load_firmware(self, fw):
        # remove old firmware image
        self.delete_file('firmware.bin')

        # load firmware image
        self.put_file('firmware.bin', fw['image']['binary'])

        # reboot to loader
        self._device.reboot_and_load_fw()

    def get_version(self):
        main_ver = self._device.firmware_version
        raw_wifi_ver = self.get_key('wifi_version')

        major = (raw_wifi_ver >> 12) & 0x0f
        minor = (raw_wifi_ver >> 7) & 0x1f
        patch = (raw_wifi_ver >> 0) & 0x7f

        wifi_ver = '%d.%d.%d' % (major, minor, patch)

        return main_ver, wifi_ver

    def echo(self):
        self._device.echo()

    def get_key(self, param):
        if self.client.is_connected():
            return self.client.get_key(param)

        else:
            return self._device.get_key(param)

    def set_key(self, param, value):
        if self.client.is_connected():
            self.client.set_key(param, value)

        else:
            self._device.set_key(param, value)

    def get_keys(self, *args):
        if self.client.is_connected():
            return self.client.get_keys(*args)

        else:
            return self._device.get_kv(*args)

    def set_keys(self, **kwargs):
        if self.client.is_connected():
            self.client.set_keys(**kwargs)

        else:
            self._device.set_kv(**kwargs)

    def list_keys(self):
        if self.client.is_connected():
            return self.client.get_all_keys()

        else:
            return self._device.get_all_kv()

    def set_wifi(self, ssid, password):
        self.set_key('wifi_ssid', ssid)
        self.set_key('wifi_password', password)

    def reboot(self):
        self._device.reboot()

    def load_vm(self, filename=None, start=True, bin_data=None):
        self.stop_vm()

        if bin_data:
            code = code_gen.compile_text(bin_data)["stream"]
            bin_filename = 'vm.fxb'

        elif filename:
            code = code_gen.compile_script(filename)["stream"]
            bin_filename = os.path.split(filename)[1] + 'b'

        try:
            self.delete_file(bin_filename)

        except IOError:
            pass

        self.put_file(bin_filename, code)

        # change vm program
        self.set_key('vm_prog', bin_filename)

        if start:
            self.start_vm()

    def reset_vm(self):
        self.set_key('vm_reset', True)

    def start_vm(self):
        self.set_key('vm_run', True)

    def stop_vm(self):
        self.set_key('vm_run', False)

    def clean_vm_files(self):
        """Deletes all .fxb files from device"""
        for fname in self.list_files():
            try:
                name, ext = fname.split('.')

            except ValueError:
                continue

            if ext == 'fxb':
                self.delete_file(fname)

    def get_vm_reg(self, regname):
        assert self.client.is_connected()

        if not regname.startswith('fx_'):
            regname = 'fx_' + regname

        # must go through the client to get VM dynamic keys.
        # the USB interface can only retrieve static keys.
        return self.client.get_key(regname)

    def delete_file(self, filename):
        self._device.delete_file(filename)

    def list_files(self):
        return self._device.list_files()

    def put_file(self, filename, data, progress=None):
        return self._device.put_file(filename, data, progress=progress)

    def get_file(self, filename, progress=None):
        return self._device.get_file(filename, progress=progress)

    def check_file(self, filename, data):
        return self._device.check_file(filename, data)

    def calc_pixel_clock(self, speed):
        if speed > 3200000:
            speed = 3200000
            # raise ValueError("Maximum pixel clock is 3.200 MHz")

        elif speed < 250000:
            speed = 250000
            # raise ValueError("Minimum pixel clock is 250 KHz")

        bsel = (F_PER / (2 * speed)) - 1

        actual = F_PER / (2 * (bsel + 1))

        return bsel, actual

    def set_pixel_clock(self, speed):
        bsel, actual = self.calc_pixel_clock(speed)

        self.set_key('pix_clock', bsel)

        return bsel, actual

    def get_pixel_clock(self):
        # check pixel mode
        # PWM mode does not use clock
        # pix_mode = self.get_key('pix_mode')

        bsel = self.get_key('pix_clock')
        actual = F_PER / (2 * (bsel + 1))

        return actual

    def get_pixel_settings(self):
        settings = {}

        for setting in PIXEL_SETTINGS:
            settings[setting] = self.get_key(setting)

        return settings

    def set_pixel_mode(self, pix_type):
        led_mode = led_modes[led_types[pix_type]]

        self.set_key('pix_mode', led_mode)

    def get_pixel_mode(self):
        mode = self.get_key('pix_mode')

        for k, v in led_modes.iteritems():
            if mode == v:
                return k

    def reset_pixels(self):
        self.stop_vm()
        self.set_pixel_mode('off')
        self.set_pixel_clock(0)
        self.set_key('pix_count', 0)
        self.set_key('pix_apa102_dimmer', 31)
        self.set_key('pix_dither', False)
        self.set_key('pix_rgb_order', 0)
        self.set_key('gfx_master_dimmer', 65535)
        self.set_key('gfx_hsfade', 1000)
        self.set_key('gfx_vfade', 1000)
        self.set_key('gfx_frame_rate', 100)
        self.set_key('gfx_interleave_x', False)
        self.set_key('gfx_transpose', False)


class DeviceGroup(DictMixin, object):
    def __init__(self, *args, **kwargs):
        try:
            host = kwargs['host']
        except KeyError:
            host = None

        self.group = {}
        self.matches = {}
        self.tags = args
        self.from_file = False

        if host == None and len(args) == 0:
            self.load_from_file()

        elif host != None and host.lower() == 'usb':
            self.matches[0] = {'host': ('usb', 0)}

        elif (len(args) > 0) or (host == 'all'):

            if host == 'all':
                # need empty list for an all query
                args = []

            # try to find a matching device
            client = catbus.Client()

            matches = client.discover(*args)
            self.matches = matches

        elif host != None:
            # check if IP address
            try:
                socket.inet_aton(host)

                ct = Chromatron(host=host, init_scan=False)
                ct.echo()

                self.matches[0] = {'host': (ct.host, 0)}

            except (socket.error, TypeError):
                raise

        else:
            raise Exception()

        self.scan_devices()

        self.timestamp = now()

    def __str__(self):
        return "Group: %d devices @ %s" % (len(self.group), self.tags)

    def keys(self):
        """Return list of keys"""
        return self.group.keys()

    # def __setattr__(self, name, value):
    #     if name not in self.__dict__:
    #         self.__dict__[name] = value

    #     elif name in ['hue', 'sat', 'val', 'r', 'g', 'b']:
    #         for d in self.group.itervalues():
    #             setattr(d, name, value)

    #             print d, name, value
        
    #     else:
    #         self.__dict__[name] = value

    def __getitem__(self, key):
        return self.group[key]

    def __delitem__(self, key):
        del self.group[key]
        del self.matches[key]

    def scan_devices(self):
        self.group = {}

        scan_group = []
        threads = []

        def scan_func(device):
            device.init_scan()

        for match in self.matches.itervalues():
            ct = Chromatron(host=match['host'][0], init_scan=False)
            scan_group.append(ct)

            t = threading.Thread(target=scan_func, args=[ct])
            t.start()

            threads.append(t)

        for t in threads:
            t.join()

        for ct in scan_group:
            try:
                self.group[ct.device_id] = ct

            except AttributeError:
                pass

        # collect methods from this class.
        local_methods = []
        for f in [f for f in dir(self) if
                    isinstance(self.__getattribute__(f), types.MethodType)]:

            local_methods.append(f)


        if len(self.group) > 0:
            # collect methods from underlying driver.
            # only adds methods that aren't used by the parent class.

            methods = []

            for f in [f for f in dir(self.group.values()[0]) if
                        isinstance(self.group.values()[0].__getattribute__(f), types.MethodType) and
                        not f.startswith('_')]:

                if f not in local_methods:
                    methods.append(f)

            for f in [f for f in dir(self.group.values()[0]._device) if
                        isinstance(self.group.values()[0]._device.__getattribute__(f), types.MethodType) and
                        not f.startswith('_')]:

                if f not in local_methods and f not in methods:
                    methods.append(f)

            for method in methods:
                self.__dict__[method] = self.make_func(method)

    def to_dict(self):
        d = {}

        d['timestamp'] = self.timestamp.isoformat()
        d['tags'] = self.tags
        d['matches'] = self.matches

        return d

    def save_to_file(self, filename='chromatron_last_query.db'):
        with open(filename, 'w+') as f:
            f.write(json.dumps(self.to_dict()))

    def load_from_file(self, filename='chromatron_last_query.db'):
        with open(filename, 'r') as f:
            data = json.loads(f.read())

            self.from_file = True

            self.timestamp = data['timestamp']
            self.tags = data['tags']
            self.matches = {}

            # need to convert device id back to a number
            # JSON only does string keys
            for k, v in data['matches'].iteritems():
                self.matches[int(k)] = v

        return self

    def make_func(self, f):
        def wrapper(*args, **kwargs):
            results = {}
            for d in self.group.itervalues():
                try:
                    method = d.__getattribute__(f)

                except AttributeError:
                    method = d._device.__getattribute__(f)

                results[d.device_id] = method(*args, **kwargs)

            return results

        return wrapper

    @property
    def dimmer(self):
        results = {}
        for d in self.group.itervalues():
            results[d.device_id] = d.dimmer

        return results

    @dimmer.setter
    def dimmer(self, value):
        for d in self.group.itervalues():
            d.dimmer = value

    @property
    def sub_dimmer(self):
        results = {}
        for d in self.group.itervalues():
            results[d.device_id] = d.sub_dimmer

        return results

    @sub_dimmer.setter
    def sub_dimmer(self, value):
        for d in self.group.itervalues():
            d.sub_dimmer = value

    # @property
    # def hue(self):
    #     return []

    # @property
    # def sat(self):
    #     return []

    # @property
    # def val(self):
    #     return []

    # @hue.setter
    # def hue(self, value):
    #     for d in self.group.itervalues():
    #         d.hue = value

    # @sat.setter
    # def sat(self, value):
    #     for d in self.group.itervalues():
    #         d.sat = value

    # @val.setter
    # def val(self, value):
    #     for d in self.group.itervalues():
    #         d.val = value


def echo_name(ct, left_align=True, nl=True):
    if left_align:
        name_s = '%-32s @ %20s' % (click.style('%s' % (ct.name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))

    else:
        name_s = '%32s @ %20s' % (click.style('%s' % (ct.name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))

    click.echo(name_s, nl=nl)

def echo_group(group):
    for ct in group.itervalues():
        echo_name(ct, left_align=False)


@click.group(invoke_without_command=True)
@click.pass_context
@click.option('--host', '-h', default=None, help='Name or IP address of target.  Can also specify USB for local connection.')
@click.option('--query', '-q', default=None, multiple=True, help="Query for matching tags.")
@click.version_option(VERSION, message='v%(version)s')
def cli(ctx, host, query):
    """Chromatron Command Line Interface

    All of these commands, except for a few setup commands, can run on groups
    of devices.  If the host or query options are passed, the command will
    discover matching devices and run on those results. If options are not
    provided, the command will run on the results from the last invocation of
    the discover command.

    Examples:

    \b
    chromatron.py discover --query Camelot
        Finds and stores all devices with a query tag matching Camelot.

    \b
    chromatron.py firmware version
        List firmware version on all devices matching the previous discover,
        in this case, devices located in Camelot.

    \b
    chromatron.py --query King_Arthur firmware version
        List firmware version on all devices with a tagged with King_Arthur.  
        This will not overwrite the previous discovery results.

    \b
    chromatron.py firmware version
        Since no options are given, this will use the previous discovery, again
        matching devices located in Camelot.  This does not re-run the
        discovery, so if a device is added or removed from Camelot, the discover
        command would need to be run again.

    """

    # If there's a way to tell if Click got passed --help, I haven't been
    # able to find it.  Instead, I'm just going to search through the
    # command line args directly.
    for arg in sys.argv:
        if arg == '--help':
            # bail out, so we don't run a discovery when we're displaying help.
            return

    ctx.obj['HOST'] = host
    ctx.obj['QUERY'] = query

    if ctx.invoked_subcommand is None:
        click.echo("No command given.  Run with --help for documentation.")
        return

    if (len(query) == 0) and (ctx.invoked_subcommand == 'discover') and (host == None):
        host = 'all'

    if 'all' in query:
        host = 'all'

    def get_group():
        if query or host:
            click.echo('Discovering...')

        group = DeviceGroup(*query, host=host)

        if group.from_file:
            click.echo('%d devices in current group' % (len(group)))

        else:
            click.echo('Found %d devices' % (len(group)))

            if len(group) == 0:
                return None

        return group

    ctx.obj['GROUP'] = get_group

@cli.group()
def wifi():
    """Wifi setup commands"""

@wifi.command('monitor')
@click.pass_context
def wifi_monitor(ctx):
    """Monitor Wifi signal strength
    """

    group = ctx.obj['GROUP']()

    try:
        while True:
            rssis = {}

            for ct_id in sorted(group):
                ct = group[ct_id]
                rssi = ct.get_key('wifi_rssi')
                rssi_s = click.style(' %2d  ' % (rssi), fg=VAL_COLOR)

                rssis[ct_id] = rssi_s


            click.clear()
            s = 'Device                                Signal'
            click.echo(s)
            s = '                                      dB'
            click.echo(s)

            for ct_id in sorted(group):
                ct = group[ct_id]

                echo_name(ct, nl=False)
                click.echo(rssis[ct_id])

            time.sleep(1.0)

    except KeyboardInterrupt:
        pass


@wifi.command('reset')
@click.pass_context
def reset_wifi(ctx):
    """Reset wifi configuration to default.

    This command only works over USB.

    """
    try:
        ct = Chromatron('usb')

    except DeviceUnreachableException:
        click.echo("No device found, check USB connection.")
        click.echo("This command cannot run over Wifi.")
        return

    click.secho("Wifi config reset:", fg='cyan')

    if click.confirm(click.style("Are you sure?", fg='magenta')):
        ct.set_wifi('', '')

        click.echo('Wifi configuration reset.  Rebooting device.')

        ct.reboot()

    else:
        click.echo('No changes made.')

@wifi.command('setup')
@click.option('--wifi_ssid', default=None, help='Wifi router')
@click.option('--wifi_password', default=None, help='Wifi password')
def setup_wifi(wifi_ssid, wifi_password):
    """Configure wifi settings over USB.

    Connects to a device over USB and prompts for wifi parameters.  If multiple
    devices are connected to USB, only one of them will be selected.

    This command ignores discovery parameters and only works over USB.
    This is to prevent accidentally breaking the wifi configuration of
    an entire group of devices.  If you really need to change wifi parameters
    of a device over wifi, see the keys subcommand.

    You can pass the SSID and password on the command line as a shortcut.

    """
    try:
        ct = Chromatron('usb')

    except DeviceUnreachableException:
        click.echo("No device found, check USB connection.")
        click.echo("This command cannot run over Wifi.")
        return

    current_wifi_ssid = ct.get_key('wifi_ssid')

    if current_wifi_ssid:
        click.echo("Current wifi SSID is set to %s" % (click.style(current_wifi_ssid, fg=NAME_COLOR)))
        if click.confirm("Are you sure you want to change it?"):
            current_wifi_ssid = None

    if not current_wifi_ssid:
        if not wifi_ssid:
            wifi_ssid = click.prompt('Wifi SSID', type=str)

            if not wifi_ssid:
                click.echo("No SSID specified!")
                return

        if not wifi_password:
            wifi_password = getpass.getpass('Wifi password: ')

            if not wifi_password:
                click.echo("No password specified!")
                return


        ct.set_wifi(wifi_ssid, wifi_password)

        click.echo('Configuring wifi and rebooting, please wait...')

        ct.reboot()
        time.sleep(4.0)

        connected = False

        for i in xrange(50):
            try:
                ct.get_key('wifi_status_reg')

                click.echo('Finished rebooting, waiting for connection')
                break

            except DeviceUnreachableException:
                pass

            time.sleep(0.5)

        for i in xrange(50):
            try:
                if ct.get_key('wifi_status_reg') > 0:
                    connected = True
                    break

                else:
                    click.echo('.', nl=False)

            except DeviceUnreachableException:
                pass

            time.sleep(0.5)

        click.echo('\n')

        if not connected:
            click.echo('Unable to connect!')

            return

        click.echo('Connected!')




@cli.group()
@click.pass_context
def meta(ctx):
    """Meta data setup commands"""

@meta.command('reset')
@click.pass_context
@click.option('--force', default=False, is_flag=True, help='Force meta data reset without prompting for confirmation.')
def reset_meta(ctx, force):
    """Reset meta data to default"""

    if not force:
        click.secho("Meta data reset:", fg='cyan')
        click.secho("Affected devices will not be queryable after this operation.", fg='magenta')
        if not click.confirm("Are you sure?"):
            click.echo("No changes made.")
            return

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        for key in catbus.META_TAGS:
            ct.set_key(key, '')

        click.echo(" meta data reset")

@meta.command('setup')
@click.pass_context
def setup_meta(ctx):
    """Set meta data on devices.

    Meta data is:

    name
    location
    
    and up to 6 additional tags

    Each item is an ASCII string, up to 32 characters.

    """

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct)

        current_name = ct.get_key(catbus.META_TAG_NAME)
        current_loc = ct.get_key(catbus.META_TAG_LOC)

        name_s      = click.prompt("Name", default=current_name)
        location_s  = click.prompt("Location", default=current_loc)
        
        ct.set_key(catbus.META_TAG_NAME, str(name_s))
        ct.set_key(catbus.META_TAG_LOC, str(location_s))

        for i in xrange(catbus.META_TAG_GROUP_COUNT):
            tag_name = 'meta_tag_%d' % (i)
            tag_s =  click.prompt("Tag %d" % (i), default=ct.get_key(tag_name))

            if len(tag_s) == 0:
                break

            ct.set_key(tag_name, str(tag_s))

        click.echo(" done.")

@meta.command('set_name')
@click.pass_context
@click.argument('tag')
def meta_set_name(ctx, tag):
    """Set name on devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        ct.set_key(catbus.META_TAG_NAME, str(tag))

@meta.command('set_location')
@click.pass_context
@click.argument('tag')
def meta_set_location(ctx, tag):
    """Set location on devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        ct.set_key(catbus.META_TAG_LOC, str(tag))

@meta.command('add_tag')
@click.pass_context
@click.argument('tag')
def add_meta_tag(ctx, tag):
    """Add a meta data tag to devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        ct.set_key('meta_cmd_add', str(tag))

@meta.command('rm_tag')
@click.pass_context
@click.argument('tag')
def rm_meta_tag(ctx, tag):
    """Remove a meta data tag from devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        ct.set_key('meta_cmd_rm', str(tag))

@meta.command('list')
@click.pass_context
def list_meta_tag(ctx):
    """List meta data tags on devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        
        for meta in catbus.META_TAGS:
            tag = ct.get_key(meta)
            tag_name = meta.replace('meta_tag_', '')
            click.echo('%20s %32s' % (click.style(tag_name, KEY_COLOR), click.style(tag, VAL_COLOR)))

@cli.command()
@click.pass_context
def show(ctx):
    """Show current devices"""

    group = ctx.obj['GROUP']()

    if group == None:
        return

    s = 'Device                                Signal Uptime   Volts  Location         Tags (0-1)'
    click.echo(s)
    s = '                                      dB     Sec. '
    click.echo(s)

    for ct in group.itervalues():
        name = ct.name
        rssi = ct.get_key('wifi_rssi')
        uptime = ct.get_key('wifi_uptime')
        supply_voltage = ct.get_key('supply_voltage')  / 1000.0
        location = ct.get_key(catbus.META_TAG_LOC)
        tag_0 = ct.get_key('meta_tag_0')
        tag_1 = ct.get_key('meta_tag_1')
        
        name_s = '%32s @ %20s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))

        rssi_s = click.style('%2d  ' % (rssi), fg=VAL_COLOR)
        uptime_s = click.style('%8d ' % (uptime), fg=VAL_COLOR)
        volts_s = click.style('%5.2f ' % (supply_voltage), fg=VAL_COLOR)
        location_s = click.style('%-16s' % (location), fg=VAL_COLOR)
        tag_0_s = click.style('%-16s' % (tag_0), fg=VAL_COLOR)
        tag_1_s = click.style('%-16s' % (tag_1), fg=VAL_COLOR)

        s = '%s %s %s %s %s %s %s' % \
            (name_s, rssi_s, uptime_s, volts_s, location_s, tag_0_s, tag_1_s)

        click.echo(s)

    return group

@cli.command()
@click.pass_context
def discover(ctx):
    """Discover Chromatron devices on the network

    Discovers devices matching host or tag options. The results of this
    command will be stored to a file, and will be used as the default set of
    devices for all other commands.

    You can specify multiple tags to narrow the search.  Only devices matching
    all tags will be found.

    If host or tags are not specified, discover will find all devices.

    Note that host and tag options should be passed before discover:

    \b
    This will work:
        chromatron.py --tag Camelot discover

    \b
    This will not:
        chromatron.py discover --tag Camelot

    """

    group = ctx.invoke(show)

    if group != None:
        group.save_to_file()

@cli.command()
@click.pass_context
def ping(ctx):
    """Ping devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        start = time.time()
        ct.echo()
        elapsed = int((time.time() - start) * 1000)

        name_s = '%32s @ %20s' % (click.style('%s' % (ct.name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))
        val_s = click.style('%3d ms' % (elapsed), fg=VAL_COLOR)

        s = '%s %s' % (name_s, val_s)

        click.echo(s)

@cli.command()
@click.pass_context
def identify(ctx):
    """Identify devices"""

    group = ctx.obj['GROUP']()

    # save VM run state, so we can restore after we finish
    vm_states = group.get_key('vm_run')

    click.echo('Identify found %d devices in query' % (len(group)))
    click.echo('Stopping VMs...')

    # stop all VMs
    group.set_key('vm_run', False)

    # set all to red
    group.set_all_hsv(h=0.0, s=1.0, v=1.0)

    click.echo('\nAll devices in the group set to RED.')
    click.echo('Currently selected device will be BLUE.')
    click.echo('Previous devices will be OFF.\n')

    marked = {}

    for ct in group.itervalues():
        name_s = '%-32s @ %20s' % (click.style('%s' % (ct.name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))

        click.echo('%s %s' % (name_s, ''), nl=False)

        # set to blue
        ct.set_all_hsv(h=0.667, s=1.0, v=1.0)

        if click.confirm("Mark this device?"):
            marked[ct.device_id] = ct

            click.echo('marked')

        # set to off
        ct.set_all_hsv(v=0.0)

    click.echo("\nFinished scanning through devices")

    click.echo("%d devices marked" % (len(marked)))

    set_group_to_marked = False

    if len(marked) > 0:
        for ct in marked.itervalues():
            ct.set_all_hsv(h=0.333, s=1.0, v=1.0)

            name_s = '%-32s @ %20s' % (click.style('%s' % (ct.name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))
            click.echo('%s %s' % (name_s, 'marked'))

        click.echo("All marked devices set to GREEN.")

        if click.confirm("Set marked devices to current discovery group?"):
            set_group_to_marked = True


    click.echo('\nIdentify finished, restarting VMs that were running')

    # restore VM states
    for ct in group.itervalues():
        ct.set_key('vm_run', vm_states[ct.device_id])


    if set_group_to_marked:
        # remove non-marked devices from group
        for ct in group.values():
            if ct.device_id not in marked:
                del group[ct.device_id]

        group.save_to_file()



@cli.command()
@click.pass_context
def reboot(ctx):
    """Reboot devices"""

    group = ctx.obj['GROUP']()
    group.reboot()

    click.echo("Rebooted:")

    echo_group(group)

@cli.group()
@click.pass_context
def automaton(ctx):
    """Automaton controls"""


@automaton.command('load')
@click.pass_context
@click.argument('filename')
@click.option('--live', default=None, is_flag=True, help='Live mode')
def automaton_load(ctx, filename, live):
    """Compile and load script to automaton"""

    import automaton
    group = ctx.obj['GROUP']()

    if live:
        click.secho('Live mode', fg='magenta')


    try:
        automaton.compile_file(filename)

        with open('automaton.auto') as f:
            data = f.read()

        group.put_file('automaton.auto', data)
        
        click.echo('Loaded %s on:' % (click.style(filename, fg=VAL_COLOR)))

        echo_group(group)

    except Exception as e:
        click.secho("Error:", fg='magenta')
        click.secho(str(e), fg=ERROR_COLOR)


    if live:
        watcher = Watcher(filename)

        try:
            while True:
                time.sleep(0.5)

                if watcher.changed():
                    try:
                        automaton.compile_file(filename)   
                        
                        with open('automaton.auto') as f:
                            data = f.read()

                        group.put_file('automaton.auto', data)

                        click.echo('Loaded %s' % (click.style(filename, fg=VAL_COLOR)))

                    except Exception as e:
                        click.secho("Error:", fg='magenta')
                        click.secho(str(e), fg=ERROR_COLOR)


        except KeyboardInterrupt:
            pass

        watcher.stop()

@cli.group()
@click.pass_context
def vm(ctx):
    """Virtual machine controls"""

@vm.command()
@click.pass_context
def start(ctx):
    """Start virtual machine"""
    group = ctx.obj['GROUP']()
    group.start_vm()

    click.echo("Started VM on:")

    echo_group(group)

@vm.command()
@click.pass_context
def stop(ctx):
    """Stop virtual machine"""

    group = ctx.obj['GROUP']()
    group.stop_vm()

    click.echo("Stopped VM on:")

    echo_group(group)


@vm.command('reset')
@click.pass_context
def vm_reset(ctx):
    """Reset virtual machine"""
    group = ctx.obj['GROUP']()
    group.reset_vm()

    click.echo("Reset VM on:")

    echo_group(group)


@vm.command()
@click.pass_context
@click.argument('filename')
@click.option('--live', default=None, is_flag=True, help='Live mode')
def load(ctx, filename, live):
    """Compile and load script to virtual machine"""

    group = ctx.obj['GROUP']()

    if live:
        click.secho('Live mode', fg='magenta')


    try:
        group.load_vm(filename)
        click.echo('Loaded %s on:' % (click.style(filename, fg=VAL_COLOR)))

        echo_group(group)

    except Exception as e:
        click.secho("Error:", fg='magenta')
        click.secho(str(e), fg=ERROR_COLOR)


    if live:
        watcher = Watcher(filename)

        try:
            while True:
                time.sleep(0.5)

                if watcher.changed():
                    try:
                        group.load_vm(filename)
                        click.echo('Loaded %s' % (click.style(filename, fg=VAL_COLOR)))

                    except Exception as e:
                        click.secho("Error:", fg='magenta')
                        click.secho(str(e), fg=ERROR_COLOR)


        except KeyboardInterrupt:
            pass

        watcher.stop()

@vm.command()
@click.pass_context
def reload(ctx):
    """Recompile and reload the FX script on device"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        try:
            prog = ct.get_key('vm_prog')

            filename, ext = os.path.splitext(prog)
            filename += '.fx'        

            ct.load_vm(filename)

            click.echo(" %s" % (filename))
            
        except KeyError:
            click.echo(" No VM program - skipping")

        except IOError:
            click.echo(" File not found: %s" % (filename))

@cli.command()
@click.pass_context
@click.argument('filename')
@click.option('--debug', default=False, is_flag=True, help='Print debug information during script compilation')
def compile(ctx, filename, debug):
    """Compile an FX script"""

    click.echo('Compiling: %s' % (filename))

    code = code_gen.compile_script(filename, debug_print=debug)["stream"]

    bin_filename = filename + 'b'

    with open(bin_filename, 'wb+') as f:
        f.write(code)

        click.echo('Saved as: %s' % (bin_filename))


@vm.command()
@click.pass_context
def clean(ctx):
    """Erase all VM script files (.fxb)"""

    group = ctx.obj['GROUP']()
    group.clean_vm_files()

    click.echo('Cleaned VM files on:')

    echo_group(group)

@cli.group()
@click.pass_context
def fs(ctx):
    """File system commands"""

@fs.command('list')
@click.pass_context
def ct_list(ctx):
    """List files"""
    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct)

        files = ct.list_files()

        for name in sorted(files):
            f = files[name]

            v = ''

            if f['flags'] == 1:
                v = 'V'

            s = "%1s %6d %s" % \
                (v,
                 f['size'],
                 f['filename'])

            click.echo(s)

@fs.command()
@click.pass_context
@click.argument('filename')
def cat(ctx, filename):
    """View a file"""
    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct)

        try:
            data = ct.get_file(filename)

            click.echo(data)

        except IOError:
            click.echo(" %s not found" % (click.style(filename, fg=ERROR_COLOR)))

@fs.command()
@click.pass_context
@click.argument('filename')
def rm(ctx, filename):
    """Remove a file"""
    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        try:
            ct.delete_file(filename)
            click.echo(" %s deleted" % (click.style(filename, fg=VAL_COLOR)))

        except IOError:
            click.echo(" %s not found" % (click.style(filename, fg=ERROR_COLOR)))

@fs.command()
@click.pass_context
@click.argument('filename')
def put(ctx, filename):
    """Put a file"""
    group = ctx.obj['GROUP']()

    f = open(filename, 'rb')
    data = f.read()
    f.close()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        try:
            ct.put_file(filename, data)
            click.echo(" %s loaded" % (click.style(filename, fg=VAL_COLOR)))

        except IOError:
            click.echo(" %s not loaded" % (click.style(filename, fg=ERROR_COLOR)))


@cli.group()
def keys():
    """Key-value system commands.

    These commands provide direct access to the key-value database.

    Use caution when setting system keys.  Actions cannot be undone, and a
    careless setting may require a complete reset and reconfiguration of the
    device.

    """

@keys.command()
@click.pass_context
def list(ctx):
    """List all keys on devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct)

        data = ct.list_keys()
        keys = sorted(data.keys())

        for k in keys:
            v = data[k]
            click.echo('    %-40s %s' % (click.style(k, fg=KEY_COLOR), click.style(str(v), fg=VAL_COLOR)))


@keys.command()
@click.argument('key')
@click.pass_context
def get(ctx, key):
    """Get key on devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        try:
            val = ct.get_key(key)

        except Exception as e:
            val = str(e)

        click.echo(' %s' % (click.style(str(val), fg=VAL_COLOR)))


@keys.command()
@click.argument('key')
@click.argument('value')
@click.pass_context
def set(ctx, key, value):
    """Set key on devices"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        try:
            ct.set_key(key, value)
            val = ct.get_key(key)

        except Exception as e:
            val = str(e)

        click.echo(' %s set to %s' % (key, click.style(str(val), fg=VAL_COLOR)))



@cli.group()
@click.pass_context
def dimmer(ctx):
    """Dimmer commands"""

@dimmer.command('master')
@click.pass_context
@click.argument('value', type=float)
def dimmer_set_master_dimmer(ctx, value):
    """Set master dimmer.

    VALUE is from 0.0 to 1.0.
    """

    group = ctx.obj['GROUP']()

    if value > 1.0:
        click.echo(click.style('Value must be between 0.0 and 1.0', fg=ERROR_COLOR))
        return

    click.echo("Setting master dimmer:")

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        
        ct.dimmer = value


@dimmer.command('sub')
@click.pass_context
@click.argument('value', type=float)
def dimmer_set_sub_dimmer(ctx, value):
    """Set submaster dimmer.

    VALUE is from 0.0 to 1.0.
    """

    group = ctx.obj['GROUP']()

    if value > 1.0:
        click.echo(click.style('Value must be between 0.0 and 1.0', fg=ERROR_COLOR))
        return

    click.echo("Setting submaster dimmer:")

    for ct in group.itervalues():
        echo_name(ct, nl=True)
        
        ct.sub_dimmer = value

@dimmer.command('show')
@click.pass_context
def dimmer_show_dimmers(ctx):
    """Show current dimmer settings"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        master_dimmer = click.style("%01.3f" % (ct.dimmer), fg=VAL_COLOR)
        sub_dimmer = click.style("%01.3f" % (ct.sub_dimmer), fg=VAL_COLOR)

        click.echo(" Master: %s Sub: %s" % (master_dimmer, sub_dimmer))




@cli.group()
@click.pass_context
def pixels(ctx):
    """Pixel commands"""

@pixels.command('reset')
@click.pass_context
def reset_pixels(ctx):
    """Reset pixel configuration to default"""

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)
        ct.reset_pixels()
        click.echo(" pixels reset")

@pixels.command('setup')
@click.pass_context
def setup_pixels(ctx):
    """Configure pixel driver settings.

    This command will set up pixel type, number of pixels, and RGB order.

    """

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        click.clear()
        echo_name(ct)
        current_settings = ct.get_pixel_settings()
        # click.echo(current_settings)

        ct.stop_vm()

        pix_type = None

        while pix_type == None:
            click.echo("Supported pixel chipsets/protocols:")
            for led_type in sorted(led_types.keys()):
                click.secho(led_type, fg=VAL_COLOR)

            pix_type = click.prompt('Enter pixel type', type=str)

            if pix_type not in led_types:
                pix_type = None

                click.secho("Unrecognized LED type.", fg=ERROR_COLOR)

                continue

            else:
                break

        # set pixel clock to default
        ct.set_pixel_clock(1000000)

        click.echo("Setting pixel type to: %s" % (click.style(pix_type, fg=VAL_COLOR)))
        ct.set_key('gfx_vfade', 100)
        ct.set_key('gfx_hsfade', 100)
        ct.set_key('gfx_master_dimmer', 16384)
        ct.set_key('gfx_sub_dimmer', 65535)

        if pix_type != 'pwm':
            pix_count = None

            while True:
                click.echo('\n')
                pix_count = click.prompt('Enter pixel count', type=int)
                ct.set_key('pix_count', pix_count)
                ct.set_pixel_mode(pix_type)
                ct.load_vm(get_package_fx_script('pix_count_test.fx'))

                click.secho("Confirming pixel count.", fg=NAME_COLOR)
                click.secho("The first and last pixels should be on.", fg=NAME_COLOR)
                click.secho("Only these two pixels should be on.", fg=NAME_COLOR)
                click.secho("If no pixels are on, disconnect power and check wiring.", fg='magenta')

                if click.confirm("Are the first and last pixels on?"):
                    click.secho("Pixel count set.", fg=SUCCESS_COLOR)
                    break

                else:
                    click.secho("Incorrect number of pixels, we'll try again.", fg=ERROR_COLOR)
                    
        else:
            ct.set_pixel_mode(pix_type)

        click.echo('\n')
        click.secho("Setting up RGB order.", fg=NAME_COLOR)

        while True:
            order = None

            ct.stop_vm()
            ct.set_key('pix_rgb_order', rgb_order['rgb'])

            # Notes:
            # pix_test_red turns on pixels in position 0
            # pix_test_green turns on pixels in position 1
            # pix_test_blue turns on pixels in position 2

            ct.load_vm(get_package_fx_script('pix_test_red.fx'))
            if click.confirm("Are the pixels %s?" % (click.style('red', fg='red'))):
                # red in position 0

                ct.load_vm(get_package_fx_script('pix_test_green.fx'))
                if click.confirm("Are the pixels %s?" % (click.style('green', fg='green'))):
                    # green in position 1
                    order = 'rgb'
                else:
                    # green must be in position 2
                    order = 'rbg'

            if order == None:
                ct.load_vm(get_package_fx_script('pix_test_red.fx'))
                if click.confirm("Are the pixels %s?" % (click.style('green', fg='green'))):
                    # green in position 0

                    ct.load_vm(get_package_fx_script('pix_test_green.fx'))
                    if click.confirm("Are the pixels %s?" % (click.style('red', fg='red'))):
                        # red in position 1
                        order = 'grb'

                    else:
                        # red must be in position 2
                        order = 'gbr'

            if order == None:
                ct.load_vm(get_package_fx_script('pix_test_red.fx'))
                if click.confirm("Are the pixels %s?" % (click.style('blue', fg='blue'))):
                    # blue in position 0

                    ct.load_vm(get_package_fx_script('pix_test_green.fx'))
                    if click.confirm("Are the pixels %s?" % (click.style('red', fg='red'))):
                        # red in position 1
                        order = 'brg'

                    else:
                        # red must be in position 2
                        order = 'bgr'

            if order == None:
                click.secho("That doesn't seem right.", fg=ERROR_COLOR)

                if click.confirm("Are the pixels on?"):
                    click.secho("OK, let's try again.", fg='magenta')

                    continue

                else:
                    click.secho("Something must be wrong, try running this command again.", fg=ERROR_COLOR)
                    return

            click.echo('\n')
            click.echo("Setting RGB order to:")

            s = ''
            for a in order:
                if a == 'r':
                    s += click.style("Red ", fg='red')

                elif a == 'g':
                    s += click.style("Green ", fg='green')

                elif a == 'b':
                    s += click.style("Blue ", fg='blue')

            click.echo(s)
            ct.set_key('pix_rgb_order', rgb_order[order])

            click.echo('\n')
            click.echo("Confirm RGB order:")

            ct.load_vm(get_package_fx_script('pix_test_red.fx'))
            if not click.confirm("Are the pixels %s?" % (click.style('red', fg='red'))):
                click.secho("Incorrect RGB ordering, we'll try again.", fg=ERROR_COLOR)
                continue

            ct.load_vm(get_package_fx_script('pix_test_green.fx'))
            if not click.confirm("Are the pixels %s?" % (click.style('green', fg='green'))):
                click.secho("Incorrect RGB ordering, we'll try again.", fg=ERROR_COLOR)
                continue

            ct.load_vm(get_package_fx_script('pix_test_blue.fx'))
            if not click.confirm("Are the pixels %s?" % (click.style('blue', fg='blue'))):
                click.secho("Incorrect RGB ordering, we'll try again.", fg=ERROR_COLOR)
                continue

            break

        click.echo('\n')
        click.secho("RGB order set.", fg=SUCCESS_COLOR)

        # set faders back to 1000 ms default
        ct.set_key('gfx_vfade', 1000)
        ct.set_key('gfx_hsfade', 1000)

        # clean test files
        for f in ['pix_test_red.fxb', 'pix_test_green.fxb', 'pix_test_blue.fxb', 'pix_count_test.fxb']:
            try:
                ct.delete_file(f)

            except IOError:
                pass

        click.echo("Loading rainbow pattern.  Master dimmer set to 25%.")

        ct.set_key('gfx_master_dimmer', 16384)
        ct.set_key('gfx_sub_dimmer', 65535)
        ct.load_vm(get_package_fx_script('rainbow.fx'))

        # delay to make sure the device has had time to save parameters
        time.sleep(2.0)

        click.echo('\n')
        click.secho("Pixel setup complete.", fg=SUCCESS_COLOR)


@pixels.command('set_clock')
@click.pass_context
@click.argument('value', type=int)
def pixel_set_clock(ctx, value):
    """Set pixel clock.

    VALUE is in Hz.

    Valid range is 250 KHz to 3.2 MHz.

    The default pixel clock is set to 1 MHz.  It is not recommended to change
    this.

    The signal quality will begin to degrade above 2 MHz, and operation
    above this range is not guaranteed.

    The WS2811/WS2812 mode uses a fixed clock and will override any setting
    attempted through this command.

    This setting is ignored in PWM mode.
    """

    group = ctx.obj['GROUP']()

    clock_s = click.style('%d Hz' % (value), fg=VAL_COLOR)

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        setting, actual = ct.set_pixel_clock(value)

        actual_s = click.style('%d Hz' % (actual), fg=VAL_COLOR)

        click.echo(" Requested clock: %s Achieved: %s" % (clock_s, actual_s))

@pixels.command('get_clock')
@click.pass_context
def pixel_get_clock(ctx):
    """Get pixel clock.

    Returns current pixel clock in Hz.
    """

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        # check if analog, if so,
        # indicate that we aren't using the
        # pixel clock
        pix_mode = ct.get_pixel_mode()
        if pix_mode == 'analog' or pix_mode == 'off':
            actual_clock_s = click.style('%s' % ('N/A'), fg=VAL_COLOR)

        else:
            actual = ct.get_pixel_clock()
            actual_clock_s = click.style('%d Hz' % (actual), fg=VAL_COLOR)

        click.echo(" %s" % (actual_clock_s))

@pixels.command('show_settings')
@click.pass_context
def pixel_show_settings(ctx):
    """Show pixel settings.

    Shows pixel mode, clock, and pixel count.

    Note that some of the pixel modes map to multiple pixel types.
    This command will return the name of the pixel mode used by
    Chromatron internally, and may not match the name used to configure
    the device.

    Example: ws2811, ws2812, and ws2812b internally all use the ws2811 mode.

    """

    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        echo_name(ct, nl=False)

        # check if analog, if so,
        # indicate that we aren't using the
        # pixel clock
        pix_mode = ct.get_pixel_mode()
        if pix_mode == 'analog' or pix_mode == 'off':
            actual_clock_s = click.style('%s' % ('N/A'), fg=VAL_COLOR)

        else:
            actual = ct.get_pixel_clock()
            actual_clock_s = click.style('%d Hz' % (actual), fg=VAL_COLOR)


        mode_s = click.style('%s' % (pix_mode), fg=VAL_COLOR)

        pix_count = ct.get_key('pix_count')
        count_s = click.style('%s' % (pix_count), fg=VAL_COLOR)

        click.echo(" Mode: %16s Clock: %20s Count: %s" % (mode_s, actual_clock_s, count_s))



@pixels.command()
@click.pass_context
@click.argument('value')
def hue(ctx, value):
    """Set hue on all pixels"""
    group = ctx.obj['GROUP']()

    value = float(value)

    for ct in group.itervalues():
        echo_name(ct)

        ct.set_all_hsv(h=value)


@pixels.command()
@click.pass_context
@click.argument('value')
def sat(ctx, value):
    """Set sat on all pixels"""
    group = ctx.obj['GROUP']()

    value = float(value)

    for ct in group.itervalues():
        echo_name(ct)

        ct.set_all_hsv(s=value)


@pixels.command()
@click.pass_context
@click.argument('value')
def val(ctx, value):
    """Set val on all pixels"""
    group = ctx.obj['GROUP']()

    value = float(value)

    for ct in group.itervalues():
        echo_name(ct)

        ct.set_all_hsv(v=value)


@cli.group()
@click.pass_context
def firmware(ctx):
    """Firmware commands"""

@firmware.command()
@click.pass_context
def update(ctx):
    """Update releases"""

    new_releases = firmware_package.update_releases()

    if len(new_releases) == 0:
        click.echo('No updates found')

    else:
        click.echo('Updated releases:')

        for release in new_releases:
            click.echo(click.style('%-32s' % (release), fg='white'))

@firmware.command()
@click.pass_context
def releases(ctx):
    """List available releases"""

    releases = firmware_package.get_releases(use_date_for_key=True)

    if len(releases) == 0:
        click.echo("No firmware releases found.  Try running 'chromatron firmware update' to update packages.")

    else:
        for published in reversed(sorted(releases.keys())):
            name = releases[published]
            name_s = click.style('%s' % (name), fg='white')
            timestamp_s = click.style('%s' % (published), fg='magenta')

            click.echo('%-32s Published :%32s' % (name_s, timestamp_s))   

@firmware.command()
@click.pass_context
@click.option('--release', '-r', default=None, help='Name of release to display')
def manifest(ctx, release):
    """Show manifest for current firmware package"""

    if release == None:
        release = firmware_package.get_most_recent_release()

    release_date = firmware_package.get_releases()[release]
    
    name_s = click.style('%s' % (release), fg='white')
    timestamp_s = click.style('%s' % (release_date), fg='magenta')

    click.echo('Release: %-32s Published :%32s\n' % (name_s, timestamp_s))   

    click.echo('Contents:')   

    firmwares = firmware_package.get_firmware(include_firmware_image=True, release=release)

    for name, info in firmwares.iteritems():
        name_s = click.style('%s' % (info['manifest']['name']), fg='white')
        version_s = click.style('%s' % (info['manifest']['version']), fg='cyan')
        timestamp_s = click.style('%s' % (info['manifest']['timestamp']), fg='magenta')

        click.echo(name_s)
        if info['image']['valid']:
            click.echo('%32s Ver:%20s Built:%32s' % (info['short_name'], version_s, timestamp_s))

        else:
            click.echo('    Ver:%20s %s' % (version_s, click.style('IMAGE CHECKSUM FAIL', fg='red')))

@firmware.command()
@click.pass_context
def backup(ctx):
    """Backup settings on selected devices"""

    group = ctx.obj['GROUP']()

    
    try:
        with open(BACKUP_FILE, 'r') as f:
            backup_data = json.loads(f.read())

    except IOError:
        backup_data = {}

    for ct in group.itervalues():
        echo_name(ct)

        click.echo(click.style('Backing up settings', fg='white'))
        
        backup_data[ct.get_key('device_id')] = ct.get_keys(*BACKUP_SETTINGS)

    with open(BACKUP_FILE, 'w+') as f:
        f.write(json.dumps(backup_data))


@firmware.command()
@click.pass_context        
def restore(ctx):
    """Restore settings to selected devices"""

    group = ctx.obj['GROUP']()

    try:
        with open(BACKUP_FILE, 'r') as f:
            backup_data = json.loads(f.read())

    except IOError:
        click.echo(click.style('No backup data found', fg=ERROR_COLOR))
        return

    for ct in group.itervalues():
        echo_name(ct)

        click.echo(click.style('Restoring settings', fg='white'))
        
        try:
            device_data = backup_data[str(ct.get_key('device_id'))]

        except KeyError:
            click.echo(click.style('No backup data found', fg=ERROR_COLOR))
            return

        ct.set_keys(**device_data)


@firmware.command()
@click.pass_context
@click.option('--release', '-r', default=None, help='Name of release to load. Default is latest.')
@click.option('--force', default=False, is_flag=True, help='Force firmware upgrade even if versions match.')
@click.option('--change_firmware', default=None, help='Change firmware on device.')
@click.option('--yes', default=False, is_flag=True, help='Answer yes to all firmware change confirmation prompts.')
def upgrade(ctx, release, force, change_firmware, yes):
    """Upgrade firmware on selected devices"""
    # this weirdness is to deal with the difference in how Click handles progress updates
    # vs the device driver.
    # Click expects an indication as to how far to advance the bar.
    # the driver callback itself sends the actual file position.
    # this construct deals with this.
    class Progress(object):
        def __init__(self, bar):
            self.pos = 0
            self.bar = bar

        def __call__(self, pos):
            updated_pos = pos - self.pos

            if updated_pos == 0:
                return

            self.bar.update(updated_pos)
            self.pos = pos

    group = ctx.obj['GROUP']()

    if release == None:
        release = firmware_package.get_most_recent_release()

    release_date = firmware_package.get_releases()[release]
    
    name_s = click.style('%s' % (release), fg='white')
    timestamp_s = click.style('%s' % (release_date), fg='magenta')

    click.echo('\nRelease: %-32s Published :%32s\n' % (name_s, timestamp_s))   

    if not click.confirm(click.style("Is this the release you intend to use?", fg='white')):
        click.echo("Firmware upgrade cancelled")
        return
        

    firmwares = firmware_package.get_firmware(include_firmware_image=True, sort_fwid=True, release=release)

    # check if the yes option is enabled, and get a final confirmation
    if change_firmware and yes:
        click.echo('Firmware change requested!')
        click.echo(click.style("BE AWARE: Loading incorrect firmware can brick hardware!", fg='red'))

        if not click.confirm(click.style("Are you sure you want to do this?\nThere will be no further confirmation prompts.", fg='red')):
            click.echo("Firmware change cancelled")
            return
                

    # we're going to manually run though the update sequence,
    # since it is kind of messy to try to get the click progress bar
    # into the Chromatron.load_firmware() method.

    for ct in group.itervalues():
        echo_name(ct)

        firmware_loaded = False

        # get firmware version
        fw_version = ct.get_key('firmware_version')
        fw_id = ct.get_key('firmware_id')

        # check if we are changing firmawre
        if change_firmware:
            new_fwid = None
            # look up firmware id
            for info in firmwares.itervalues():
                if info['short_name'] == change_firmware:
                    new_fwid = info['manifest']['fwid'].replace('-', '')

            if new_fwid == None:
                click.echo("Matching firmware image not found!")
                return

            old_name = ct.get_key('firmware_name')
            click.echo("Changing firmware from: %s to %s" % (old_name, firmwares[new_fwid]['short_name']))

            if not yes:
                click.echo(click.style("BE AWARE: Loading incorrect firmware can brick hardware!", fg='red'))

                if not click.confirm(click.style("Are you sure you want to do this?", fg='red')):
                    click.echo("Firmware change cancelled")
                    return
                
            # ok, we're going through with the change
            fw_id = new_fwid            

            click.echo("Proceeding with firmware change")


        # check if we have the firmware ID we need
        if fw_id not in firmwares:
            click.echo("Matching firmware image not found!")
            click.echo("Skipping this device...")
            continue

        ct_fw_version = firmwares[fw_id]['manifest']['version']
        ct_fw_data = firmwares[fw_id]['image']['binary']

        # check if device already has this version
        if (fw_version == ct_fw_version) and (not force):
            click.echo("Main firmware version already loaded")

        else:
            firmware_loaded = True

            # remove old firmware image
            ct.delete_file('firmware.bin')

            # load firmware
            with click.progressbar(length=len(ct_fw_data), label='Loading main CPU firmware  ') as progress_bar:
                ct.put_file('firmware.bin', ct_fw_data, progress=Progress(progress_bar))

            # verify
            try:
                click.echo("Verifying... ", nl=False)

                ct.check_file('firmware.bin', ct_fw_data)

                click.echo("OK")

            except IOError:
                click.echo(click.style("Firmware verify fail!", fg='red'))
                return

        # get firmware version
        wifi_md5 = ct.get_key('wifi_md5')

        if CHROMATRON_WIFI_FWID not in firmwares:
            click.echo("Wifi firmware image not found!")
            continue

        ct_wifi_fw_md5 = firmwares[CHROMATRON_WIFI_FWID]['manifest']['md5']
        ct_wifi_fw_data = firmwares[CHROMATRON_WIFI_FWID]['image']['binary']

        # check if device already has this version
        if (wifi_md5 == ct_wifi_fw_md5) and (not force):
            click.echo("Wifi firmware version already loaded")

        else:
            firmware_loaded = True

            # remove old wifi firmware
            try:
                ct.delete_file('wifi_firmware.bin')

            except IOError: # file not found
                pass

            with click.progressbar(length=len(ct_wifi_fw_data), label='Loading wifi CPU firmware  ') as progress_bar:
                ct.put_file('wifi_firmware.bin', ct_wifi_fw_data, progress=Progress(progress_bar))


            # verify
            try:
                click.echo("Verifying... ", nl=False)

                ct.check_file('wifi_firmware.bin', ct_wifi_fw_data)

                click.echo("OK")

            except IOError:
                click.echo(click.style("Firmware verify fail!", fg='red'))
                return

            wifi_fw_len = len(ct_wifi_fw_data) - 16
            ct.set_key('wifi_fw_len', wifi_fw_len)
            ct.set_key('wifi_md5', ct_wifi_fw_md5)


        if firmware_loaded:
            click.echo('Rebooting...')

            # reboot to loader
            ct._device.reboot_and_load_fw()

            click.echo('Waiting for device')

            for i in xrange(100):
                if i > 10:
                    # don't start trying to echo too soon,
                    # because the reboot command takes a few seconds
                    # to process.
                    try:
                        ct.echo()

                        break

                    except DeviceUnreachableException:
                        pass

                click.echo('.', nl=False)

                time.sleep(0.5)

            click.echo('.')

        # make sure we're connected
        try:
            ct.echo()

            # confirm firmware versions
            fw_version = ct.get_key('firmware_version')
            wifi_md5 = ct.get_key('wifi_md5')

            if fw_version != ct_fw_version:
                click.echo(click.style('Upgrade failed!  Main firmware version mismatch.', fg='red'))
                return

            elif wifi_md5 != ct_wifi_fw_md5:
                click.echo(click.style('Upgrade failed!  Wifi firmware version mismatch.', fg='red'))
                return

            click.echo(click.style('Upgrade complete', fg='green'))

        except DeviceUnreachableException:
            click.echo(click.style('Failed to connect to device!', fg='red'))
            return


@firmware.command()
@click.pass_context
def version(ctx):
    """Show firmware version on selected devices"""
    group = ctx.obj['GROUP']()

    for ct in group.itervalues():
        main_version, wifi_firmware_version = ct.get_version()
        fw_name = ct.get_key('firmware_name')

        name_s = '%32s @ %20s' % (click.style('%s' % (ct.name), fg=NAME_COLOR), click.style('%s' % (ct.host), fg=HOST_COLOR))
        val_s = '%32s Main: %s Wifi: %s' % (click.style(fw_name, fg='white'), click.style(main_version, fg='cyan'), click.style(wifi_firmware_version, fg='cyan'))

        s = '%s %s' % (name_s, val_s)

        click.echo(s)



def main():
    cli(obj={})

if __name__ == '__main__':
    main()

    sys.exit(0)


    d1 = DeviceGroup('arc')
    d2 = DeviceGroup('flux')

    print d1
    print d2

    # ct = Chromatron('10.0.0.120')


    # print ct

    # # import yappi
    # # yappi.start()


    try:
        d1.val = 1.0
        d2.val = 1.0

        for d in d1.itervalues():
            print d
            d.val = 1.0

        for d in d2.itervalues():
            print d
            d.val = 1.0


        while True:
            time.sleep(0.02)

            for d in d1.itervalues():
            #     print d
                d.hue += 0.01
            #     print "D"

            # d = d1.values()[0]

            # d.hue += 0.01

            # print "E"

            for d in d2.itervalues():
                d.hue += 0.01

            d1.update_pixels()
            d2.update_pixels()


            # for i in xrange(len(ct.val)):
            #     if i == 0:
            #         ct.hue[0] = ct.hue[1] + 0.5

            #     else:
            #         ct.hue[i] += 0.01

            #     # ct.val[i] += 0.01
            # ct.val += 0.01

            # # ct.hue = 0.0

            # ct.update_pixels()


    except KeyboardInterrupt:
        pass

    # # yappi.get_func_stats().print_all()

