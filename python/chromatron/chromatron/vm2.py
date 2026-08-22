# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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

from .ir2 import *


class VM(object):
    def __init__(self, pix_size_x=4, pix_size_y=4):
        
        

        # init memory
        self.memory = []

        # sort data by addresses
        data = [a for a in sorted(self.data, key=lambda data: data.addr)]

        # for a in data:
            # print a.addr, a.length

        addr = -1
        for var in data:
            if var.addr <= addr:
                continue

            addr = var.addr

            for i in range(var.length):
                try:
                    self.memory.append(var.default_value[i])
                except TypeError:
                    self.memory.append(var.default_value)

            addr += var.length - 1

    def dump_registers(self):
        registers = {}
        for var in self.data:    
            value = self.memory[var.addr]

            # convert fixed16 to float
            if var.type == 'f16':
                value = (value >> 16) + (value & 0xffff) / 65536.0

            registers[var.name] = value

        return registers

    def run_once(self):
        self.run('init')
        self.run('loop')

    def run(self, func):
        cycles = 0
        pc = 0

        return_stack = []

        offsets = {}

        # linearize code stream
        code = []
        for v in list(self.code.values()):
            code.extend(v)

        # scan code stream and get offsets for all functions and labels
        for i in range(len(code)):
            ins = code[i]
            if isinstance(ins, insFunction) or isinstance(ins, insLabel):
                offsets[ins.name] = i


        # setup PC
        try:
            pc = offsets[func]
        
        except KeyError:
            raise VMRuntimeError("Function '%s' not found" % (func))

        while True:
            cycles += 1

            ins = code[pc]

            # print(cycles, pc, ins)

            pc += 1

            try:    
                ret_val = ins.execute(self)

                if isinstance(ins, insCall):
                    # push PC to return stack
                    return_stack.append(pc)

                # if returning label to jump to
                if ret_val != None:
                    # jump to target
                    pc = offsets[ret_val.name]
                
            except ReturnException:
                if len(return_stack) == 0:
                    # program is complete
                    break

                # pop PC from return stack
                pc = return_stack.pop(-1)


        # clean up





