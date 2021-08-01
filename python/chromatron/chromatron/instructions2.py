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

from catbus import *
from sapphire.common import catbus_string_hash


class ReturnException(Exception):
    pass


opcodes = {
    'MOV':                  0x01,
    'LDC':                  0x02,
}


def string_hash_func(s):
    return catbus_string_hash(s)

def hash_to_bc(s):
    h = string_hash_func(s)
    
    return [(h >> 24) & 0xff, (h >> 16) & 0xff, (h >> 8) & 0xff, (h >> 0) & 0xff]

def convert_to_f16(value):
    return (value << 16) & 0xffffffff

def convert_to_i32(value):
    return int(value / 65536.0)

class BaseInstruction(object):
    mnemonic = 'NOP'
    opcode = None

    def __init__(self, lineno=None):
        self.lineno = lineno

    def __str__(self):
        return self.mnemonic

    def __repr__(self):
        return str(self)

    def assemble(self):
        raise NotImplementedError(self.mnemonic)

    def execute(self, vm):
        raise NotImplementedError(self.mnemonic)

    def len(self):
        return len(self.assemble())

    @property
    def opcode(self):
        global opcodes
        return opcodes[self.mnemonic]

# pseudo instruction - does not actually produce an opcode
class insReg(BaseInstruction):
    def __init__(self, reg=None, var=None, **kwargs):
        super().__init__(**kwargs)
        self.reg = reg
        self.var = var

    def __str__(self):
        if self.var is not None:
            return "%s(%s @ %s)" % (self.var.name, self.var.type, self.reg)

        else:
            return "Reg(%s)" % (self.reg)
    
    def assemble(self):
        # convert to 16 bits
        l = self.reg & 0xff
        h = (self.reg >> 8) & 0xff

        return [l, h]


class insMov(BaseInstruction):
    mnemonic = 'MOV'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.memory[self.dest.addr] = vm.memory[self.src.addr]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insLoadConst(BaseInstruction):
    mnemonic = 'LDC'

    def __init__(self, dest, value, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.value = value

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.value)

    def execute(self, vm):
        vm.memory[self.dest.addr] = self.value

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())

        return bc


class insLabel(BaseInstruction):
    def __init__(self, name=None, **kwargs):
        super().__init__(**kwargs)
        self.name = name

    def __str__(self):
        return "Label(%s)" % (self.name)

    def execute(self, vm):
        pass

    def assemble(self):
        # leave room for 16 bits
        return [self, None]