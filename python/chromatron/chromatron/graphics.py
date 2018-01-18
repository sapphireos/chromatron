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
import os

import kivy
kivy.require('1.9.1')
from kivy.app import App
from kivy.uix.widget import Widget
from kivy.uix.button import Button
from kivy.uix.gridlayout import GridLayout
from kivy.graphics import *
from kivy.clock import Clock

from trig import sine, cosine
import math
import code_gen
import time
import threading
from sapphire.common.ribbon import Ribbon

from filewatcher import Watcher


class Pixel(object):
    def __init__(self):
        self._hue = 0.0
        self._sat = 1.0
        self._val = 0.0

        self._target_hue = 0.0
        self._target_sat = 1.0
        self._target_val = 0.0

        self._hue_step = 0.0
        self._sat_step = 0.0
        self._val_step = 0.0

        self._hs_fade = 1.0
        self._v_fade = 1.0

        self.fade_rate = 0.02

        self._fading = False

        self._lock = threading.Lock()

    def is_fading(self):
        return self._fading

    def fade(self):
        if not self._fading:
            return

        with self._lock:
            if self._hue_step != 0.0:
                diff = self._target_hue - self._hue

                if abs(diff) < abs(self._hue_step):
                    self._hue = self._target_hue
                    self._hue_step = 0.0

                else:
                    self._hue += self._hue_step

                self._hue %= 1.0

            if self._sat_step != 0.0:
                diff = self._target_sat - self._sat

                if abs(diff) < abs(self._sat_step):
                    self._sat = self._target_sat
                    self._sat_step = 0.0

                else:
                    self._sat += self._sat_step

                # saturate math
                if self._sat > 1.0:
                    self._sat = 1.0

                elif self._sat < 0.0:
                    self._sat = 0.0

            if self._val_step != 0.0:
                diff = self._target_val - self._val

                if abs(diff) < abs(self._val_step):
                    self._val = self._target_val
                    self._val_step = 0.0

                else:
                    self._val += self._val_step

                # saturate math
                if self._val > 1.0:
                    self._val = 1.0

                elif self._val < 0.0:
                    self._val = 0.0


            if (self._hue_step == 0.0) and \
               (self._sat_step == 0.0) and \
               (self._val_step == 0.0):

               self._fading = False

    @property
    def hsv(self):
        with self._lock:
            return self._hue, self._sat, self._val

    @property
    def hs_fade(self):
        return self._hs_fade

    @hs_fade.setter
    def hs_fade(self, a):
        self._hs_fade = a

    @property
    def v_fade(self):
        return self._v_fade

    @v_fade.setter
    def v_fade(self, a):
        self._v_fade = a

    @property
    def hue(self):
        return self._target_hue

    @hue.setter
    def hue(self, a):
        with self._lock:
            self._target_hue = a

            if self._target_hue == self._hue:
                return

            self._fading = True

            hs_fade_steps = int(self.hs_fade / self.fade_rate)

            # require at least 2 fade steps, this mimics hardware
            if hs_fade_steps <= 1:
                hs_fade_steps = 2

            diff = a - self._hue

            # adjust to shortest distance and allow the fade to wrap around
            # the hue circle
            if abs(diff) > 0.5:
                if diff > 0.0:
                    diff -= 1.0

                else:
                    diff += 1.0

            step = diff / hs_fade_steps

            # more limits based on hardware fader
            if step > 0.5:
                step = 0.5

            elif step < -0.5:
                step = -0.5

            elif step == 0.0:
                if diff >= 0:
                    step = 1.0/32768.0
                else:
                    step = -1.0/32768.0

            self._hue_step = step


    @property
    def sat(self):
        return self._target_sat

    @sat.setter
    def sat(self, a):
        with self._lock:
            self._target_sat = a

            if self._target_sat == self._sat:
                return

            self._fading = True

            hs_fade_steps = int(self.hs_fade / self.fade_rate)

            # require at least 2 fade steps, this mimics hardware
            if hs_fade_steps <= 1:
                hs_fade_steps = 2

            diff = a - self._sat
            step = diff / hs_fade_steps

            # more limits based on hardware fader
            if step > 0.5:
                step = 0.5

            elif step < -0.5:
                step = -0.5

            elif step == 0.0:
                if diff >= 0:
                    step = 1.0/32768.0
                else:
                    step = -1.0/32768.0

            self._sat_step = step

    @property
    def val(self):
        return self._target_val

    @val.setter
    def val(self, a):
        with self._lock:
            self._target_val = a

            if self._target_val == self._val:
                return

            self._fading = True

            v_fade_steps = int(self.v_fade / self.fade_rate)

            # require at least 2 fade steps, this mimics hardware
            if v_fade_steps <= 1:
                v_fade_steps = 2

            diff = a - self._val
            step = diff / v_fade_steps

            # more limits based on hardware fader
            if step > 0.5:
                step = 0.5

            elif step < -0.5:
                step = -0.5

            elif step == 0.0:
                if diff >= 0:
                    step = 1.0/32768.0
                else:
                    step = -1.0/32768.0

            self._val_step = step



