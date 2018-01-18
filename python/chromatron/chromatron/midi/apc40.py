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
import midi
import fnmatch


ABLETON_ALT_MODE = [0xF0, 0x47, 0, 0x73, 0x60, 0x00, 0x04, 0x42, 0, 0, 0, 0xF7]



button_led_modes = {
    'off':              0,
    'green':            1,
    'green_blinking':   2,
    'red':              3,
    'red_blinking':     4,
    'orange':           5,
    'orange_blinking':  6,
}

knob_led_modes = {
    'off':      0,
    'single':   1,
    'volume':   2,
    'pan':      3,
}

class InvalidControlType(Exception):
    pass


class APC40(midi.KVMidi):
    def __init__(self, port='Akai APC40', **kwargs):
        super(APC40, self).__init__(port, name='apc40', **kwargs)

        # build output maps based on control type
        self.knob_led_mode_map = fnmatch.filter(self._out_config.keys(), '*led_mode')
        self.knob_led_map = fnmatch.filter(self._out_config.keys(), '*knob')
        self.button_led_map = fnmatch.filter(self._out_config.keys(), '*led')

        # self.reset(0.005)
        self.reset(0.003)

    def reset(self, delay=0.0):
        # switch to ableton alternate mode
        self.send_message(ABLETON_ALT_MODE)

        # initialize controls to default states

        for mode in self.knob_led_mode_map:
            self.set_knob_led_mode(mode, 'volume')

        for led in self.button_led_map:
            self.set_button_led(led, 'green')
            time.sleep(delay)

        for i in xrange(127):
            for led in self.knob_led_map:
                self.set_knob_led(led, i)

            time.sleep(delay)

        for led in self.button_led_map:
            self.set_button_led(led, 'orange')
            time.sleep(delay)

        for i in xrange(127):
            for led in self.knob_led_map:
                self.set_knob_led(led, 127 - i)

            time.sleep(delay)

        for led in self.button_led_map:
            self.set_button_led(led, 'red')
            time.sleep(delay)

        for led in self.button_led_map:
            self.set_button_led(led, 'off')

            time.sleep(delay)


    def on_noteoff(self, control, value, timedelta):
        # fire on_value event for subclasses
        self.on_value(control, value)

    def on_noteon(self, control, value, timedelta):
        # fire on_value event for subclasses
        self.on_value(control, value)

    def on_controlchange(self, control, value, timedelta):
        # set knob LED
        if control in self.knob_led_map:
            self.set_knob_led(control, value)

        # fire on_value event for subclasses
        self.on_value(control, value)

    def on_value(self, control, value):
        # print "value: %s -> %s" % (control, value)
        pass

    def set_button_led(self, control, mode):
        if control not in self.button_led_map:
            raise InvalidControlType()

        self.noteon(control, button_led_modes[mode])

    def set_knob_led_mode(self, control, mode):
        if control not in self.knob_led_mode_map:
            raise InvalidControlType()

        self.controlchange(control, knob_led_modes[mode])

    def set_knob_led(self, control, value):
        if control not in self.knob_led_map:
            raise InvalidControlType()

        self.controlchange(control, value)

    def set_led(self, key, value):
        # set knob LED
        if key in self.knob_led_map:
            self.set_knob_led(key, value)

        # set button LED
        if key in self.button_led_map:
            self.set_button_led(key, value)

        # set knob LED mode
        if key in self.knob_led_mode_map:
            self.set_knob_led_mode(key, value)



if __name__ == "__main__":

    import sapphire.common.util as util

    util.setup_basic_logging()

    controller = 'Akai APC40'

    try:
        while True:
            try:
                apc40 = APC40(port=controller)

                # apc40.kv.send('track_1_fader', 'gfx_master_dimmer', ['shelf', 'office'])
                # apc40.kv.send('track_2_fader', 'gfx_master_dimmer', ['living_room_lamps'])
                # apc40.kv.send('track_3_fader', 'gfx_master_dimmer', ['bedroom'])

                # apc40.kv.send('track_1_fader', 'gfx_master_dimmer', ['shelf'])
                # apc40.kv.receive('meow', 'gfx_master_dimmer', ['shelf'])
                # apc40.kv.send('track_1_fader', 'gfx_master_dimmer', ['meow', 'woof'])
                # apc40.kv.send('track_2_fader', 'gfx_master_dimmer', ['demo'])

                # apc40.kv.send('device_1_control_knob', 'fx_hue', ['shelf'])
                # apc40.kv.send('device_2_control_knob', 'fx_sat', ['shelf'])
                # apc40.kv.send('device_3_control_knob', 'fx_val', ['shelf'])

                # apc40.kv.send('device_5_control_knob', 'fx_hue', ['demo'])
                # apc40.kv.send('device_6_control_knob', 'fx_sat', ['demo'])
                # apc40.kv.send('device_7_control_knob', 'fx_val', ['demo'])

                apc40.kv.send('track_1_fader', 'gfx_master_dimmer', ['lantern_4'])
                apc40.kv.send('track_2_fader', 'gfx_master_dimmer', ['lantern_6'])
                apc40.kv.send('track_3_fader', 'gfx_master_dimmer', ['lantern_3'])
                apc40.kv.send('track_4_fader', 'gfx_master_dimmer', ['lantern_5'])
                apc40.kv.send('track_5_fader', 'gfx_master_dimmer', ['lantern_1'])

                apc40.kv.send('track_8_fader', 'gfx_master_dimmer', ['pixelator'])


                while True:
                    time.sleep(0.5)

                    if not midi.port_connected(controller):
                        apc40.kv.stop()
                        raise midi.MidiPortNotFound

            except midi.MidiPortNotFound:
                time.sleep(1.0)

    except KeyboardInterrupt:
        pass
