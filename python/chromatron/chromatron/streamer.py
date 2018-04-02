
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


from sapphire.common import Ribbon
import socket
import time
import threading
import struct


from elysianfields import *


PIXEL_SERVER_PORT = 8004
PIXEL_MSG_MAX_LEN = 80

class PixelHSV(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="h"),
                  Uint16Field(_name="s"),
                  Uint16Field(_name="v")]

        super(PixelHSV, self).__init__(_name="pixel_hsv", _fields=fields, **kwargs)

class PixelRGB(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="r"),
                  Uint16Field(_name="g"),
                  Uint16Field(_name="b")]

        super(PixelRGB, self).__init__(_name="pixel_rgb", _fields=fields, **kwargs)

CHROMA_MSG_TYPE_HSV = 1
class PixelHSVMsg(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="type"),
                  Uint8Field(_name="flags"),
                  Uint16Field(_name="index"),
                  Uint8Field(_name="count")]
                  # ArrayField(_name="pixels", _field=PixelHSV)]
                  # ArrayField(_name="pixels", _field=Uint16Field)]

        super(PixelHSVMsg, self).__init__(_name="pixel_hsv_msg", _fields=fields, **kwargs)

        self.type = CHROMA_MSG_TYPE_HSV

CHROMA_MSG_TYPE_RGB = 2
class PixelRGBMsg(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="type"),
                  Uint8Field(_name="flags"),
                  Uint16Field(_name="index"),
                  Uint8Field(_name="count")]
                  # ArrayField(_name="pixels", _field=PixelRGB)]
                  # ArrayField(_name="pixels", _field=Uint16Field)]

        super(PixelRGBMsg, self).__init__(_name="pixel_rgb_msg", _fields=fields, **kwargs)

        self.type = CHROMA_MSG_TYPE_RGB


class PixelArray(object):
    def __init__(self, name=None, length=0, value=0.0, streamer=None):
        super(PixelArray, self).__init__()

        self.name = name
        self.length = length
        self.list = [value for a in xrange(length)]

        self.streamer = streamer
        self.streamer.register_array(self)

        self.__lock = threading.Lock()

    def check(self, v):
        if v > 1.0:
            v = 1.0

        elif v < 0.0:
            v = 0.0
    
        return v

    def _lock(self):
        self.__lock.acquire()

    def _release(self):
        self.__lock.release()

    def _get_list(self):
        return self.list

    def __len__(self): 
        return len(self.list)

    def __getitem__(self, i): 
        return self.list[i % self.length]

    def __delitem__(self, i): 
        raise TypeError("Cannot delete a pixel from array")
        
    def __setitem__(self, i, v):
        with self.__lock:
            self.list[i % self.length] = self.check(v)

    def insert(self, i, v):
        raise NotImplementedError

    def __str__(self):
        with self.__lock:
            return str(self.list)

    def __iadd__(self, other):
        with self.__lock:
            for i in xrange(len(self.list)):
                self.list[i] = self.check(other + self.list[i])

        return self

    def __isub__(self, other):
        with self.__lock:
            for i in xrange(len(self.list)):
                self.list[i] = self.check(other - self.list[i])

        return self

    def __imul__(self, other):
        with self.__lock:
            for i in xrange(len(self.list)):
                self.list[i] = self.check(other * self.list[i])

        return self

    def __idiv__(self, other):
        with self.__lock:
            for i in xrange(len(self.list)):
                self.list[i] = self.check(other / self.list[i])

        return self

    def __imod__(self, other):
        with self.__lock:
            for i in xrange(len(self.list)):
                self.list[i] = self.check(other % self.list[i])

        return self

    def set_all(self, other):
        with self.__lock:
            other = self.check(other)

            for i in xrange(len(self.list)):
                self.list[i] = other


class HueArray(PixelArray):
    def __init__(self, *args, **kwargs):
        super(HueArray, self).__init__(*args, **kwargs)

    def check(self, v):
        # hue can wrap    
        v %= 1.0
        
        return v


class RGBArray(PixelArray):
    def __init__(self, *args, **kwargs):
        super(RGBArray, self).__init__(*args, **kwargs)


class Streamer(object):
    def __init__(self, host=None):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        self.host = host

        self.arrays = {}

        self.mode = 'hsv'

    def register_array(self, array):
        self.arrays[array.name] = array

    def update_hsv(self):
        self.mode = 'hsv'
        self.update()

    def update_rgb(self):
        self.mode = 'rgb'
        self.update()

    def update(self):
        msgs = []

        if self.mode == 'hsv':
            hue = self.arrays['hue']
            sat = self.arrays['sat']
            val = self.arrays['val']

            hue._lock()
            sat._lock()
            val._lock()

            hue_list = hue._get_list()
            sat_list = sat._get_list()
            val_list = val._get_list()

            pixels = []

            i = 0
            index = 0
            for i in xrange(len(hue_list)):
                h = hue_list[i]
                s = sat_list[i]
                v = val_list[i]

                # convert to 16 bit integers
                h *= 65535
                s *= 65535
                v *= 65535
                
                pixels.append(int(h))
                pixels.append(int(s))
                pixels.append(int(v))

                if len(pixels) >= PIXEL_MSG_MAX_LEN * 3:
                    msg = PixelHSVMsg(index=index, count=len(pixels) / 3).pack()    
                    msg += struct.pack('<%dH' % (len(pixels)), *pixels)

                    msgs.append(msg)
                    index += (len(pixels) / 3)

                    pixels = []

            hue._release()
            sat._release()
            val._release()

            # get last message
            if len(pixels) >= 3:
                msg = PixelHSVMsg(pixels=pixels, index=index, count=len(pixels) / 3).pack()
                msg += struct.pack('<%dH' % (len(pixels)), *pixels)

                msgs.append(msg)
        
        # rgb mode
        elif self.mode == 'rgb':
            r = self.arrays['r']
            g = self.arrays['g']
            b = self.arrays['b']

            r._lock()
            g._lock()
            b._lock()

            r_list = r._get_list()
            g_list = g._get_list()
            b_list = b._get_list()

            pixels = []

            i = 0
            index = 0
            for i in xrange(len(r_list)):
                pix_r = r_list[i]
                pix_g = g_list[i]
                pix_b = b_list[i]

                # convert to 16 bit integers
                pix_r *= 65535
                pix_g *= 65535
                pix_b *= 65535
                
                pixels.append(int(pix_r))
                pixels.append(int(pix_g))
                pixels.append(int(pix_b))

                if len(pixels) >= PIXEL_MSG_MAX_LEN * 3:
                    msg = PixelRGBMsg(index=index, count=len(pixels) / 3).pack()    
                    msg += struct.pack('<%dH' % (len(pixels)), *pixels)

                    msgs.append(msg)
                    index += (len(pixels) / 3)

                    pixels = []

            r._release()
            g._release()
            b._release()

            # get last message
            if len(pixels) >= 3:
                msg = PixelRGBMsg(pixels=pixels, index=index, count=len(pixels) / 3).pack()
                msg += struct.pack('<%dH' % (len(pixels)), *pixels)

                msgs.append(msg)

        # transmit!
        for msg in msgs:
            self._sock.sendto(msg, (self.host, PIXEL_SERVER_PORT))