class GraphicState(Ribbon):
    def initialize(self, width=8, height=8):
        self.width = width
        self.height = height

        self._hs_fade = 0.5
        self._v_fade = 0.5
        self._fader_rate = 0.02

        self.wrap = False

        self.coord_mode = 'abs'

        self.reset()

    def reset(self):
        self.pixels = [Pixel() for i in xrange(self.height * self.width)]

        self.update_fader_settings()

    @property
    def hs_fade(self):
        return self._hs_fade

    @hs_fade.setter
    def hs_fade(self, a):
        self._hs_fade = a
        self.update_fader_settings()

    @property
    def v_fade(self):
        return self._v_fade

    @v_fade.setter
    def v_fade(self, a):
        self._v_fade = a
        self.update_fader_settings()

    @property
    def fader_rate(self):
        return self._fader_rate

    @fader_rate.setter
    def fader_rate(self, a):
        self._fader_rate = a
        self.update_fader_settings()

    def is_fading(self, x, y):
        return self.pixels[x][y].is_fading()

    def update_fader_settings(self):
        for i in xrange(len(self.pixels)):
            self.pixels[i].hs_fade = self._hs_fade
            self.pixels[i].v_fade = self._v_fade
            self.pixels[i].fade_rate = self._fader_rate

    def calc_index(self, x, y):
        if y == 65535:
            i = x % len(self.pixels)

        else:
            x %= self.width
            y %= self.height

            i = x + (y * self.width)

        return i

    def set_hue(self, a, x, y):
        i = self.calc_index(x, y)
        self.pixels[i].hue = a

    def set_sat(self, a, x, y):
        i = self.calc_index(x, y)
        self.pixels[i].sat = a

    def set_val(self, a, x, y):
        i = self.calc_index(x, y)
        self.pixels[i].val = a

    def set_hs_fade(self, a, x, y):
        i = self.calc_index(x, y)
        self.pixels[i].hs_fade = a

    def set_v_fade(self, a, x, y):
        i = self.calc_index(x, y)
        self.pixels[i].v_fade = a

    def get_hue(self, x, y):
        i = self.calc_index(x, y)
        return self.pixels[i].hue

    def get_sat(self, x, y):
        i = self.calc_index(x, y)
        return self.pixels[i].sat

    def get_val(self, x, y):
        i = self.calc_index(x, y)
        return self.pixels[i].val

    def get_hs_fade(self, x, y):
        i = self.calc_index(x, y)
        return self.pixels[i].hs_fade

    def get_v_fade(self, x, y):
        i = self.calc_index(x, y)
        return self.pixels[i].v_fade

    def get_hsv(self, i):
        # if i == 0:
            # print self.pixels[i].hs_fade
        return self.pixels[i].hsv

    # def get_array(self, array, i):
    #     if array == code_gen.PIXELS_HUE:
    #         return self.pixels[i].hue

    #     elif array == code_gen.PIXELS_SAT:
    #         return self.pixels[i].sat

    #     elif array == code_gen.PIXELS_VAL:
    #         return self.pixels[i].val

    #     elif array == code_gen.PIXELS_HS_FADE:
    #         return self.pixels[i].hs_fade

    #     elif array == code_gen.PIXELS_V_FADE:
    #         return self.pixels[i].v_fade

    #     else:
    #         if isinstance(array, float):
    #             return array / 65535.0

    #         return array

    # def set_array(self, array, i, a):
    #     if isinstance(a, int):
    #         a = a / 65535.0

    #     if array == code_gen.PIXELS_HUE:
    #         self.pixels[i].hue = a

    #     elif array == code_gen.PIXELS_SAT:
    #         self.pixels[i].sat = a

    #     elif array == code_gen.PIXELS_VAL:
    #         self.pixels[i].val = a

    #     elif array == code_gen.PIXELS_HS_FADE:
    #         self.pixels[i].hs_fade = a

    #     elif array == code_gen.PIXELS_V_FADE:
    #         self.pixels[i].v_fade = a

    def load_arrays(self, arrays):
        for i in xrange(len(self.pixels)):
            self.pixels[i].hue = arrays['hue'][i] / 65536.0
            self.pixels[i].sat = arrays['sat'][i] / 65536.0
            self.pixels[i].val = arrays['val'][i] / 65536.0
            self.pixels[i].hs_fade = arrays['hs_fade'][i]
            self.pixels[i].v_fade = arrays['v_fade'][i]


    # def array_mov(self, dest, source):
    #     for i in xrange(len(self.pixels)):
    #         a = self.get_array(source, i)
    #         self.set_array(dest, i, a)

    # def array_op(self, dest, op, op1):
    #     for i in xrange(len(self.pixels)):
    #         a = op1

    #         if op == 'add':
    #             c = a + b

    #         elif op == 'sub':
    #             c = a - b

    #         elif op == 'mul':
    #             c = a * b

    #         elif op == 'div':
    #             c = a / b

    #         elif op == 'mod':
    #             c = a % b

    #         self.set_array(dest, i, c)


    def clear(self):
        for i in xrange(len(self.pixels)):
            self.pixels[i].val = 0.0

    def loop(self):
        next_run = time.time() + self._fader_rate

        for i in xrange(len(self.pixels)):
            self.pixels[i].fade()

        self.wait(next_run - time.time())


