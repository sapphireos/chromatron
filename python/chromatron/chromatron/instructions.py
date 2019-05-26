# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2019  Jeremy Billheimer
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

import random



class ReturnException(Exception):
    pass


PIX_ATTR_CODES = {
    'hue':      0,
    'sat':      1,
    'val':      2,
    'hs_fade':  3,
    'v_fade':   4,
    'pval':     5,
}

THREAD_FUNCS = ['start_thread', 'stop_thread', 'thread_running']


opcodes = {
    'MOV':                  0x01,
    'CLR':                  0x02,

    'NOT':                  0x03,

    'COMP_EQ':              0x04,
    'COMP_NEQ':             0x05,
    'COMP_GT':              0x06,
    'COMP_GTE':             0x07,
    'COMP_LT':              0x08,
    'COMP_LTE':             0x09,
    'AND':                  0x0A,
    'OR':                   0x0B,
    'ADD':                  0x0C,
    'SUB':                  0x0D,
    'MUL':                  0x0E,
    'DIV':                  0x0F,
    'MOD':                  0x10,
    
    'F16_COMP_EQ':          0x11,
    'F16_COMP_NEQ':         0x12,
    'F16_COMP_GT':          0x13,
    'F16_COMP_GTE':         0x14,
    'F16_COMP_LT':          0x15,
    'F16_COMP_LTE':         0x16,
    'F16_AND':              0x17,
    'F16_OR':               0x18,
    'F16_ADD':              0x19,
    'F16_SUB':              0x1A,
    'F16_MUL':              0x1B,
    'F16_DIV':              0x1C,
    'F16_MOD':              0x1D,

    'JMP':                  0x1E,

    'JMP_IF_Z':             0x1F,
    'JMP_IF_NOT_Z':         0x20,
    
    'JMP_IF_LESS_PRE_INC':  0x21,

    'RET':                  0x22,
    'CALL':                 0x23,
    'LCALL':                0x24,
    'DBCALL':               0x25,

    'INDEX':                0x26,
    'LOAD_INDIRECT':        0x27,
    'STORE_INDIRECT':       0x28,

    'ASSERT':               0x29,
    'HALT':                 0x2A,

    'VMOV':                 0x2B,
    'VADD':                 0x2C,
    'VSUB':                 0x2D,
    'VMUL':                 0x2E,
    'VDIV':                 0x2F,
    'VMOD':                 0x30,
    
    'PMOV':                 0x31,
    'PADD':                 0x32,
    'PSUB':                 0x33,
    'PMUL':                 0x34,
    'PDIV':                 0x35,
    'PMOD':                 0x36,
    
    'PSTORE_HUE':           0x37,
    'PSTORE_SAT':           0x38,
    'PSTORE_VAL':           0x39,
    'PSTORE_HSFADE':        0x3A,
    'PSTORE_VFADE':         0x3B,

    'PLOAD_HUE':            0x3C,
    'PLOAD_SAT':            0x3D,
    'PLOAD_VAL':            0x3E,
    'PLOAD_HSFADE':         0x3F,
    'PLOAD_VFADE':          0x40,

    'DB_STORE':             0x41,    
    'DB_LOAD':              0x42,    
    
    'CONV_I32_TO_F16':      0x43,
    'CONV_F16_TO_I32':      0x44,

    'IS_V_FADING':          0x45,
    'IS_HS_FADING':         0x46,

    'PSTORE_PVAL':          0x47,
    'PLOAD_PVAL':           0x48,
}


def string_hash_func(s):
    return catbus_string_hash(s)

def hash_to_bc(s):
    h = string_hash_func(s)
    
    return [(h >> 24) & 0xff, (h >> 16) & 0xff, (h >> 8) & 0xff, (h >> 0) & 0xff]


class BaseInstruction(object):
    mnemonic = 'NOP'
    opcode = None

    def __str__(self):
        return self.mnemonic

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

class insNop(BaseInstruction):
    def execute(self, vm):
        pass

    def assemble(self):
        return []

# pseudo instruction - does not actually produce an opcode
class insAddr(BaseInstruction):
    def __init__(self, addr=None, var=None):
        self.addr = addr
        self.var = var

    def __str__(self):
        if self.var != None:
            return "Addr(%s, %s)" % (self.addr, self.var.type)

        else:
            return "Addr(%s)" % (self.addr)
    
    def assemble(self):
        # convert to 16 bits
        l = self.addr & 0xff
        h = (self.addr >> 8) & 0xff

        return [l, h]


