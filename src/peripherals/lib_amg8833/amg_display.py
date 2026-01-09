
import sys
import os

import math
import numpy as np
from copy import deepcopy as copy

from pprint import pprint
import kivy
# kivy.require('1.9.1')
from kivy.app import App
from kivy.uix.widget import Widget
from kivy.uix.button import Button
from kivy.uix.label import Label
from kivy.uix.gridlayout import GridLayout
from kivy.graphics import *
from kivy.clock import Clock

from sapphire.common import util
from catbus import CatbusService

import socket
import struct



class DisplayWindow(Widget):
    def __init__(self, border_width=1, **kwargs):
        super(DisplayWindow, self).__init__(**kwargs)

        self.pixels_width = 8
        self.pixels_height = 8
        
        self.border_width = border_width

        self.bind(size=self.on_resize)

        self.catbus = CatbusService(tags=['amg_display'], visible=False)


        try:
            param = sys.argv[2]

        except IndexError:
            param = 'amg_pixels'

        self.catbus.add_item('pixels', [0] * 64)
        # self.catbus.add_item('ref', [0] * 64)
        # self.catbus.add_item('therm', 0)

        self.catbus.receive(param, 'pixels', [sys.argv[1]], rate=100)
        # self.catbus.receive('amg_ref1', 'ref', [sys.argv[1]], rate=100)
        # self.catbus.receive('amg_temp', 'therm', [sys.argv[1]], rate=100)

        self.min_temp = None
        self.max_temp = None

    def setup_pixels(self):
        # create list of rectangles (virtual pixels)
        self.pixels = []
        self.rects = []
        self.labels = []

        self.canvas.clear()

        canvas_width, canvas_height = self.size
        # NOTE! when drawing on the canvas, you MUST take into account the XY position
        # of the widget itself.  Otherwise the elements will not appear on the canvas.
        # this is not intuitive.
        canvas_x, canvas_y = self.pos

        pseudo_pixel_width = canvas_width / self.pixels_width
        pseudo_pixel_height = canvas_height / self.pixels_height

        with self.canvas:
            
            # pseudo_y = canvas_y + (canvas_height - pseudo_pixel_height)
            pseudo_x = 0

            for x in range(self.pixels_width):
            # for y in xrange(self.pixels_height):
            
                self.rects.append([])

                # pseudo_x = canvas_x
                pseudo_y = canvas_y + ( self.pixels_height * pseudo_pixel_height ) - pseudo_pixel_height

                # Kivy's coords put 0,0 at the bottom left, but Chromatron
                # puts it at top left.  So we draw from top down in Kivy.
                # for x in xrange(self.pixels_width):    
                for y in range(self.pixels_height):
                    self.pixels.append(Color(0.0, 0.0, 0.0, mode='hsv'))
                    # self.rects[y].append(
                    self.rects[x].append(
                        Rectangle(pos=(pseudo_x, pseudo_y),
                                  size=(pseudo_pixel_width - self.border_width, pseudo_pixel_height - self.border_width)))

                    l = Label(pos=(pseudo_x, pseudo_y), text='test', markup=True, font_size='30')
                    l.grid_x = x
                    l.grid_y = y
                    self.labels.append(l)
                    # pseudo_x += pseudo_pixel_width
                    pseudo_y -= pseudo_pixel_height

                # pseudo_y -= pseudo_pixel_height
                pseudo_x += pseudo_pixel_width

         
                    
    def on_resize(self, obj, size):
        self.setup_pixels()

    def update(self, dt):
        s = 1.0
        v = 1.0

        try:
            pixels = self.catbus['pixels']
            # ref = self.catbus['ref']
            # therm = self.catbus['therm'] / 100.0

        except KeyError:
            return

        try:
            pixels = [float(a) / 1.0 for a in pixels] 

        except TypeError:
            return

        print('%f' % (pixels[51]))

        if self.min_temp == None:
            self.min_temp = min(pixels)
            self.max_temp = max(pixels)

        if min(pixels) < self.min_temp:
            self.min_temp = min(pixels)

        if max(pixels) > self.max_temp:
            self.max_temp = max(pixels)

        if self.min_temp == self.max_temp:
            self.max_temp = self.min_temp + 1.0

        # self.min_temp = 90.0
        # self.max_temp = 120.0
        
        for i in range(len(pixels)):
            self.labels[i].text = '[color=000000]%d (%d, %d)\n%3.2f[/color]' % \
                    (i,
                    self.labels[i].grid_x, 
                    self.labels[i].grid_y, 
                    pixels[i])

            if pixels[i] < self.min_temp:
                h = 0.667
            else:
                h = ((pixels[i] - self.min_temp) / (self.max_temp - self.min_temp) * 0.6) + 0.667


            self.pixels[i].hsv = [h, s, v]

    


class GraphicsApp(App):
    def __init__(self, **kwargs):
        super(GraphicsApp, self).__init__(**kwargs)

    def build(self):
        window = DisplayWindow()
        Clock.schedule_interval(window.update, 1.0 / 10.0)

        return window






if __name__ == '__main__':

    GraphicsApp().run()