class FXRunner(Ribbon):
    def initialize(self, fx_file=None, width=8, height=8):
        self.fx_file = fx_file

        self.gfx = GraphicState(width=width, height=height)
        
        self.frame_rate = 0.02
        # self.frame_rate = 1.0

        self.load_vm()

        self.watcher = Watcher(fx_file)

    def load_vm(self):
        self.gfx.reset()

        self.code = code_gen.compile_script(self.fx_file)

        self.vm = code_gen.VM(
                    self.code["vm_code"],
                    self.code["vm_data"],
                    pix_size_x=self.gfx.width,
                    pix_size_y=self.gfx.height)

        self.vm.init()

    def loop(self):
        next_run = time.time() + self.frame_rate

        self.vm.loop()

        self.gfx.load_arrays(self.vm.dump_hsv())

        self.wait(next_run - time.time())

        if self.watcher.changed():
            print "Reloading VM"

            self.load_vm()


    def clean_up(self):
        self.watcher.stop()


class DisplayWindow(Widget):
    def __init__(self, gfx, border_width=1, **kwargs):
        super(DisplayWindow, self).__init__(**kwargs)

        self.gfx = gfx
        self.border_width = border_width

        self.bind(size=self.on_resize)

    def setup_pixels(self):
        # create list of rectangles (virtual pixels)
        self.pixels = []
        self.rects = []

        self.canvas.clear()

        canvas_width, canvas_height = self.size
        # NOTE! when drawing on the canvas, you MUST take into account the XY position
        # of the widget itself.  Otherwise the elements will not appear on the canvas.
        # this is not intuitive.
        canvas_x, canvas_y = self.pos

        pseudo_pixel_width = canvas_width / self.gfx.width
        pseudo_pixel_height = canvas_height / self.gfx.height

        with self.canvas:
            pseudo_x = canvas_x

            for x in xrange(self.gfx.width):
                self.rects.append([])

                # Kivy's coords put 0,0 at the bottom left, but Chromatron
                # puts it at top left.  So we draw from top down in Kivy.
                pseudo_y = canvas_y + (canvas_height - pseudo_pixel_height)

                for y in xrange(self.gfx.height):
                    self.pixels.append(Color(0.0, 0.0, 0.0, mode='hsv'))
                    self.rects[x].append(
                        Rectangle(pos=(pseudo_x, pseudo_y),
                                  size=(pseudo_pixel_width - self.border_width, pseudo_pixel_height - self.border_width)))

                    pseudo_y -= pseudo_pixel_height

                pseudo_x += pseudo_pixel_width


    def on_resize(self, width, height):
        self.setup_pixels()

    def update(self, dt):
        # start = time.time()

        for i in xrange(len(self.gfx.pixels)):

            # this will miss final updates:
            # if not self.gfx.is_fading(x, y):
                # continue

            # update hsv all at once.
            # going one at a time will cause artifacts
            h, s, v = self.gfx.get_hsv(i)
            # print h,s,v

            self.pixels[i].hsv = [h, s, v]

        # elapsed = time.time() - start
        # print elapsed * 1000.0