class insLabel(BaseInstruction):
    def __init__(self, name=None):
        self.name = name

    def __str__(self):
        return "Label(%s)" % (self.name)

    def execute(self, vm):
        pass

    def assemble(self):
        # leave room for 16 bits
        return [self, None]

class insFuncTarget(BaseInstruction):
    def __init__(self, name=None):
        self.name = name

    def __str__(self):
        return "Target(%s)" % (self.name)

    def execute(self, vm):
        pass

    def assemble(self):
        # leave room for 16 bits
        return [self, None]

class insPixelArray(BaseInstruction):
    def __init__(self, name=None):
        self.name = name

    def __str__(self):
        return "PixelArray(%s)" % (self.name)

    def execute(self, vm):
        pass

    def assemble(self):
        return [self]

class insFunction(BaseInstruction):
    def __init__(self, name=None, args=[]):
        self.name = name
        self.args = args

    def __str__(self):
        args = ''
        for a in self.args:
            args += '%s, ' % (a)

        args = args[:len(args) - 2]

        return "Func %s (%s)" % (self.name, args)

    def execute(self, vm):
        pass

    def assemble(self):
        return []


class insMov(BaseInstruction):
    mnemonic = 'MOV'

    def __init__(self, dest, src):
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

class insClr(BaseInstruction):
    mnemonic = 'CLR'

    def __init__(self, dest):
        self.dest = dest

    def __str__(self):
        return "%s %s <- 0" % (self.mnemonic, self.dest)

    def execute(self, vm):
        vm.memory[self.dest.addr] = 0

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())

        return bc

class insNot(BaseInstruction):
    mnemonic = 'NOT'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = NOT %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        if vm.memory[self.src.addr] == 0:
            vm.memory[self.dest.addr] = 1

        else:            
            vm.memory[self.dest.addr] = 0

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insBinop(BaseInstruction):
    mnemonic = 'BINOP'
    symbol = "??"

    def __init__(self, result, op1, op2):
        super(insBinop, self).__init__()
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


class insF16CompareEq(insBinop):
    mnemonic = 'F16_COMP_EQ'
    symbol = "=="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] == vm.memory[self.op2.addr]

class insF16CompareNeq(insBinop):
    mnemonic = 'F16_COMP_NEQ'
    symbol = "!="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] != vm.memory[self.op2.addr]

class insF16CompareGt(insBinop):
    mnemonic = 'F16_COMP_GT'
    symbol = ">"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] > vm.memory[self.op2.addr]

class insF16CompareGtE(insBinop):
    mnemonic = 'F16_COMP_GTE'
    symbol = ">="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] >= vm.memory[self.op2.addr]

class insF16CompareLt(insBinop):
    mnemonic = 'F16_COMP_LT'
    symbol = "<"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] < vm.memory[self.op2.addr]

class insF16CompareLtE(insBinop):
    mnemonic = 'F16_COMP_LTE'
    symbol = "<="

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] <= vm.memory[self.op2.addr]

class insF16And(insBinop):
    mnemonic = 'F16_AND'
    symbol = "AND"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] and vm.memory[self.op2.addr]

class insF16Or(insBinop):
    mnemonic = 'F16_OR'
    symbol = "OR"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] or vm.memory[self.op2.addr]

class insF16Add(insBinop):
    mnemonic = 'F16_ADD'
    symbol = "+"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] + vm.memory[self.op2.addr]

class insF16Sub(insBinop):
    mnemonic = 'F16_SUB'
    symbol = "-"

    def execute(self, vm):
        vm.memory[self.result.addr] = vm.memory[self.op1.addr] - vm.memory[self.op2.addr]

class insF16Mul(insBinop):
    mnemonic = 'F16_MUL'
    symbol = "*"

    def execute(self, vm):
        # NOTE!
        # need to cast to i64 in C version to prevent overflow!

        vm.memory[self.result.addr] = (vm.memory[self.op1.addr] * vm.memory[self.op2.addr]) / 65536

