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

import rtmidi
import time
import midi_config
from sapphire.keyvalue import KVService


NOTE_OFF            = 0x80
NOTE_ON             = 0x90
CONTROL_CHANGE      = 0xB0



class MidiPortNotFound(Exception):
    pass


class Midi(object):
    def __init__(self, port=None, **kwargs):
        super(Midi, self).__init__()

        cfg = midi_config.load_config()

        self._in_config = cfg['midi_in'][port]
        self._out_config = cfg['midi_out'][port]

        self._handlers = {
            NOTE_OFF: self.on_noteoff,
            NOTE_ON: self.on_noteon,
            CONTROL_CHANGE: self.on_controlchange,
        }

        self._msg_decode = {
            NOTE_OFF: 'note',
            NOTE_ON: 'note',
            CONTROL_CHANGE: 'control'
        }

        self._port = port
        self._midiout = rtmidi.MidiOut()
        self._midiin = rtmidi.MidiIn()

        available_output_ports = list_output_ports()
        available_input_ports = list_input_ports()

        selected_output_port = None
        selected_input_port = None

        try:
            for i in xrange(len(available_output_ports)):
                if available_output_ports[i].find(port) >= 0:
                    selected_output_port = i
                    break

            for i in xrange(len(available_input_ports)):
                if available_input_ports[i].find(port) >= 0:
                    selected_input_port = i
                    break

            self._midiout.open_port(selected_output_port)
            self._midiin.open_port(selected_input_port)

        except:
            raise MidiPortNotFound(port)

        self._midiin.set_callback(self._on_message)

    def _on_message(self, msg, data):
        # decode message into a more useful format
        midi_msg = msg[0]
        timedelta = msg[1]

        msg_type = midi_msg[0] & 0xf0
        channel = midi_msg[0] & 0x0f
        note = midi_msg[1]
        value = midi_msg[2]

        control = self._in_config[(channel, note, self._msg_decode[msg_type])]

        if msg_type in self._handlers:
            self._handlers[msg_type](control, value, timedelta)

    def on_noteoff(self, control, value, timedelta):
        pass

    def on_noteon(self, control, value, timedelta):
        pass

    def on_controlchange(self, control, value, timedelta):
        pass


    def noteoff(self, control, value):
        channel, note = self._out_config[control]
        self.send_message([NOTE_OFF | (channel & 0x0f), note, value])

    def noteon(self, control, value):
        channel, note = self._out_config[control]
        self.send_message([NOTE_ON | (channel & 0x0f), note, value])

    def controlchange(self, control, value):
        channel, note = self._out_config[control]
        self.send_message([CONTROL_CHANGE | (channel & 0x0f), note, value])


    def send_message(self, data):
        self._midiout.send_message(data)



class KVMidi(Midi):
    def __init__(self,
                 port=None,
                 name='midi',
                 convert_16_bit=True,
                 convert_float_1=False,
                 **kwargs):

        super(KVMidi, self).__init__(port, **kwargs)

        # override handlers
        self._handlers = {
            NOTE_OFF: self.kv_on_noteoff,
            NOTE_ON: self.kv_on_noteon,
            CONTROL_CHANGE: self.kv_on_controlchange,
        }

        self.kv = KVService(name=name, tags=['midi'])

        self.convert_16_bit = convert_16_bit
        self.convert_float_1 = convert_float_1

    def _on_data(self, control, value):
        if self.convert_16_bit:
            value = int(value * 65535.0 / 127.0)

        elif self.convert_float_1:
            value = value / 127.0

        self.kv[control] = value

    def kv_on_noteoff(self, control, value, timedelta):
        value = 0 # set noteoff values to 0
        self._on_data(control, value)

        self.on_noteoff(control, value, timedelta)

    def kv_on_noteon(self, control, value, timedelta):
        self._on_data(control, value)

        self.on_noteon(control, value, timedelta)

    def kv_on_controlchange(self, control, value, timedelta):
        self._on_data(control, value)

        self.on_controlchange(control, value, timedelta)



class MidiInputPrinter(Midi):
    def on_noteoff(self, note, value, timedelta):
        print "NOTE OFF", note, value, timedelta

    def on_noteon(self, note, value, timedelta):
        print "NOTE ON", note, value, timedelta

    def on_controlchange(self, note, value, timedelta):
        print "CONTROL CHANGE", note, value, timedelta


def list_output_ports():
    return rtmidi.MidiOut().get_ports()

def list_input_ports():
    return rtmidi.MidiIn().get_ports()

def port_connected(target_port):
    ports = list_input_ports()

    for port in ports:
        if port.find(target_port) >= 0:
            return True

    return False

def print_midi_ports():
    print "Inputs:"

    for port in list_input_ports():
        print port

    print ""

    print "Outputs:"

    for port in list_output_ports():
        print port

    print ""



def main():
    try:
        while True:
            time.sleep(0.1)

    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    #main()

    print_midi_ports()

    MidiInputPrinter(port='Akai APC40')


    main()



"""

apt install libasound2-dev
apt install libjack-dev

pip install python-rtmidi



import time
import rtmidi


def on_input(msg, data):
    print msg, data


midiout = rtmidi.MidiOut()
available_ports = midiout.get_ports()

print available_ports

if available_ports:
    midiout.open_port(1)
else:
    midiout.open_virtual_port("My virtual output")

# switch to ableton alternate mode
midiout.send_message([0xF0, 0x47, 0, 0x73, 0x60, 0x00, 0x04, 0x42, 0, 0, 0, 0xF7])

# clip launch LED modes
midiout.send_message([0x90, 0x35, 1]) # row 1, col 1 green
midiout.send_message([0x91, 0x35, 2]) # row 1, col 2 blinking green
midiout.send_message([0x92, 0x35, 3]) # row 1, col 3 red
midiout.send_message([0x93, 0x35, 4]) # row 1, col 4 blinking red
midiout.send_message([0x94, 0x35, 5]) # row 1, col 5 orange
midiout.send_message([0x95, 0x35, 6]) # row 1, col 6 blinking orange
midiout.send_message([0x96, 0x35, 7]) # row 1, col 7 green
midiout.send_message([0x97, 0x35, 8]) # row 1, col 8 green

midiout.send_message([0xB0, 0x38, 0]) # track knob 1, led off
midiout.send_message([0xB0, 0x39, 1]) # track knob 2, led single
midiout.send_message([0xB0, 0x3A, 2]) # track knob 3, led volume style
midiout.send_message([0xB0, 0x3B, 3]) # track knob 4, led pan style

midiout.send_message([0xB0, 0x30, 10]) # track knob 1
midiout.send_message([0xB0, 0x31, 80]) # track knob 2
midiout.send_message([0xB0, 0x32, 30]) # track knob 3
midiout.send_message([0xB0, 0x33, 40]) # track knob 4


#note_on = [0x99, 60, 112] # channel 10, middle C, velocity 112
#note_off = [0x89, 60, 0]

#midiout.send_message([])

#time.sleep(0.5)
#midiout.send_message(note_off)

del midiout




"""