# class FXGrid(GridLayout):
#     def __init__(self, fx_runner=None, **kwargs):
#         super(FXGrid, self).__init__(**kwargs)

#         self.fx_runner = fx_runner
#         self.bind(size=self.on_resize)

#     def on_resize(self, width, height):
#         self.canvas.clear()
#         self.clear_widgets()

#         with self.canvas:
#             Color(0.0, 0.0, 1.0, mode='hsv')
#             Rectangle(pos=self.pos,
#                       size=self.size)


#         self.displays = []

#         for row in xrange(self.rows):
#             for column in xrange(self.cols):
#                 display_window = DisplayWindow(self.fx_runner.gfx)

#                 self.displays.append(display_window)
#                 self.add_widget(display_window)

#     def update(self, dt):
#         for disp in self.displays:
#             disp.update(dt)


class GraphicsApp(App):
    def __init__(self, fx_file, **kwargs):
        super(GraphicsApp, self).__init__(**kwargs)
        self.fx_file = fx_file

    def build(self):
        fx = FXRunner(fx_file=self.fx_file, width=4, height=4)

        # window = FXGrid(padding=10, spacing=10, cols=8, rows=5, fx_runner=fx)
        window = DisplayWindow(fx.gfx)
        Clock.schedule_interval(window.update, 1.0 / 20.0)

        return window



"""
Kivy notes:

Need to install SDL2 before installing Kivy.  If not, Kivy will use
Pygame, and window resizing won't work (you just get a black screen).

brew install sdl2 sdl2_image sdl2_ttf sdl2_mixer gstreamer
pip install -I Cython==0.23

USE_OSX_FRAMEWORKS=0 ARCHFLAGS="-arch x86_64" pip install --no-cache-dir kivy

Need the --no-cache-dir to force Kivy to rebuild against SDL2.
It won't work without it, since pip will just install the cached
version that was compiled incorrectly.

"""



if __name__ == '__main__':

    # import yappi
    # yappi.start()


    GraphicsApp(fx_file=sys.argv[1]).run()


    # yappi.get_func_stats().print_all()