class insF16Div(insBinop):
    mnemonic = 'F16_DIV'
    symbol = "/"

    def execute(self, vm):
        if vm.memory[self.op2.addr] == 0.0:
            vm.memory[self.result.addr] = 0

        else:
            # NOTE!
            # need to cast left hand side to i64 for multiply by 65536.
            # doing it in this order prevents loss of precision on the fractional side.
            vm.memory[self.result.addr] = (vm.memory[self.op1.addr] * 65536) / vm.memory[self.op2.addr]

class insF16Mod(insBinop):
    mnemonic = 'F16_MOD'
    symbol = "%"

    def execute(self, vm):
        if vm.memory[self.op2.addr] == 0:
            vm.memory[self.result.addr] = 0

        else:
            vm.memory[self.result.addr] = vm.memory[self.op1.addr] % vm.memory[self.op2.addr]


class BaseJmp(BaseInstruction):
    mnemonic = 'JMP'

    def __init__(self, label):
        super(BaseJmp, self).__init__()

        self.label = label

    def __str__(self):
        return "%s -> %s" % (self.mnemonic, self.label)

    # def assemble(self):
        # return [self.opcode, ('label', self.label.name), 0]

class insJmp(BaseJmp):
    def execute(self, vm):
        return self.label

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.label.assemble())

        return bc

class insJmpConditional(BaseJmp):
    def __init__(self, op1, label):
        super(insJmpConditional, self).__init__(label)

        self.op1 = op1

    def __str__(self):
        return "%s, %s -> %s" % (self.mnemonic, self.op1, self.label)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.label.assemble())
        bc.extend(self.op1.assemble())

        return bc

class insJmpIfZero(insJmpConditional):
    mnemonic = 'JMP_IF_Z'

    def execute(self, vm):
        if vm.memory[self.op1.addr] == 0:
            return self.label

class insJmpNotZero(insJmpConditional):
    mnemonic = 'JMP_IF_NOT_Z'

    def execute(self, vm):
        if vm.memory[self.op1.addr] != 0:
            return self.label

# class insJmpIfZeroPostDec(insJmpConditional):
    # mnemonic = 'JMP_IF_Z_DEC'

# class insJmpIfGte(BaseJmp):
#     mnemonic = 'JMP_IF_GTE'

#     def __init__(self, op1, op2, label):
#         super(insJmpIfGte, self).__init__(label)

#         self.op1 = op1
#         self.op2 = op2

#     def __str__(self):
#         return "%s, %s >= %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)


class insJmpIfLessThanPreInc(BaseJmp):
    mnemonic = 'JMP_IF_LESS_PRE_INC'

    def __init__(self, op1, op2, label):
        super(insJmpIfLessThanPreInc, self).__init__(label)

        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%s, ++%s < %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)

    def execute(self, vm):
        # increment op1
        vm.memory[self.op1.addr] += 1

        # compare to op2
        if vm.memory[self.op1.addr] < vm.memory[self.op2.addr]:
            # return jump target
            return self.label

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.label.assemble())
        bc.extend(self.op1.assemble())
        bc.extend(self.op2.assemble())

        return bc

class insReturn(BaseInstruction):
    mnemonic = 'RET'

    def __init__(self, op1):
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

class insCall(BaseInstruction):
    mnemonic = 'CALL'

    def __init__(self, target, params=[], args=[]):
        self.target = target
        self.params = params
        self.args = args

        assert len(self.params) == len(self.args)

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        args = ''
        for arg in self.args:
            args += '%s, ' % (arg)
        args = args[:len(args) - 2]

        return "%s %s (%s):(%s)" % (self.mnemonic, self.target, params, args)

    def execute(self, vm):
        # load arguments with parameters
        for i in xrange(len(self.params)):
            param = self.params[i]
            arg = self.args[i]

            vm.memory[arg.addr] = vm.memory[param.addr]

        return insLabel(self.target)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(insFuncTarget(self.target).assemble())

        assert len(self.params) == len(self.args)

        bc.append(len(self.params))
        for i in xrange(len(self.params)):
            bc.extend(self.params[i].assemble())
            bc.extend(self.args[i].assemble())

        return bc


