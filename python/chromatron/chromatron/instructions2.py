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


class insProgram(object):
    def __init__(self, funcs={}):
        self.funcs = funcs

    def __str__(self):
        s = 'FX Instructions:\n'

        for func in self.funcs.values():
            s += str(func)

        return s

    def run(self):
        pass

    def assemble(self):
        pass

class insFunc(object):
    def __init__(self, name, code=[], source_code=[], lineno=None):
        self.name = name
        self.code = code
        self.source_code = source_code
        self.lineno = lineno

    def __str__(self):
        s = ''
        s += "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
        s += f"Func: {self.name}\n"
        s += "Code:\n"
        s += "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n"
        lines_printed = []
        for ins in self.code:
            if ins.lineno >= 0 and ins.lineno not in lines_printed and not isinstance(ins, insLabel):
                s += f'________________________________________________________\n'
                s += f' {ins.lineno}: {self.source_code[ins.lineno - 1].strip()}\n'
                lines_printed.append(ins.lineno)

            s += f'\t{ins}\n'

        s += f'VM Instructions: {len([i for i in self.code if not isinstance(i, insLabel)])}\n'

        return s

    def run(self):
        pass

    def assemble(self):
        pass


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


class BaseJmp(BaseInstruction):
    mnemonic = 'JMP'

    def __init__(self, label, **kwargs):
        super().__init__(**kwargs)

        self.label = label

    def __str__(self):
        return "%s -> %s" % (self.mnemonic, self.label)


class insJmp(BaseJmp):
    def execute(self, vm):
        return self.label

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.label.assemble())

        return bc

class insJmpConditional(BaseJmp):
    def __init__(self, op1, label, **kwargs):
        super().__init__(label, **kwargs)

        self.op1 = op1

    def __str__(self):
        return "%s, %s -> %s" % (self.mnemonic, self.op1, self.label)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.label.assemble())
        bc.extend(self.op1.assemble())

        return bc

class insJmpIfZero(insJmpConditional):
    mnemonic = 'JMPZ'

    def execute(self, vm):
        if vm.memory[self.op1.addr] == 0:
            return self.label

class insReturn(BaseInstruction):
    mnemonic = 'RET'

    def __init__(self, op1, **kwargs):
        super().__init__(**kwargs)
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def execute(self, vm):
        vm.memory[0] = vm.memory[self.op1.addr]

        raise ReturnException

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.op1.assemble())
        
        return bc



class insBinop(BaseInstruction):
    mnemonic = 'BINOP'
    symbol = "??"

    def __init__(self, result, op1, op2, **kwargs):
        super().__init__(**kwargs)
        self.result = result
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%-16s %16s <- %16s %4s %16s" % (self.mnemonic, self.result, self.op1, self.symbol, self.op2)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.result.assemble())
        bc.extend(self.op1.assemble())
        bc.extend(self.op2.assemble())

        return bc

class insCompareEq(insBinop):
    mnemonic = 'COMP_EQ'
    symbol = "=="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] == vm.memory[self.op2.addr]

class insCompareNeq(insBinop):
    mnemonic = 'COMP_NEQ'
    symbol = "!="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] != vm.memory[self.op2.addr]

class insCompareGt(insBinop):
    mnemonic = 'COMP_GT'
    symbol = ">"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] > vm.memory[self.op2.addr]

class insCompareGtE(insBinop):
    mnemonic = 'COMP_GTE'
    symbol = ">="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] >= vm.memory[self.op2.addr]

class insCompareLt(insBinop):
    mnemonic = 'COMP_LT'
    symbol = "<"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] < vm.memory[self.op2.addr]

class insCompareLtE(insBinop):
    mnemonic = 'COMP_LTE'
    symbol = "<="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] <= vm.memory[self.op2.addr]

class insAnd(insBinop):
    mnemonic = 'AND'
    symbol = "AND"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] and vm.memory[self.op2.addr]

class insOr(insBinop):
    mnemonic = 'OR'
    symbol = "OR"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] or vm.memory[self.op2.addr]

class insAdd(insBinop):
    mnemonic = 'ADD'
    symbol = "+"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] + vm.memory[self.op2.addr]

class insSub(insBinop):
    mnemonic = 'SUB'
    symbol = "-"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] - vm.memory[self.op2.addr]

class insMul(insBinop):
    mnemonic = 'MUL'
    symbol = "*"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] * vm.memory[self.op2.addr]

class insDiv(insBinop):
    mnemonic = 'DIV'
    symbol = "/"

    def execute(self, vm):
        if vm.memory[self.op2.addr] == 0:
            vm.memory[self.result.addr] = 0

        else:
            vm.memory[self.result.addr] = vm.memory[self.op1.addr] / vm.memory[self.op2.addr]

class insMod(insBinop):
    mnemonic = 'MOD'
    symbol = "%"

    def execute(self, vm):
        if vm.memory[self.op2.addr] == 0:
            vm.memory[self.result.addr] = 0

        else:
            vm.memory[self.result.addr] = vm.memory[self.op1.addr] % vm.memory[self.op2.addr]
