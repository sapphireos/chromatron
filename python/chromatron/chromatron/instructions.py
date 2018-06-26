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


class ReturnException(Exception):
    pass


opcodes = {
    'MOV':                  0x01,

    'COMP_EQ':              0x02,
    'COMP_NEQ':             0x03,
    'COMP_GT':              0x04,
    'COMP_GTE':             0x05,
    'COMP_LT':              0x06,
    'COMP_LTE':             0x07,
    'AND':                  0x08,
    'OR':                   0x09,
    'ADD':                  0x0A,
    'SUB':                  0x0B,
    'MUL':                  0x0C,
    'DIV':                  0x0D,
    'MOD':                  0x0E,

    'JMP':                  0x0F,

    'JMP_IF_Z':             0x10,
    'JMP_IF_NOT_Z':         0x11,
    # 'JMP_IF_Z_DEC':         0x12,
    # 'JMP_IF_GTE':           0x13,
    'JMP_IF_LESS_PRE_INC':  0x14,

    'PRINT':                0x15,

    'RET':                  0x16,
    'CALL':                 0x17,

    'INDEX':                0x18,

    'LOAD_INDIRECT':        0x19,
    'STORE_INDIRECT':       0x1A,

    'RAND':                 0x1B,
    'ASSERT':               0x1C,
    'HALT':                 0x1D,

    'VECTOR':               0x1E,

    # 'VADD':                 0x1E,
    # 'VSUB':                 0x1F,
    # 'VMUL':                 0x20,
    # 'VDIV':                 0x21,
    # 'VMOD':                 0x22,
    # 'VMOV':                 0x23,

    'CONV_I32_TO_F16':      0x24,
    'CONV_F16_TO_I32':      0x25,

    'F16_COMP_EQ':          0x26,
    'F16_COMP_NEQ':         0x27,
    'F16_COMP_GT':          0x28,
    'F16_COMP_GTE':         0x29,
    'F16_COMP_LT':          0x2A,
    'F16_COMP_LTE':         0x2B,
    'F16_AND':              0x2C,
    'F16_OR':               0x2D,
    'F16_ADD':              0x2E,
    'F16_SUB':              0x2F,
    'F16_MUL':              0x30,
    'F16_DIV':              0x31,
    'F16_MOD':              0x32,
}


class BaseInstruction(object):
    mnemonic = 'NOP'
    opcode = None

    def __str__(self):
        return self.mnemonic

    def assemble(self):
        raise NotImplementedError(self.mnemonic)

    def execute(self, memory):
        raise NotImplementedError(self.mnemonic)

    def len(self):
        return len(self.assemble())

    @property
    def opcode(self):
        global opcodes
        return opcodes[self.mnemonic]

class insNop(BaseInstruction):
    def execute(self, memory):
        pass

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
        return [self.addr]


class insLabel(BaseInstruction):
    def __init__(self, name=None):
        self.name = name

    def __str__(self):
        return "Label(%s)" % (self.name)

    def execute(self, memory):
        pass

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

    def execute(self, memory):
        pass


class insMov(BaseInstruction):
    mnemonic = 'MOV'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, memory):
        memory[self.dest.addr] = memory[self.src.addr]

    # def assemble(self):
    #     bc = [self.opcode]
    #     bc.extend(self.dest.assemble())
    #     bc.extend(self.src.assemble())

    #     return bc

class insNot(BaseInstruction):
    mnemonic = 'NOT'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = NOT %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, memory):
        if memory[self.src.addr] == 0:
            memory[self.dest.addr] = 1

        else:            
            memory[self.dest.addr] = 0

    # def assemble(self):
    #     bc = [self.opcode]
    #     bc.extend(self.dest.assemble())
    #     bc.extend(self.src.assemble())

    #     return bc

class insBinop(BaseInstruction):
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

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] == memory[self.op2.addr]

class insCompareNeq(insBinop):
    mnemonic = 'COMP_NEQ'
    symbol = "!="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] != memory[self.op2.addr]

class insCompareGt(insBinop):
    mnemonic = 'COMP_GT'
    symbol = ">"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] > memory[self.op2.addr]

class insCompareGtE(insBinop):
    mnemonic = 'COMP_GTE'
    symbol = ">="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] >= memory[self.op2.addr]

class insCompareLt(insBinop):
    mnemonic = 'COMP_LT'
    symbol = "<"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] < memory[self.op2.addr]

class insCompareLtE(insBinop):
    mnemonic = 'COMP_LTE'
    symbol = "<="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] <= memory[self.op2.addr]

class insAnd(insBinop):
    mnemonic = 'AND'
    symbol = "AND"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] and memory[self.op2.addr]

class insOr(insBinop):
    mnemonic = 'OR'
    symbol = "OR"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] or memory[self.op2.addr]

class insAdd(insBinop):
    mnemonic = 'ADD'
    symbol = "+"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] + memory[self.op2.addr]