class insLibCall(BaseInstruction):
    mnemonic = 'LCALL'

    def __init__(self, target, result, params=[]):
        self.target = target
        self.result = result
        self.params = params

        self.lib_funcs = {
            'len': self._len,
            'min': self._min,
            'max': self._max,
            'sum': self._sum,
            'avg': self._avg,
            'rand': self._rand,
            'test_lib_call': self._test_lib_call,
        }

        self.thread_funcs = THREAD_FUNCS

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s = %s (%s)" % (self.mnemonic, self.result, self.target, params)

    def execute(self, vm):
        result = self.lib_funcs[self.target](vm.memory)

        vm.memory[self.result.addr] = result

    def assemble(self):
        bc = [self.opcode]
        bc.extend(hash_to_bc(self.target))

        bc.append(len(self.params))
        for param in self.params:
            try:
                bc.extend(param.assemble())

            except AttributeError:
                # maybe got a string or something that can't assemble()

                # check if this is a thread call
                if self.target in self.thread_funcs:
                    param = insFuncTarget(param)
                    
                    bc.extend(param.assemble())

        bc.extend(self.result.assemble())

        return bc

    def _test_lib_call(self, memory):
        return memory[self.params[0].addr] + memory[self.params[1].addr]

    def _rand(self, memory):
        start = 0
        end = 65535

        if len(self.params) == 1:
            end = memory[self.params[0].addr]

        if len(self.params) == 2:
            start = memory[self.params[0].addr]
            end = memory[self.params[1].addr]

        return random.randint(start, end)

    def _len(self, memory):
        return self.params[0].var.count

    def _min(self, memory):
        addr = self.params[0].addr
        a = memory[addr]

        for i in xrange(self.params[0].var.length - 1):
            addr += 1
            if memory[addr] < a:
                a = memory[addr]

        return a

    def _max(self, memory):
        addr = self.params[0].addr
        a = memory[addr]

        for i in xrange(self.params[0].var.length - 1):
            addr += 1
            if memory[addr] > a:
                a = memory[addr]

        return a

    def _sum(self, memory):
        addr = self.params[0].addr
        a = 0

        for i in xrange(self.params[0].var.length):
        
            a += memory[addr]
            addr += 1

        return a

    def _avg(self, memory):
        addr = self.params[0].addr
        a = 0

        for i in xrange(self.params[0].var.length):
        
            a += memory[addr]
            addr += 1

        return a / self.params[0].var.length

class insDBCall(BaseInstruction):
    mnemonic = 'DBCALL'

    def __init__(self, target, db_item, result, params=[]):
        self.target = target
        self.db_item = db_item
        self.result = result
        self.params = params

        self.lib_funcs = {
            'len': self._len,
            # 'min': self._min,
            # 'max': self._max,
            # 'sum': self._sum,
            # 'avg': self._avg,
        }

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s = %s (%s, %s)" % (self.mnemonic, self.result, self.target, self.db_item, params)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(hash_to_bc(self.target))
        bc.extend(hash_to_bc(self.db_item))

        bc.append(len(self.params))
        for param in self.params:
            bc.extend(param.assemble())

        bc.extend(self.result.assemble())
        
        return bc

    def _len(self, vm):
        item = vm.db[self.db_item]
        
        try:
            return len(item)

        except TypeError:
            return 1

    # def _min(self, memory):
    #     addr = self.params[0].addr
    #     a = memory[addr]

    #     for i in xrange(self.params[0].var.length - 1):
    #         addr += 1
    #         if memory[addr] < a:
    #             a = memory[addr]

    #     return a

    # def _max(self, memory):
    #     addr = self.params[0].addr
    #     a = memory[addr]

    #     for i in xrange(self.params[0].var.length - 1):
    #         addr += 1
    #         if memory[addr] > a:
    #             a = memory[addr]

    #     return a

    # def _sum(self, memory):
    #     addr = self.params[0].addr
    #     a = 0

    #     for i in xrange(self.params[0].var.length):
        
    #         a += memory[addr]
    #         addr += 1

    #     return a

    # def _avg(self, memory):
    #     addr = self.params[0].addr
    #     a = 0

    #     for i in xrange(self.params[0].var.length):
        
    #         a += memory[addr]
    #         addr += 1

    #     return a / self.params[0].var.length

    def execute(self, vm):
        result = self.lib_funcs[self.target](vm)

        vm.memory[self.result.addr] = result



