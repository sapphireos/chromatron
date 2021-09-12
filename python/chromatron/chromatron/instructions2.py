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

import logging
from catbus import *
from sapphire.common import catbus_string_hash
# import numpy as np

from .exceptions import *

MAX_INT32 =  2147483647
MIN_INT32 = -2147483648
MAX_UINT32 = 4294967295

class ReturnException(Exception):
    pass

class VMException(Exception):
    pass

class CycleLimitExceeded(VMException):
    pass

opcodes = {
    'MOV':                  0x01,
    'LDI':                  0x02,
    'LDC':                  0x03,
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
    def __init__(self, funcs={}, global_vars={}):
        self.funcs = funcs
        self.globals = global_vars

        # initialize memory
        memory_size = 0
        for v in self.globals.values():
            memory_size += v.size

        self.memory = [0] * memory_size

        for func in self.funcs.values():
            func.memory = self.memory
            func.program = self

    def __str__(self):
        s = 'VM Instructions:\n'

        for func in self.funcs.values():
            s += str(func)

        return s

    def run(self):
        pass

    def run_func(self, func):
        return self.funcs[func].run()

    def assemble(self):
        pass

class insFunc(object):
    def __init__(self, name, params=[], code=[], source_code=[], local_size=None, register_count=None, lineno=None):
        self.name = name
        self.params = params
        self.code = code
        self.source_code = source_code
        self.register_count = register_count
        self.lineno = lineno

        self.registers = None
        self.memory = None
        self.locals = None
        self.local_size = local_size
        self.return_val = None
        self.program = None

        self.cycle_limit = 16384

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

        s += f"Registers: {self.register_count}\n"
        s += f'VM Instructions: {len([i for i in self.code if not isinstance(i, insLabel)])}\n'

        return s

    def prune_jumps(self):
        iterations = 0
        iteration_limit = 128

        old_length = len(self.code)

        new_code = []
        prev_code = self.code
        while prev_code != new_code:
            if iterations > iteration_limit:
                raise CompilerFatal(f'Prune jumps failed after {iterations} iterations')
                break

            new_code = []
            prev_code = self.code

            # this is a peephole optimization
            # note we skip the last instruction in the loop, since
            # we check current + 1 in the peephole
            for index in range(len(self.code) - 1):
                ins = self.code[index]
                next_ins = self.code[index + 1]

                # if a jump followed by a label:
                if isinstance(ins, insJmp) and isinstance(next_ins, insLabel):
                    # check if label is jump target:
                    if ins.label.name == next_ins.name:
                        # skip this jump
                        pass

                    else:
                        new_code.append(ins)

                else:
                    new_code.append(ins)

            # append last instruction (since loop will miss it)
            new_code.append(self.code[-1])

            self.code = new_code

            iterations += 1
        
        logging.debug(f'Prune jumps in {iterations} iterations. Eliminated {old_length - len(self.code)} instructions')


    def run(self, *args, **kwargs):
        logging.info(f'VM run func {self.name}')

        if not self.register_count:
            raise VMException("Registers are not initialized!")

        labels = {}

        # scan code stream and get offsets for all functions and labels
        for i in range(len(self.code)):
            ins = self.code[i]
            if isinstance(ins, insLabel):
                labels[ins.name] = i

        self.registers = [0] * self.register_count
        self.locals = [0] * self.local_size
        registers = self.registers # just makes debugging a bit easier
        memory = self.memory
        local = self.locals

        # apply args to registers
        for i in range(len(args)):
            self.registers[self.params[i].reg] = args[i]

        self.return_val = 0

        pc = 0
        cycles = 0

        while True:
            cycles += 1

            if cycles > self.cycle_limit:
                raise CycleLimitExceeded


            ins = self.code[pc]

            # print(cycles, pc, ins)

            pc += 1

            if isinstance(ins, insLabel):
                continue

            try:    
                ret_val = ins.execute(self)

                # if returning label to jump to
                if ret_val != None:
                    # jump to target
                    pc = labels[ret_val.name]
                
            except ReturnException:
                logging.info(f'VM ran func {self.name} in {cycles} cycles. Returned: {self.return_val}')

                return self.return_val

            except AssertionError:
                msg = f'Assertion [{self.source_code[ins.lineno - 1].strip()}] failed at line {ins.lineno}'
                logging.error(msg)

                raise AssertionError(msg)

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
    mnemonic = ''

    def __init__(self, reg=None, var=None, **kwargs):
        super().__init__(**kwargs)
        self.reg = reg
        self.var = var

    def __str__(self):
        if self.var is not None:
            s = "%s(%s @ %s)" % (self.var.ssa_name, self.var.data_type, self.reg)
            return f'{s:18}'

        else:
            return "Reg(%s)" % (self.reg)
    
    def assemble(self):
        # convert to 16 bits
        l = self.reg & 0xff
        h = (self.reg >> 8) & 0xff

        return [l, h]

# pseudo instruction - does not actually produce an opcode
class insAddr(BaseInstruction):
    def __init__(self, addr=None, var=None, **kwargs):
        super().__init__(**kwargs)
        self.addr = addr
        self.var = var

    def __str__(self):
        if self.var != None:
            return "%s(%s @ %s)" % (self.var.name, self.var.data_type, self.addr)

        else:
            return "Addr(%s)" % (self.addr)
    
    def assemble(self):
        # convert to 16 bits
        l = self.addr & 0xff
        h = (self.addr >> 8) & 0xff

        return [l, h]


class insNop(BaseInstruction):
    def execute(self, vm):
        pass

    def assemble(self):
        return []


# register to register move
class insMov(BaseInstruction):
    mnemonic = 'MOV'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.registers[self.dest.reg] = vm.registers[self.src.reg]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

# loads from 16 bit immediate value
class insLoadImmediate(BaseInstruction):
    mnemonic = 'LDI'

    def __init__(self, dest, value, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.value = value

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.value)

    def execute(self, vm):
        vm.registers[self.dest.reg] = self.value

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())

        return bc

