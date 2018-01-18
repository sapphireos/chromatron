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
import os
import ConfigParser

CONFIG_EXTENSION = '.cfg'

def load_config(path=os.getcwd()):

    midi_cfg_data = {'midi_in':{}, 'midi_out':{}}

    config = ConfigParser.RawConfigParser()

    for f in os.listdir(path):
        fname, ext = os.path.splitext(f)

        if ext != CONFIG_EXTENSION:
            continue

        config.read(f)

        for section in config.sections():
            try:
                cfg_type, device_name = section.split(':')

            except ValueError:
                continue

            cfg_type = cfg_type.strip()
            device_name = device_name.strip()

            # add device to data structure
            try:
                midi_cfg_data[cfg_type][device_name] = {}

            except KeyError:
                continue

            for item in config.items(section):
                control_name = item[0]
                
                tokens = item[1].split(',')
                channel = tokens[0]
                note = tokens[1]
                
                try:
                    control_type = tokens[2].strip()

                except IndexError:
                    control_type = 'note'

                try:
                    channel = int(channel)

                except ValueError:
                    channel = int(channel, 16)

                try:
                    note = int(note)

                except ValueError:
                    note = int(note, 16)

                if cfg_type == 'midi_in':
                    # for inputs, we reverse the mapping so we look up by
                    # channel and note to get the name
                    midi_cfg_data[cfg_type][device_name][(channel, note, control_type)] = control_name

                else:
                    midi_cfg_data[cfg_type][device_name][control_name] = (channel, note)




    return midi_cfg_data


if __name__ == "__main__":
    from pprint import pprint
    pprint(load_config())