class insIndex(BaseInstruction):
    mnemonic = 'INDEX'

    def __init__(self, result, base_addr, indexes, counts, strides):
        self.result = result
        self.base_addr = base_addr
        self.indexes = indexes
        self.counts = counts
        self.strides = strides

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)
        return "%s %s <- %s %s" % (self.mnemonic, self.result, self.base_addr, indexes)

    def execute(self, vm):
        addr = self.base_addr.addr

        for i in xrange(len(self.indexes)):
            index = vm.memory[self.indexes[i].addr]
            
            count = self.counts[i]
            stride = self.strides[i]            

            if count > 0:
                index %= count
                index *= stride

            addr += index

        vm.memory[self.result.addr] = addr

    def assemble(self):
        assert len(self.indexes) == len(self.counts)
        assert len(self.indexes) == len(self.strides)

        bc = [self.opcode]
        bc.extend(self.result.assemble())
        bc.extend(self.base_addr.assemble())
        bc.append(len(self.indexes))

        for i in xrange(len(self.indexes)):
            bc.extend(self.indexes[i].assemble())
            bc.extend([self.counts[i] % 0xff, (self.counts[i] >> 8) % 0xff])
            bc.extend([self.strides[i] % 0xff, (self.strides[i] >> 8) % 0xff])
            
        return bc


class insIndirectLoad(BaseInstruction):
    mnemonic = 'LOAD_INDIRECT'

    def __init__(self, dest, addr):
        self.dest = dest
        self.addr = addr

    def __str__(self):
        return "%s %s <- *%s" % (self.mnemonic, self.dest, self.addr)
    
    def execute(self, vm):
        addr = vm.memory[self.addr.addr]
        vm.memory[self.dest.addr] = vm.memory[addr]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.addr.assemble())

        return bc


class insIndirectStore(BaseInstruction):
    mnemonic = 'STORE_INDIRECT'

    def __init__(self, src, addr):
        self.src = src
        self.addr = addr

    def __str__(self):
        return "%s *%s <- %s" % (self.mnemonic, self.addr, self.src)

    def execute(self, vm):
        addr = vm.memory[self.addr.addr]
        vm.memory[addr] = vm.memory[self.src.addr]
    
    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.addr.assemble())
        bc.extend(self.src.assemble())

        return bc


# class insRand(BaseInstruction):
#     mnemonic = 'RAND'

#     def __init__(self, dest, start=0, end=65535):
#         self.dest = dest
#         self.start = start
#         self.end = end

#     def __str__(self):
#         return "%s %s <- rand(%s, %s)" % (self.mnemonic, self.dest, self.start, self.end)

#     def assemble(self):
#         bc = [self.opcode]
#         bc.extend(self.dest.assemble())
#         bc.extend(self.start.assemble())
#         bc.extend(self.end.assemble())

#         return bc

class insAssert(BaseInstruction):
    mnemonic = 'ASSERT'

    def __init__(self, op1):
        self.op1 = op1

    def __str__(self):
        return "%s %s == TRUE" % (self.mnemonic, self.op1)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.op1.assemble())
        
        return bc

class insHalt(BaseInstruction):
    mnemonic = 'HALT'
    
    def __init__(self):
        pass

    def __str__(self):
        return "%s" % (self.mnemonic)

    def assemble(self):
        bc = [self.opcode]
        
        return bc

class insVector(BaseInstruction):
    mnemonic = 'VECTOR'

    def __init__(self, target, value):
        super(insVector, self).__init__()
        self.target = target
        self.value = value

        self.type = self.target.var.get_base_type()

        self.length = self.target.var.length

    def __str__(self):
        return "%s *%s %s= %s" % (self.mnemonic, self.target, self.symbol, self.value)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.target.assemble())
        bc.extend(self.value.assemble())

        # convert to 16 bits
        l = self.length & 0xff
        h = (self.length >> 8) & 0xff

        bc.extend([l, h])

        target_type = get_type_id(self.type)
        bc.append(target_type)

        return bc


class insVectorMov(insVector):
    mnemonic = 'VMOV'
    op = "mov"
    symbol = "="

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        addr = vm.memory[self.target.addr]

        for i in xrange(self.length):
            vm.memory[addr] = value
            addr += 1

class insVectorAdd(insVector):
    mnemonic = 'VADD'
    op = "add"
    symbol = "+"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        addr = vm.memory[self.target.addr]

        for i in xrange(self.length):
            vm.memory[addr] += value
            addr += 1

class insVectorSub(insVector):
    mnemonic = 'VSUB'
    op = "sub"
    symbol = "-"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        addr = vm.memory[self.target.addr]

        for i in xrange(self.length):
            vm.memory[addr] -= value
            addr += 1

