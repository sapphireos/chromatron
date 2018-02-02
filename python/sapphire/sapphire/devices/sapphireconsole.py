#! python
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

import threading

from sapphire.devices.device import Device, DeviceUnreachableException

import sys
import traceback # ignore unused warning!

import cmd2 as cmd

from pyparsing import *


# set up query parse grammar
query_item   = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!"#$%&\()*+,-./:;<>?@[\\]^_`{|}~'
equals       = Suppress('=')
query_param  = Word(query_item).setResultsName('param')
query_value  = Word(query_item).setResultsName('value')
query_expr   = Group(query_param + equals + query_value)
query_list   = ZeroOrMore(query_expr)

def make_query_dict(line):
    q = query_list.parseString(line)

    d = dict()

    for i in q:
        d[i.param] = i.value

    return d

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
        print (str(errmsg))

    def do_scan(self, line):
        pass

    def query(self, line):
        if line == 'all':
            devices = self.devices

        else:
            qdict = make_query_dict(line)
            devices = [o for o in self.devices if o.query(**qdict)]

        return devices

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
            print "Found %d devices" % (len(devices))

            for device in devices:
                print device.who()

    def do_select(self, line):
        # query for targets
        devices = self.query(line)

        self.targets = devices

        self.init_shell(line)

        print "Selected %d devices" % (len(self.targets))

        return True

    def do_add(self, line):
        # query for targets
        devices = self.query(line)

        for device in devices:
            self.targets.append(device)

        self.init_shell(line)

        print "Selected %d devices" % (len(self.targets))

        return True

    def do_remove(self, line):
        # query for targets
        devices = self.query(line)

        for device in devices:
            self.targets.remove(device)

        self.init_shell(line)

        print "Selected %d devices" % (len(self.targets))

        return True

    def do_who(self, line):
        for target in self.targets:
            print target.who()


cli_template = """
    def do_$fname(self, line):
        for target in self.targets:
            sys.stdout.write('%s@%s: ' % (target.name.ljust(16), target.host))

            try:
                print target.cli_$fname(line)

            except DeviceUnreachableException as e:
                print 'Error:%s from %s' % (e, target.host)

            except Exception as e:
                print 'Error: %s' % (e)
                #traceback.print_exc()

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

    for fname in cli_funcs:
        s += cli_template.replace('$fname', fname)

    exec(s)

    return aConsole(targets=targets, devices=devices)

def main():
    try:
        host = sys.argv[1]
        sys.argv[1] = '' # prevents an unknown syntax error in the command loop

        try:
            print "Connecting to %s" % host

            d = Device(host=host)
            d.scan()

            c = makeConsole(targets=[d])
            c.cmdloop()

        except:
            print "*** Unable to connect to %s" % host
            raise

    except IndexError:
        print "No device specified"


if __name__ == '__main__':
    main()