# loads from constant pool
class insLoadConst(BaseInstruction):
    mnemonic = 'LDC'

    def __init__(self, dest, value, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.value = value

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.value)

    def execute(self, vm):
        vm.registers[self.dest.reg] = self.value

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())

        return bc

class insLoadGlobal(BaseInstruction):
    mnemonic = 'LDG'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.src is not None

    def __str__(self):
        return "%s %s <-G %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        src = vm.registers[self.src.reg]
        vm.registers[self.dest.reg] = vm.memory[src]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc


class insLoadLocal(BaseInstruction):
    mnemonic = 'LDL'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.src is not None

    def __str__(self):
        return "%s %s <-L %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        src = vm.registers[self.src.reg]
        vm.registers[self.dest.reg] = vm.locals[src]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insLoadRef(BaseInstruction):
    mnemonic = 'REF'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.src is not None

    def __str__(self):
        return "%s %s <-R %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.registers[self.dest.reg] = self.src

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc



class insLoadGlobalImmediate(BaseInstruction):
    mnemonic = 'LDGI'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.src is not None

    def __str__(self):
        return "%s %s <-G 0x%s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.registers[self.dest.reg] = vm.memory[self.src.addr]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insStoreGlobal(BaseInstruction):
    mnemonic = 'STG'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.dest is not None

    def __str__(self):
        return "%s %s <-G %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        dest = vm.registers[self.dest.reg]
        vm.memory[dest] = vm.registers[self.src.reg]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insStoreLocal(BaseInstruction):
    mnemonic = 'STL'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.dest is not None

    def __str__(self):
        return "%s %s <-L %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        dest = vm.registers[self.dest.reg]
        vm.locals[dest] = vm.registers[self.src.reg]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc

class insStoreGlobalImmediate(BaseInstruction):
    mnemonic = 'STGI'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.dest is not None

    def __str__(self):
        return "%s 0x%s <-G %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.memory[self.dest.addr] = vm.registers[self.src.reg]

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc


class insLookup(BaseInstruction):
    mnemonic = 'LKP'
    
    def __init__(self, result, base_addr, indexes, counts, strides, **kwargs):
        super().__init__(**kwargs)
        self.result = result
        self.base_addr = base_addr
        self.indexes = indexes
        self.counts = counts
        self.strides = strides

        assert base_addr is not None

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)
        return "%s %s <- 0x%s %s" % (self.mnemonic, self.result, hex(self.base_addr.addr), indexes)

    def execute(self, vm):
        addr = self.base_addr.addr

        for i in range(len(self.indexes)):
            index = vm.registers[self.indexes[i].reg]
            
            count = self.counts[i]
            stride = self.strides[i]            

            if count > 0:
                index %= count
                index *= stride

            addr += index

        vm.registers[self.result.reg] = addr

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
        if vm.registers[self.op1.reg] == 0:
            return self.label

class insReturn(BaseInstruction):
    mnemonic = 'RET'

    def __init__(self, op1, **kwargs):
        super().__init__(**kwargs)
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def execute(self, vm):
        vm.return_val = vm.registers[self.op1.reg]

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

class insCompare(insBinop):    
    def execute(self, vm):
        value = self.compare(vm)

        if value:
            vm.registers[self.result.reg] = 1

        else:
            vm.registers[self.result.reg] = 0

class insCompareEq(insCompare):
    mnemonic = 'COMP_EQ'
    symbol = "=="

    def compare(self, vm):
        return vm.registers[self.op1.reg] == vm.registers[self.op2.reg]

class insCompareNeq(insCompare):
    mnemonic = 'COMP_NEQ'
    symbol = "!="

    def compare(self, vm):
        return vm.registers[self.op1.reg] != vm.registers[self.op2.reg]