class insVectorMul(insVector):
    mnemonic = 'VMUL'
    op = "mul"
    symbol = "*"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        addr = vm.memory[self.target.addr]

        if self.type == 'f16':
            for i in xrange(self.length):
                vm.memory[addr] = (vm.memory[addr] * value) / 65536
                    
                addr += 1

        else:
            for i in xrange(self.length):
                vm.memory[addr] *= value
                addr += 1

class insVectorDiv(insVector):
    mnemonic = 'VDIV'
    op = "div"
    symbol = "/"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        addr = vm.memory[self.target.addr]

        # check for divide by zero
        if value == 0:
            for i in xrange(self.length):
                vm.memory[addr] = value
                addr += 1

        elif self.type == 'f16':
            for i in xrange(self.length):
                vm.memory[addr] = (vm.memory[addr] * 65536) / value
                    
                addr += 1

        else:
            for i in xrange(self.length):
                vm.memory[addr] /= value
                addr += 1

class insVectorMod(insVector):
    mnemonic = 'VMOD'
    op = "mod"
    symbol = "%"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        addr = vm.memory[self.target.addr]

        # check for divide by zero
        if value == 0:
            for i in xrange(self.length):
                vm.memory[addr] = value
                addr += 1

        elif self.type == 'f16':
            for i in xrange(self.length):
                vm.memory[addr] = vm.memory[addr] % value
                    
                addr += 1

        else:
            for i in xrange(self.length):
                vm.memory[addr] %= value
                addr += 1



class insPixelVector(BaseInstruction):
    mnemonic = 'PIXEL_VECTOR'

    def __init__(self, pixel_array, attr, value):
        super(insPixelVector, self).__init__()
        self.pixel_array = pixel_array
        self.attr = attr
        self.value = value

    def __str__(self):
        return "%s *%s.%s %s= %s" % (self.mnemonic, self.pixel_array, self.attr, self.symbol, self.value)

    def assemble(self):
        bc = [self.opcode]
        
        bc.append(insPixelArray(self.pixel_array))
        bc.append(PIX_ATTR_CODES[self.attr])

        bc.extend(self.value.assemble())

        return bc

class insPixelVectorMov(insPixelVector):
    mnemonic = 'PMOV'
    op = "mov"
    symbol = "="

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        array = vm.gfx_data[self.attr]

        # this is a shortcut to allow assignment 1.0 to be maximum, instead
        # of rolling over to 0.0.
        if value == 65536:
            value = 65535

        value %= 65536

        for i in xrange(len(array)):
            array[i] = value
    
class insPixelVectorAdd(insPixelVector):
    mnemonic = 'PADD'
    op = "add"
    symbol = "+"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        array = vm.gfx_data[self.attr]

        if self.attr == 'hue':
            for i in xrange(len(array)):
                array[i] += value

                array[i] %= 65536


        else:
            for i in xrange(len(array)):
                array[i] += value

                if array[i] < 0:
                    array[i] = 0

                elif array[i] > 65535:
                    array[i] = 65535

class insPixelVectorSub(insPixelVector):
    mnemonic = 'PSUB'
    op = "sub"
    symbol = "-"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        array = vm.gfx_data[self.attr]

        if self.attr == 'hue':
            for i in xrange(len(array)):
                array[i] -= value

                array[i] %= 65536


        else:
            for i in xrange(len(array)):
                array[i] -= value

                if array[i] < 0:
                    array[i] = 0

                elif array[i] > 65535:
                    array[i] = 65535

class insPixelVectorMul(insPixelVector):
    mnemonic = 'PMUL'
    op = "mul"
    symbol = "*"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        array = vm.gfx_data[self.attr]

        if self.attr == 'hue':
            for i in xrange(len(array)):
                array[i] = (array[i] * value) / 65536

                array[i] %= 65536

        elif self.attr in ['hs_fade', 'v_fade']:
            for i in xrange(len(array)):

                array[i] = array[i] * value

                array[i] %= 65536

        else:
            for i in xrange(len(array)):
                array[i] = (array[i] * value) / 65536

                if array[i] < 0:
                    array[i] = 0

                elif array[i] > 65535:
                    array[i] = 65535

