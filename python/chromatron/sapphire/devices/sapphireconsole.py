#! python
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

import threading

from sapphire.devices.device import Device, DeviceUnreachableException
from sapphire.devices import channel 
from chromatron import DeviceGroup

import sys
import traceback # ignore unused warning!

# cmd2 now directly included in source tree.
# modern versions of pip refuse to install cmd2 0.6.9 and newer versions of cmd2
# remove features (like auto command abbreviation)
from . import cmd2 as cmd


class SapphireConsole(cmd.Cmd):

    prompt = '(Nothing): '

    ruler = '-'

    def __init__(self, targets=[], devices=[]):
        self.targets = targets
        self.devices = devices

        cmd.Cmd.__init__(self)

    def cmdloop(self):
        SapphireConsole.next_cmd = None

        cmd.Cmd.cmdloop(self)

        return SapphireConsole.next_cmd

    def perror(self, errmsg, statement=None):
        traceback.print_exc()
        print((str(errmsg)))

    def do_scan(self, line):
        pass

    def init_shell(self, query):
        # set up next command shell
        if len(self.targets) > 0:
            target_type = type(self.targets[0])

            # check if all targets are the same type
            for target in self.targets:
                if type(target) != target_type:
                    target_type = Device
                    break

            SapphireConsole.next_cmd = makeConsole(targets=self.targets,
                                                   devices=self.devices,
                                                   device=target_type())

            #SapphireConsole.next_cmd.prompt = "(%s): " % (query)
            SapphireConsole.next_cmd.prompt = "(%3d devices): " % (len(self.targets))

        else:
            # no targets, set up empty shell
            SapphireConsole.next_cmd = SapphireConsole()

            SapphireConsole.next_cmd.prompt = "(Nothing): "

    def do_query(self, line):
        devices = self.query(line)

        if devices:
            print("Found %d devices" % (len(devices)))

            for device in devices:
                print(device.who())

    def do_select(self, line):
        # query for targets
        devices = self.query(line)

        self.targets = devices

        self.init_shell(line)

        print("Selected %d devices" % (len(self.targets)))

        return True

    def do_add(self, line):
        # query for targets
        devices = self.query(line)

        for device in devices:
            self.targets.append(device)

        self.init_shell(line)

        print("Selected %d devices" % (len(self.targets)))

        return True

    def do_remove(self, line):
        # query for targets
        devices = self.query(line)

        for device in devices:
            self.targets.remove(device)

        self.init_shell(line)

        print("Selected %d devices" % (len(self.targets)))

        return True

    def do_who(self, line):
        for target in self.targets:
            print(target.who())

    # needed if we ever update cmd2
    # def sigint_handler(self, signum, frame):
    #     raise SystemExit

    # def do_exit(self, line):
    #     raise SystemExit


cli_template = """
    def do_$fname(self, line):
        for target in self.targets:
            sys.stdout.write('%s@%12s: ' % (target.name.ljust(16), target.host))

            try:
                print(target.cli_$fname(line))

            except DeviceUnreachableException as e:
                print('Error:%s from %s' % (e, target.host))

            except Exception as e:
                print('Error: %s' % (e))
                traceback.print_exc()

"""

cli_template_group = """
    def do_$fname(self, line):
        try:
            print(self.targets[0].cli_$fname(line, targets=self.targets))

        except DeviceUnreachableException as e:
            print('Error:%s from %s' % (e, target.host))

        except Exception as e:
            print('Error: %s' % (e))
            traceback.print_exc()

"""

def makeConsole(targets=[], devices=[], device=None):
    if not device:
        try:
            device = targets[0]
        except:
            pass

    # get command line function listing from device
    cli_funcs = device.get_cli()

    s = "class aConsole(SapphireConsole):\n"

    group_commands = ['nettime']

    for fname in cli_funcs:
        if fname in group_commands:

            s += cli_template_group.replace('$fname', fname)

        else:
            s += cli_template.replace('$fname', fname)
    
    exec(s)

    return locals()['aConsole'](targets=targets, devices=devices)

def main():
    try:
        host = sys.argv[1]
        sys.argv[1] = '' # prevents an unknown syntax error in the command loop

        targets = []
        try:
            d = Device(host=host)
            print("Connecting to %s" % host)
            d.scan()

            targets = [d]

        except channel.InvalidChannel:
            # try a query
            query = sys.argv[2:]
            query.append(host)

            devices = DeviceGroup(*query)

            targets = [d._device for d in devices.values()]

            if len(targets) == 0:
                print("No devices found")
                sys.exit(0)

            print(f"Found {len(targets)} devices")

        except:
            print("*** Unable to connect to %s" % host)
            raise

        c = makeConsole(targets=targets)
        c.cmdloop()

    except IndexError:
        print("No device specified")


if __name__ == '__main__':
    main()