class insSub(insBinop):
    mnemonic = 'SUB'
    symbol = "-"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] - memory[self.op2.addr]

class insMul(insBinop):
    mnemonic = 'MUL'
    symbol = "*"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] * memory[self.op2.addr]

class insDiv(insBinop):
    mnemonic = 'DIV'
    symbol = "/"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] / memory[self.op2.addr]

class insMod(insBinop):
    mnemonic = 'MOD'
    symbol = "%"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] % memory[self.op2.addr]


class insF16CompareEq(insBinop):
    mnemonic = 'F16_COMP_EQ'
    symbol = "=="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] == memory[self.op2.addr]

class insF16CompareNeq(insBinop):
    mnemonic = 'F16_COMP_NEQ'
    symbol = "!="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] != memory[self.op2.addr]

class insF16CompareGt(insBinop):
    mnemonic = 'F16_COMP_GT'
    symbol = ">"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] > memory[self.op2.addr]

class insF16CompareGtE(insBinop):
    mnemonic = 'F16_COMP_GTE'
    symbol = ">="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] >= memory[self.op2.addr]

class insF16CompareLt(insBinop):
    mnemonic = 'F16_COMP_LT'
    symbol = "<"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] < memory[self.op2.addr]

class insF16CompareLtE(insBinop):
    mnemonic = 'F16_COMP_LTE'
    symbol = "<="

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] <= memory[self.op2.addr]

class insF16And(insBinop):
    mnemonic = 'F16_AND'
    symbol = "AND"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] and memory[self.op2.addr]

class insF16Or(insBinop):
    mnemonic = 'F16_OR'
    symbol = "OR"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] or memory[self.op2.addr]

class insF16Add(insBinop):
    mnemonic = 'F16_ADD'
    symbol = "+"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] + memory[self.op2.addr]

class insF16Sub(insBinop):
    mnemonic = 'F16_SUB'
    symbol = "-"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] - memory[self.op2.addr]

class insF16Mul(insBinop):
    mnemonic = 'F16_MUL'
    symbol = "*"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] * memory[self.op2.addr]

class insF16Div(insBinop):
    mnemonic = 'F16_DIV'
    symbol = "/"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] / memory[self.op2.addr]

class insF16Mod(insBinop):
    mnemonic = 'F16_MOD'
    symbol = "%"

    def execute(self, memory):
        memory[self.result.addr] = memory[self.op1.addr] % memory[self.op2.addr]


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
    def execute(self, memory):
        return self.label

class insJmpConditional(BaseJmp):
    def __init__(self, op1, label):
        super(insJmpConditional, self).__init__(label)

        self.op1 = op1

    def __str__(self):
        return "%s, %s -> %s" % (self.mnemonic, self.op1, self.label)

    # def assemble(self):
        # return [self.opcode, self.op1.addr, ('label', self.label.name), 0]


class insJmpIfZero(insJmpConditional):
    mnemonic = 'JMP_IF_Z'

    def execute(self, memory):
        if memory[self.op1.addr] == 0:
            return self.label

class insJmpNotZero(insJmpConditional):
    mnemonic = 'JMP_IF_NOT_Z'

    def execute(self, memory):
        if memory[self.op1.addr] != 0:
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

    # def assemble(self):
        # return [self.opcode, self.op1.addr, self.op2.addr, ('label', self.label.name), 0]


class insJmpIfLessThanPreInc(BaseJmp):
    mnemonic = 'JMP_IF_LESS_PRE_INC'

    def __init__(self, op1, op2, label):
        super(insJmpIfLessThanPreInc, self).__init__(label)

        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%s, ++%s < %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)

    def execute(self, memory):
        # increment op1
        memory[self.op1.addr] += 1

        # compare to op2
        if memory[self.op1.addr] < memory[self.op2.addr]:
            # return jump target
            return self.label

    # def assemble(self):
        # return [self.opcode, self.op1.addr, self.op2.addr, ('label', self.label.name), 0]

class insReturn(BaseInstruction):
    mnemonic = 'RET'

    def __init__(self, op1):
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def execute(self, memory):
        memory[0] = memory[self.op1.addr]

        raise ReturnException

    # def assemble(self):
        # return [self.opcode, self.op1.addr]

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

    def execute(self, memory):
        # load arguments with parameters
        for i in xrange(len(self.params)):
            param = self.params[i]
            arg = self.args[i]

            memory[arg.addr] = memory[param.addr]

        return insLabel(self.target)


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
        }

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s = %s (%s)" % (self.mnemonic, self.result, self.target, params)

    def _len(self, memory):
        return self.params[0].var.dimensions[0]

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

    def execute(self, memory):
        result = self.lib_funcs[self.target](memory)

        memory[self.result.addr] = result