class insPixelVectorDiv(insPixelVector):
    mnemonic = 'PDIV'
    op = "div"
    symbol = "/"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        array = vm.gfx_data[self.attr]

        # check for divide by zero
        if value == 0:
            for i in xrange(len(array)):
                array[i] = value

        elif self.attr in ['hue']:
            for i in xrange(len(array)):
                array[i] = (array[i] * 65536) / value

                array[i] %= 65536

        elif self.attr in ['hs_fade', 'v_fade']:
            for i in xrange(len(array)):
                array[i] = array[i] / value

                array[i] %= 65536

        else:
            for i in xrange(len(array)):
                array[i] = (array[i] * 65536) / value

                if array[i] < 0:
                    array[i] = 0

                elif array[i] > 65535:
                    array[i] = 65535

class insPixelVectorMod(insPixelVector):
    mnemonic = 'PMOD'
    op = "mod"
    symbol = "%"

    def execute(self, vm):
        value = vm.memory[self.value.addr]
        array = vm.gfx_data[self.attr]

        # check for divide by zero
        if value == 0:
            for i in xrange(len(array)):
                array[i] = value

        elif self.attr == 'hue':
            for i in xrange(len(array)):
                array[i] %= value

                array[i] %= 65536

        else:
            for i in xrange(len(array)):
                array[i] %= value

                if array[i] < 0:
                    array[i] = 0

                elif array[i] > 65535:
                    array[i] = 65535


    
class insPixelStore(BaseInstruction):
    mnemonic = 'PSTORE'

    def __init__(self, pixel_array, attr, indexes, value):
        super(insPixelStore, self).__init__()
        self.pixel_array = pixel_array
        self.attr = attr
        self.indexes = indexes
        self.value = value

        assert len(self.indexes) == 2

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)

        return "%s %s.%s%s = %s" % (self.mnemonic, self.pixel_array, self.attr, indexes, self.value)

    def execute(self, vm):
        if self.attr in vm.gfx_data:
            array = vm.gfx_data[self.attr]

            index_x = vm.memory[self.indexes[0].addr]
            index_y = vm.memory[self.indexes[1].addr]

            a = vm.memory[self.value.addr]

            # most attributes will rail to 0 to 65535
            if a < 0:
                a = 0
            elif a > 65535:
                a = 65535
            
            array[vm.calc_index(index_x, index_y)] = a

        else:
            # pixel attributes not settable in code for now
            pass 

    def assemble(self):
        bc = [self.opcode]
        bc.append(insPixelArray(self.pixel_array))

        index_x = self.indexes[0]
        bc.extend(index_x.assemble())
        index_y = self.indexes[1]
        bc.extend(index_y.assemble())

        bc.extend(self.value.assemble())

        return bc



class insPixelStoreHue(insPixelStore):
    mnemonic = 'PSTORE_HUE'

    def execute(self, vm):
        if self.attr in vm.gfx_data:
            array = vm.gfx_data[self.attr]

            index_x = vm.memory[self.indexes[0].addr]
            try:
                index_y = vm.memory[self.indexes[1].addr]

            except IndexError:
                index_y = 65535

            a = vm.memory[self.value.addr]

            # this is a shortcut to allow assignment 1.0 to be maximum, instead
            # of rolling over to 0.0.
            if a == 65536:
                a = 65535

            # hue will wrap around
            a %= 65536
            
            array[vm.calc_index(index_x, index_y)] = a

        else:
            # pixel attributes not settable in code for now
            pass 
    
class insPixelStoreSat(insPixelStore):
    mnemonic = 'PSTORE_SAT'

class insPixelStoreVal(insPixelStore):
    mnemonic = 'PSTORE_VAL'

class insPixelStorePVal(insPixelStore):
    mnemonic = 'PSTORE_PVAL'

class insPixelStoreHSFade(insPixelStore):
    mnemonic = 'PSTORE_HSFADE'

class insPixelStoreVFade(insPixelStore):
    mnemonic = 'PSTORE_VFADE'