class insCompareGt(insCompare):
    mnemonic = 'COMP_GT'
    symbol = ">"

    def compare(self, vm):
        return vm.registers[self.op1.reg] > vm.registers[self.op2.reg]

class insCompareGtE(insCompare):
    mnemonic = 'COMP_GTE'
    symbol = ">="

    def compare(self, vm):
        return vm.registers[self.op1.reg] >= vm.registers[self.op2.reg]

class insCompareLt(insCompare):
    mnemonic = 'COMP_LT'
    symbol = "<"

    def compare(self, vm):
        return vm.registers[self.op1.reg] < vm.registers[self.op2.reg]

class insCompareLtE(insCompare):
    mnemonic = 'COMP_LTE'
    symbol = "<="

    def compare(self, vm):
        return vm.registers[self.op1.reg] <= vm.registers[self.op2.reg]

class insAnd(insBinop):
    mnemonic = 'AND'
    symbol = "AND"

    def execute(self, vm):
        vm.registers[self.result.reg] = vm.registers[self.op1.reg] and vm.registers[self.op2.reg]

class insOr(insBinop):
    mnemonic = 'OR'
    symbol = "OR"

    def execute(self, vm):
        vm.registers[self.result.reg] = vm.registers[self.op1.reg] or vm.registers[self.op2.reg]


class insArith(insBinop):    
    def execute(self, vm):
        result = self.arith(vm.registers[self.op1.reg], vm.registers[self.op2.reg]) & 0xffffffff

        if result > 0x8000000:
            result = result - 0x100000000

        # vm.registers[self.result.reg] = int((self.arith(vm.registers[self.op1.reg] & 0xffffffff, vm.registers[self.op2.reg] & 0xffffffff)) & 0xffffffff)
        vm.registers[self.result.reg] = result 

class insAdd(insArith):
    mnemonic = 'ADD'
    symbol = "+"

    def arith(self, op1, op2):
        return op1 + op2

class insSub(insArith):
    mnemonic = 'SUB'
    symbol = "-"

    def arith(self, op1, op2):
        return op1 - op2

class insMul(insArith):
    mnemonic = 'MUL'
    symbol = "*"

    def arith(self, op1, op2):
        return op1 * op2

class insDiv(insArith):
    mnemonic = 'DIV'
    symbol = "/"

    def arith(self, op1, op2):
        if op2 == 0:
            return 0

        else:
            return op1 // op2

class insMod(insArith):
    mnemonic = 'MOD'
    symbol = "%"

    def arith(self, op1, op2):
        if op2 == 0:
            return 0

        else:
            return op1 % op2


class insVector(BaseInstruction):
    mnemonic = 'VECTOR'

    def __init__(self, target, value, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.value = value

        #self.type = self.target.var.get_base_type()

        self.length = self.target.var.ref.size

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
        value = vm.registers[self.value.reg]
        addr = vm.registers[self.target.reg]

        for i in range(self.length):
            vm.memory[addr] = value
            addr += 1



class insAssert(BaseInstruction):
    mnemonic = 'ASSERT'

    def __init__(self, op1, **kwargs):
        super().__init__(**kwargs)
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def execute(self, vm):
        value = vm.registers[self.op1.reg]
            
        assert value        

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.op1.assemble())
        
        return bc


class insCall(BaseInstruction):
    mnemonic = 'CALL'

    def __init__(self, target, params=[], result=None, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.params = params
        self.result = result

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s (%s) -> %s" % (self.mnemonic, self.target.name, params, self.result)

    def execute(self, vm):
        ret_val = self.target.run(*[vm.registers[p.reg] for p in self.params])

        vm.registers[self.result.reg] = ret_val

    def assemble(self):
        bc = [self.opcode]
        bc.extend(insFuncTarget(self.target).assemble())

        assert len(self.params) == len(self.args)

        bc.append(len(self.params))
        for i in range(len(self.params)):
            bc.extend(self.params[i].assemble())
            bc.extend(self.args[i].assemble())

        return bc

class insIndirectCall(BaseInstruction):
    mnemonic = 'ICALL'

    def __init__(self, ref, params=[], result=None, **kwargs):
        super().__init__(**kwargs)
        self.ref = ref
        self.params = params
        self.result = result

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s (%s) -> %s" % (self.mnemonic, self.ref, params, self.result)

    def execute(self, vm):
        func = vm.registers[self.ref.reg]

        target = vm.program.funcs[func.var.name]

        ret_val = target.run(*[vm.registers[p.reg] for p in self.params])

        vm.registers[self.result.reg] = ret_val
        
    def assemble(self):
        bc = [self.opcode]
        bc.extend(insFuncTarget(self.ref).assemble())

        assert len(self.params) == len(self.args)

        bc.append(len(self.params))
        for i in range(len(self.params)):
            bc.extend(self.params[i].assemble())
            bc.extend(self.args[i].assemble())

        return bc