class insIndex(BaseInstruction):
    mnemonic = 'INDEX'

    def __init__(self, result, target, indexes):
        self.result = result
        self.target = target
        self.indexes = indexes

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)
        return "%s %s <- %s %s" % (self.mnemonic, self.result, self.target, indexes)

    def execute(self, memory):
        addr = self.target.addr

        for i in xrange(len(self.indexes)):
            index = memory[self.indexes[i].addr]
            length = self.target.var.dimensions[i]
            stride = self.target.var.strides[i]

            index %= length
            index *= stride

            addr += index

        memory[self.result.addr] = addr

class insIndirectLoad(BaseInstruction):
    mnemonic = 'LOAD_INDIRECT'

    def __init__(self, dest, addr):
        self.dest = dest
        self.addr = addr

    def __str__(self):
        return "%s %s <- *%s" % (self.mnemonic, self.dest, self.addr)
    
    def execute(self, memory):
        addr = memory[self.addr.addr]
        memory[self.dest.addr] = memory[addr]

    # def assemble(self):
        # return [self.opcode, self.dest.addr, self.src.addr, self.index.addr]



class insIndirectStore(BaseInstruction):
    mnemonic = 'STORE_INDIRECT'

    def __init__(self, src, addr):
        self.src = src
        self.addr = addr

    def __str__(self):
        return "%s *%s <- %s" % (self.mnemonic, self.addr, self.src)

    def execute(self, memory):
        addr = memory[self.addr.addr]
        memory[addr] = memory[self.src.addr]

    # def assemble(self):
        # return [self.opcode, self.dest.addr, self.src.addr, self.index.addr]






class insRand(BaseInstruction):
    mnemonic = 'RAND'

    def __init__(self, dest, start=0, end=65535):
        self.dest = dest
        self.start = start
        self.end = end

    def __str__(self):
        return "%s %s <- rand(%s, %s)" % (self.mnemonic, self.dest, self.start, self.end)

    # def assemble(self):
        # return [self.opcode, self.dest.addr, self.start.addr, self.end.addr]

class insAssert(BaseInstruction):
    mnemonic = 'ASSERT'

    def __init__(self, op1):
        self.op1 = op1

    def __str__(self):
        return "%s %s == TRUE" % (self.mnemonic, self.op1)

    # def assemble(self):
        # return [self.opcode, self.op1.addr]

class insHalt(BaseInstruction):
    mnemonic = 'HALT'
    
    def __init__(self):
        pass

    def __str__(self):
        return "%s" % (self.mnemonic)

    # def assemble(self):
        # return [self.opcode]


class insVector(BaseInstruction):
    mnemonic = 'VECTOR'

    def __init__(self, target, value):
        super(insVector, self).__init__()
        self.target = target
        self.value = value

    def __str__(self):
        return "%s %s %s= %s" % (self.mnemonic, self.target, self.symbol, self.value)

class insVectorMov(insVector):
    op = "mov"
    symbol = "="

    def execute(self, memory):
        value = memory[self.value.addr]
        addr = self.target.addr

        for i in xrange(self.target.var.length):
            memory[addr] = value
            addr += 1

class insVectorAdd(insVector):
    op = "add"
    symbol = "+"

    def execute(self, memory):
        value = memory[self.value.addr]
        addr = self.target.addr

        for i in xrange(self.target.var.length):
            memory[addr] += value
            addr += 1

class insVectorSub(insVector):
    op = "sub"
    symbol = "-"

    def execute(self, memory):
        value = memory[self.value.addr]
        addr = self.target.addr

        for i in xrange(self.target.var.length):
            memory[addr] -= value
            addr += 1

class insVectorMul(insVector):
    op = "mul"
    symbol = "*"

    def execute(self, memory):
        value = memory[self.value.addr]
        addr = self.target.addr

        for i in xrange(self.target.var.length):
            memory[addr] *= value
            addr += 1

class insVectorDiv(insVector):
    op = "div"
    symbol = "/"

    def execute(self, memory):
        value = memory[self.value.addr]
        addr = self.target.addr

        for i in xrange(self.target.var.length):
            memory[addr] /= value
            addr += 1

class insVectorMod(insVector):
    op = "mod"
    symbol = "%"

    def execute(self, memory):
        value = memory[self.value.addr]
        addr = self.target.addr

        for i in xrange(self.target.var.length):
            memory[addr] %= value
            addr += 1





class insConvI32toF16(BaseInstruction):
    mnemonic = 'CONV_I32_TO_F16'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = F16(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, memory):
        memory[self.dest.addr] = (memory[self.src.addr] << 16) & 0xffffffff

    # def assemble(self):
    #     bc = [self.opcode]
    #     bc.extend(self.dest.assemble())
    #     bc.extend(self.src.assemble())

    #     return bc

class insConvF16toI32(BaseInstruction):
    mnemonic = 'CONV_F16_TO_I32'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = I32(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, memory):
        memory[self.dest.addr] = int(memory[self.src.addr] / 65536.0)

    # def assemble(self):
    #     bc = [self.opcode]
    #     bc.extend(self.dest.assemble())
    #     bc.extend(self.src.assemble())

    #     return bc