class insPixelLoad(BaseInstruction):
    mnemonic = 'PLOAD'

    def __init__(self, target, pixel_array, attr, indexes):
        super(insPixelLoad, self).__init__()
        self.pixel_array = pixel_array
        self.attr = attr
        self.indexes = indexes
        self.target = target

        assert len(self.indexes) == 2

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)

        return "%s %s = %s.%s%s" % (self.mnemonic, self.target, self.pixel_array, self.attr, indexes)

    def execute(self, vm):
        if self.attr in vm.gfx_data:
            array = vm.gfx_data[self.attr]

            index_x = vm.memory[self.indexes[0].addr]
            index_y = vm.memory[self.indexes[1].addr]

            vm.memory[self.target.addr] = array[vm.calc_index(index_x, index_y)]

        else:
            pixel_array = vm.pixel_arrays[self.pixel_array]

            vm.memory[self.target.addr] = pixel_array[self.attr]


    def assemble(self):
        bc = [self.opcode]
        bc.append(insPixelArray(self.pixel_array))

        index_x = self.indexes[0]
        bc.extend(index_x.assemble())
        index_y = self.indexes[1]
        bc.extend(index_y.assemble())

        bc.extend(self.target.assemble())

        return bc


class insPixelLoadHue(insPixelLoad):
    mnemonic = 'PLOAD_HUE'

class insPixelLoadSat(insPixelLoad):
    mnemonic = 'PLOAD_SAT'

class insPixelLoadVal(insPixelLoad):
    mnemonic = 'PLOAD_VAL'

class insPixelLoadPVal(insPixelLoad):
    mnemonic = 'PLOAD_PVAL'

class insPixelLoadHSFade(insPixelLoad):
    mnemonic = 'PLOAD_HSFADE'

class insPixelLoadVFade(insPixelLoad):
    mnemonic = 'PLOAD_VFADE'



class insPixelFading(insPixelLoad):
    def execute(self, vm):
        # not implemented for now, because the Python VM doesn't have faders

        pass


class insPixelIsHSFade(insPixelFading):
    mnemonic = 'IS_HS_FADING'

class insPixelIsVFade(insPixelFading):
    mnemonic = 'IS_V_FADING'



class insDBStore(BaseInstruction):
    mnemonic = 'DB_STORE'

    def __init__(self, db_item, indexes, value):
        super(insDBStore, self).__init__()
        self.db_item = db_item
        self.indexes = indexes
        self.value = value

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.var.name)

        return "%s db.%s%s = %s" % (self.mnemonic, self.db_item, indexes, self.value)

    def execute(self, vm):
        try:
            if len(vm.db[self.db_item]) <= 1:
                raise TypeError

            try:
                index = self.indexes[0].var.value % len(vm.db[self.db_item])
                vm.db[self.db_item][index] = vm.memory[self.value.addr]

            except IndexError:
                for i in xrange(len(vm.db[self.db_item])):
                    vm.db[self.db_item][i] = vm.memory[self.value.addr]                    

        except TypeError:
            vm.db[self.db_item] = vm.memory[self.value.addr]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(hash_to_bc(self.db_item))

        bc.append(len(self.indexes))
        for index in self.indexes:
            bc.extend(index.assemble())

        value_type = get_type_id(self.value.var.type)
        bc.append(value_type)

        bc.extend(self.value.assemble())

        return bc


class insDBLoad(BaseInstruction):
    mnemonic = 'DB_LOAD'

    def __init__(self, target, db_item, indexes):
        super(insDBLoad, self).__init__()
        self.db_item = db_item
        self.indexes = indexes
        self.target = target

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.var.name)

        return "%s %s = db.%s%s" % (self.mnemonic, self.target, self.db_item, indexes)

    def execute(self, vm):
        try:
            if len(vm.db[self.db_item]) <= 1:
                raise TypeError

            try:
                index = self.indexes[0].var.value % len(vm.db[self.db_item])

            except IndexError:
                index = 0 

            vm.memory[self.target.addr] = vm.db[self.db_item][index]

        except TypeError:
            vm.memory[self.target.addr] = vm.db[self.db_item]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(hash_to_bc(self.db_item))

        bc.append(len(self.indexes))
        for index in self.indexes:
            bc.extend(index.assemble())

        target_type = get_type_id(self.target.var.type)
        bc.append(target_type)
        bc.extend(self.target.assemble())

        return bc

class insConvMov(insMov):
    mnemonic = 'MOV'


class insConvI32toF16(BaseInstruction):
    mnemonic = 'CONV_I32_TO_F16'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = F16(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.memory[self.dest.addr] = (vm.memory[self.src.addr] << 16) & 0xffffffff

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insConvF16toI32(BaseInstruction):
    mnemonic = 'CONV_F16_TO_I32'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = I32(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.memory[self.dest.addr] = int(vm.memory[self.src.addr] / 65536.0)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

