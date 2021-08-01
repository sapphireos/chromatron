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

from .ir import *

class VM(object):
    def __init__(self, builder=None, code=None, data=None, strings=None, pix_size_x=4, pix_size_y=4):
        self.pixel_arrays = {}

        if builder == None:
            self.code = code
            self.data = data
            self.data.extend(strings)

        else:
            self.code = builder.code
            self.data = builder.data_table
            self.data.extend(builder.strings)

            for k, v in list(builder.pixel_arrays.items()):
                self.pixel_arrays[k] = v.fields

        # set up pixel arrays
        self.pix_count = pix_size_x * pix_size_y

        self.hue        = [0 for i in range(self.pix_count)]
        self.sat        = [0 for i in range(self.pix_count)]
        self.val        = [0 for i in range(self.pix_count)]
        self.hs_fade    = [0 for i in range(self.pix_count)]
        self.v_fade     = [0 for i in range(self.pix_count)]

        self.gfx_data   = {'hue': self.hue,
                           'sat': self.sat,
                           'val': self.val,
                           'hs_fade': self.hs_fade,
                           'v_fade': self.v_fade}


        # init db
        self.db = {}
        self.db['pix_size_x'] = pix_size_x
        self.db['pix_size_y'] = pix_size_y
        self.db['pix_count'] = self.pix_count
        self.db['kv_test_array'] = [0] * 4
        self.db['kv_test_key'] = 0

        # set up master pixel array
        self.pixel_arrays['pixels']['count'] = self.pix_count
        self.pixel_arrays['pixels']['index'] = 0
        self.pixel_arrays['pixels']['size_x'] = pix_size_x
        self.pixel_arrays['pixels']['size_y'] = pix_size_y
        self.pixel_arrays['pixels']['reverse'] = 0


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

            if isinstance(var, irStrLiteral):
                self.memory.append(var.strlen)

                s = []
                for i in range(var.strlen):
                    s.append(var.strdata[i])

                    if len(s) == 4:
                        self.memory.append(s)
                        addr += 1
                        s = []

                if len(s) > 0:
                    self.memory.append(s)
                    addr += 1
                
            else:
                for i in range(var.length):
                    try:
                        self.memory.append(var.default_value[i])
                    except TypeError:
                        self.memory.append(var.default_value)

                addr += var.length - 1


    def calc_index(self, x, y, pixel_array='pixels'):
        count = self.pixel_arrays[pixel_array]['count']
        size_x = self.pixel_arrays[pixel_array]['size_x']
        size_y = self.pixel_arrays[pixel_array]['size_y']

        if y == 65535:
            i = x % count

        else:
            x %= size_x
            y %= size_y

            i = x + (y * size_x)

        return i

    def dump_hsv(self):
        return self.gfx_data

    def dump_registers(self):
        registers = {}
        for var in self.data:
            # don't dump consts
            if isinstance(var, irConst):
                continue

            if isinstance(var, irArray):
                value = []
                addr = var.addr
                for i in range(var.length):
                    value.append(self.memory[addr])
                    addr += 1

            elif isinstance(var, irRecord):
                value = {}

                for f in var.fields:
                    index = self.memory[var.offsets[f].addr]
                    value[f] = self.memory[index + var.addr]

            elif isinstance(var, irStrLiteral):
                value = var.strdata

            elif isinstance(var, irVar_str):
                # lookup reference
                ref = self.memory[var.addr]
                
                # get string length in characters
                strlen = self.memory[ref]
                ref += 1
                
                # convert string length to memory cells
                memlen = int(((strlen - 1) / 4) + 1)

                # unpack string
                s = []
                for i in range(memlen):
                    s.extend(self.memory[ref])
                    ref += 1

                value = ''.join(s)
                
            else:
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





