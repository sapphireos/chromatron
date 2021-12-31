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

from enum import Enum

from catbus import *
from sapphire.common import catbus_string_hash
from .opcode import *
from .image import FXImage

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

def string_hash_func(s):
    return catbus_string_hash(s)

def hash_to_bc(s):
    h = string_hash_func(s)
    
    return [(h >> 24) & 0xff, (h >> 16) & 0xff, (h >> 8) & 0xff, (h >> 0) & 0xff]

def convert_to_f16(value):
    return (value << 16) & 0xffffffff

def convert_to_i32(value):
    return int(value / 65536.0)

class StorageType(Enum):
    GLOBAL          = 0
    LOCAL           = 1
    PIXEL_ARRAY     = 2
    STRING_LITERALS = 3
    FUNCTIONS       = 4

class insProgram(object):
    def __init__(self, name, funcs={}, global_vars=[], objects=[], strings={}, call_graph={}):
        self.name = name
        self.globals = global_vars
        self.call_graph = call_graph

        # initialize memory
        self.global_memory_size = 0
        for v in self.globals:
            self.global_memory_size += v.size 

        self.memory = [0] * self.global_memory_size

        for v in [g for g in self.globals if g.data_type == 'strlit']:
            self.memory[v.addr.addr] = v.init_val

        for func in funcs.values():
            func.memory = self.memory
            func.program = self

        # get worst case stack depth
        self.stacks = {}
        def get_stack(func, call_graph):
            func_stack = func.local_memory_size

            worst_stack = 0
            for func_name in call_graph:
                temp_stack = get_stack(self.funcs[func_name], call_graph[func_name])

                if temp_stack > worst_stack:
                    worst_stack = temp_stack

            func_stack += worst_stack

            self.stacks[func.name] = func_stack

            return func_stack

        worst_stack = 0
        for func_name in self.call_graph:
            func_stack = get_stack(funcs[func_name], self.call_graph[func_name])

            if func_stack > worst_stack:
                worst_stack = func_stack

        self.maximum_stack_depth = worst_stack

        func_refs = sorted([f for f in objects if f.data_type == 'func'], key=lambda f: f.addr.addr)

        self.funcs = [funcs[f.name] for f in func_refs]

        self.objects = objects

        self.strings = strings

        self.pix_size_x = 4
        self.pix_size_y = 4

        # set up pixel arrays
        self.pix_count = self.pix_size_x * self.pix_size_y

        # self.pixel_arrays = {p.name: p for p in self.objects if p.data_type == 'PixelArray'}
        self.pixel_arrays = {}
        for p in [p for p in self.objects if p.data_type == 'PixelArray']:
            if 'pix_count' in p.keywords:
                pix_count = p.keywords['pix_count']

            else:
                pix_count = self.pix_count

            if 'pix_index' in p.keywords:
                pix_index = p.keywords['pix_index']

            else:
                pix_index = 0

            if 'pix_size_x' in p.keywords:
                pix_size_x = p.keywords['pix_size_x']

            else:
                pix_size_x = self.pix_size_x

            if 'pix_size_y' in p.keywords:
                pix_size_y = p.keywords['pix_size_y']

            else:
                pix_size_y = self.pix_size_y

            array = {
                'count': pix_count,
                'index': pix_index,
                'size_x': pix_size_x,
                'size_y': pix_size_y,
            }

            self.pixel_arrays[p] = array

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

        # set up constant pool
        # this is sourced from load const instructions
        self.constants = []

        for func in self.funcs:
            for ins in func.code:
                if not isinstance(ins, insLoadConst):
                    continue

                value = ins.value

                assert isinstance(value, int)
                if value not in self.constants:
                    self.constants.append(value)
                
                ins.src = self.constants.index(ins.value)

    def __str__(self):
        s = 'VM Instructions:\n'

        for func in self.funcs:
            s += str(func)

        return s

    def run(self):
        pass

    def run_func(self, func):
        return self.funcs[func].run()

    def assemble(self):
        bytecode = {}

        for func in self.funcs:
            bytecode[func.name] = func.assemble()

        return FXImage(self, bytecode)


class insFunc(object):
    def __init__(self, name, params=[], code=[], source_code=[], local_vars=None, register_count=None, return_stack=[], lineno=None):
        self.name = name
        self.params = params
        self.code = code
        self.source_code = source_code
        self.register_count = register_count
        self.lineno = lineno

        self.registers = None
        self.memory = None
        self.locals = None
        self.local_vars = local_vars
        self.return_val = None
        self.program = None
        self.return_stack = return_stack

        self.cycle_limit = 16384
        self.ret_val = None

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

    @property
    def local_memory_size(self):
        # size in WORDS not bytes
        size = self.register_count

        for l in self.local_vars:
            size += l.size
 
        return size

    @property
    def funcs(self):
        return self.program.funcs

    @property
    def objects(self):
        return self.program.objects

    @property
    def strings(self):
        return self.program.strings

    @property
    def gfx_data(self):
        return self.program.gfx_data

    @property
    def pixel_arrays(self):
        return self.program.pixel_arrays

    def get_pixel_array(self, index):
        try:
            return list(self.pixel_arrays.values())[index]

        except TypeError:
            return list(self.pixel_arrays.values())[index.addr]

    def calc_index(self, indexes=[], pixel_array='pixels'):
        try:
            # pixel array by name
            array = self.pixel_arrays[pixel_array]

        except KeyError:
            # pixel array by index
            array = self.get_pixel_array(pixel_array)

        except TypeError:
            # pixel array directly
            array = pixel_array


        count = array['count']
        index = array['index']
        size_x = array['size_x']
        size_y = array['size_y']

        if len(indexes) == 0:
            return 0

        x = indexes[0]

        y = 65535
        if len(indexes) >= 2:
            y = indexes[1]

        if y == 65535:
            i = x % count

        else:
            x %= size_x
            y %= size_y

            i = x + (y * size_x)

        pix_count = self.pixel_arrays['pixels']['count'] # TOTAL pixel count

        i = (i + index) % pix_count

        return i

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
        assert self.program is not None

        logging.info(f'VM run func {self.name}')

        if not self.register_count:
            raise VMException("Registers are not initialized!")

        labels = {}

        # scan code stream and get offsets for all functions and labels
        for i in range(len(self.code)):
            ins = self.code[i]
            if isinstance(ins, insLabel):
                labels[ins.name] = i

        # self.registers = [0] * self.register_count
        
        self.locals = [0] * self.register_count
        for l in self.local_vars:
            self.locals.extend(l.assemble())

        # registers and locals are mirrored
        self.registers = self.locals

        # just makes debugging a bit easier:
        registers = self.registers 
        memory = self.memory
        local = self.locals

        if self in self.return_stack:
            raise VMException(f'Recursive call of function: {self.name}')

        self.return_stack.insert(0, self)

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
                if isinstance(ret_val, insLabel):
                    # jump to target
                    pc = labels[ret_val.name]
                
            except ReturnException:
                logging.info(f'VM ran func {self.name} in {cycles} cycles. Returned: {self.return_val}')

                self.return_stack.pop(0)

                return self.return_val

            except AssertionError:
                msg = f'Assertion [{self.source_code[ins.lineno - 1].strip()}] failed at line {ins.lineno}'
                logging.error(msg)

                raise AssertionError(msg)

    def assemble(self):
        return [ins.assemble() for ins in self.code]


class BaseInstruction(object):
    mnemonic = '_INS'

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

# pseudo instruction - does not actually produce an opcode
class insReg(BaseInstruction):
    mnemonic = '_REG'

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
        return self.reg
        
# pseudo instruction - does not actually produce an opcode
class insAddr(BaseInstruction):
    mnemonic = '_ADDR'

    def __init__(self, addr=None, var=None, storage=None, **kwargs):
        super().__init__(**kwargs)
        self.addr = addr
        self.var = var
        self.storage = storage

    def __str__(self):
        if self.var != None:
            return "%s(%s @ %s: %s)" % (self.var.name, self.var.data_type, self.addr, self.storage)

        else:
            return "Addr(%s)" % (self.addr)
    
    def assemble(self):
        assert self.addr >= 0

        return self.addr

class insNop(BaseInstruction):
    mnemonic = 'NOP'

    def execute(self, vm):
        pass


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
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

# loads from 16 bit immediate value
class insLoadImmediate(BaseInstruction):
    mnemonic = 'LDI'

    def __init__(self, dest, value, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest

        if isinstance(value, float):
            value = int(value * 65536)
        else:
            value = value

        self.value = value

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.value)

    def execute(self, vm):
        vm.registers[self.dest.reg] = self.value

    def assemble(self):
        return OpcodeFormat1Imm1Reg(self.mnemonic, self.value, self.dest.assemble(), lineno=self.lineno)

# loads from constant pool
class insLoadConst(BaseInstruction):
    mnemonic = 'LDC'

    def __init__(self, dest, value, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest

        if isinstance(value, float):
            value = int(value * 65536)
        else:
            value = value

        self.value = value

        self.src = None

    def __str__(self):
        return "%s %s <- CONSTANTS[%s](%s)" % (self.mnemonic, self.dest, self.src, self.value)

    def execute(self, vm):
        vm.registers[self.dest.reg] = self.value

    def assemble(self):
        assert self.src is not None
        return OpcodeFormat1Imm1Reg(self.mnemonic, self.src, self.dest.assemble(), lineno=self.lineno)

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
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

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
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

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
        if self.src not in vm.objects:
            raise CompilerFatal(f'Load Ref does not seem to point to an object: {self.src}')

        vm.registers[self.dest.reg] = self.src.addr.addr

    def assemble(self):
        return OpcodeFormat1Imm1Reg(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

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
        return OpcodeFormat1Imm1Reg(self.mnemonic, self.src.assemble(), self.dest.assemble(), lineno=self.lineno)

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
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

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
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

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
        return OpcodeFormat1Imm1Reg(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

class insLoadString(BaseInstruction):
    mnemonic = 'LDSTR'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

        assert self.src is not None

    def __str__(self):
        return "%s %s <-S %s" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        dest = vm.registers[self.dest.reg]
        src = vm.registers[self.src.reg]

        dest_global = self.dest.var.ref.is_global
        src_global = self.src.var.ref.is_global

        if src_global:
            src_str = vm.memory[src]

        else:
            src_str = vm.locals[src]

        if dest_global:
            vm.memory[dest] = src_str

        else:
            vm.locals[dest] = src_str    

    def assemble(self):
        dest_global = self.dest.var.ref.is_global
        src_global = self.src.var.ref.is_global

        flags = 0

        if dest_global:
            flags |= 0x01

        if src_global:
            flags |= 0x02

        return OpcodeFormat3AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), flags, lineno=self.lineno)

class insLookup(BaseInstruction):
    mnemonic = 'LKP'
    
    def __init__(self, result, base_addr, indexes, counts, strides, **kwargs):
        super().__init__(**kwargs)
        self.result = result
        self.base_addr = base_addr
        self.indexes = indexes
        self.counts = counts
        self.strides = strides

        self.len = len(indexes)
        assert len(counts) == self.len
        assert len(strides) == self.len

        assert base_addr is not None

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)
        return "%s %s <- %s %s" % (self.mnemonic, self.result, self.base_addr, indexes)

    def execute(self, vm):
        addr = self.base_addr.addr

        for i in range(len(self.indexes)):
            index = vm.registers[self.indexes[i].reg]
            
            count = vm.registers[self.counts[i].reg]
            stride = vm.registers[self.strides[i].reg]

            if count > 0:
                index %= count
                index *= stride

            addr += index

        vm.registers[self.result.reg] = addr

    def assemble(self):
        raise NotImplementedError

class insLookup0(insLookup):
    mnemonic = 'LKP0'

    def assemble(self):
        indexes = [i.assemble() for i in self.indexes]
        counts = [i.assemble() for i in self.counts]
        strides = [i.assemble() for i in self.strides]

        return OpcodeFormatLookup0(self.mnemonic, self.base_addr.assemble(), lineno=self.lineno)

class insLookup1(insLookup):
    mnemonic = 'LKP1'

    def assemble(self):
        indexes = [i.assemble() for i in self.indexes]
        counts = [i.assemble() for i in self.counts]
        strides = [i.assemble() for i in self.strides]

        return OpcodeFormatLookup1(
                self.mnemonic, 
                self.base_addr.assemble(), 
                indexes, 
                counts, 
                strides, 
                lineno=self.lineno)

class insLookup2(insLookup):
    mnemonic = 'LKP2'

    def assemble(self):
        indexes = [i.assemble() for i in self.indexes]
        counts = [i.assemble() for i in self.counts]
        strides = [i.assemble() for i in self.strides]

        return OpcodeFormatLookup1(
                self.mnemonic, 
                self.base_addr.assemble(), 
                indexes, 
                counts, 
                strides, 
                lineno=self.lineno)

class insLookup3(insLookup):
    mnemonic = 'LKP3'

    def assemble(self):
        indexes = [i.assemble() for i in self.indexes]
        counts = [i.assemble() for i in self.counts]
        strides = [i.assemble() for i in self.strides]

        return OpcodeFormatLookup1(
                self.mnemonic, 
                self.base_addr.assemble(), 
                indexes, 
                counts, 
                strides, 
                lineno=self.lineno)
    
# class insLookupGlobal(BaseInstruction):
#     mnemonic = 'LKPG'
    
#     def __init__(self, result, base_addr, indexes, counts, strides, **kwargs):
#         super().__init__(**kwargs)
#         self.result = result
#         self.base_addr = base_addr
#         self.indexes = indexes
#         self.counts = counts
#         self.strides = strides

#         assert base_addr is not None

#     def __str__(self):
#         indexes = ''
#         for index in self.indexes:
#             indexes += '[%s]' % (index)
#         return "%s %s <- 0x%s %s" % (self.mnemonic, self.result, hex(self.base_addr.addr), indexes)

#     def execute(self, vm):
#         addr = self.base_addr.addr

#         for i in range(len(self.indexes)):
#             index = vm.registers[self.indexes[i].reg]
            
#             count = self.counts[i]
#             stride = self.strides[i]            

#             if count > 0:
#                 index %= count
#                 index *= stride

#             addr += index

#         vm.registers[self.result.reg] = addr

# class insLookupLocal(BaseInstruction):
#     mnemonic = 'LKPL'
    
#     def __init__(self, result, base_addr, indexes, counts, strides, **kwargs):
#         super().__init__(**kwargs)
#         self.result = result
#         self.base_addr = base_addr
#         self.indexes = indexes
#         self.counts = counts
#         self.strides = strides

#         assert base_addr is not None

#     def __str__(self):
#         indexes = ''
#         for index in self.indexes:
#             indexes += '[%s]' % (index)
#         return "%s %s <- 0x%s %s" % (self.mnemonic, self.result, hex(self.base_addr.addr), indexes)

#     def execute(self, vm):
#         addr = self.base_addr.addr

#         for i in range(len(self.indexes)):
#             index = vm.registers[self.indexes[i].reg]
            
#             count = self.counts[i]
#             stride = self.strides[i]            

#             if count > 0:
#                 index %= count
#                 index *= stride

#             addr += index

#         vm.registers[self.result.reg] = addr

class insLabel(BaseInstruction):
    def __init__(self, name=None, **kwargs):
        super().__init__(**kwargs)
        self.name = name

    def __str__(self):
        return "Label(%s)" % (self.name)

    def execute(self, vm):
        pass

    def assemble(self):
        return OpcodeLabel(self, lineno=self.lineno)


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
        return OpcodeFormat1Imm(self.mnemonic, self.label.assemble(), lineno=self.lineno)
        
class insJmpConditional(BaseJmp):
    def __init__(self, op1, label, **kwargs):
        super().__init__(label, **kwargs)

        self.op1 = op1

    def __str__(self):
        return "%s, %s -> %s" % (self.mnemonic, self.op1, self.label)

    def assemble(self):
        return OpcodeFormat1Imm1Reg(self.mnemonic, self.label.assemble(), self.op1.assemble(), lineno=self.lineno)

class insJmpIfZero(insJmpConditional):
    mnemonic = 'JMPZ'

    def execute(self, vm):
        if vm.registers[self.op1.reg] == 0:
            return self.label

class insLoop(BaseJmp):
    mnemonic = 'LOOP'

    def __init__(self, iterator_in, iterator_out, stop, label, **kwargs):
        super().__init__(label, **kwargs)

        self.iterator_in = iterator_in
        self.iterator_out = iterator_out
        self.stop = stop

    def __str__(self):
        return "%s, %s <- ++%s < %s -> %s" % (self.mnemonic, self.iterator_out, self.iterator_in, self.stop, self.label)

    def execute(self, vm):
        # increment op1
        vm.registers[self.iterator_out.reg] = vm.registers[self.iterator_in.reg] + 1

        # compare to op2
        if vm.registers[self.iterator_out.reg] < vm.registers[self.stop.reg]:
            # return jump target
            return self.label

    def assemble(self):
        return OpcodeFormat1Imm3Reg(self.mnemonic, self.label.assemble(), self.iterator_in.assemble(), self.iterator_out.assemble(), self.stop.assemble(), lineno=self.lineno)

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
        return OpcodeFormat1AC(self.mnemonic, self.op1.assemble(), lineno=self.lineno)

class insBinop(BaseInstruction):
    mnemonic = 'BINOP'
    symbol = "??"

    def __init__(self, result, op1, op2, **kwargs):
        super().__init__(**kwargs)
        self.result = result
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%-s %s <- %s %4s %s" % (self.mnemonic, self.result, self.op1, self.symbol, self.op2)

    def assemble(self):
        return OpcodeFormat3AC(self.mnemonic, self.result.assemble(), self.op1.assemble(), self.op2.assemble(), lineno=self.lineno)

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


class insF16Mul(insArith):
    mnemonic = 'MUL_F16'
    symbol = "*"

    def arith(self, op1, op2):
        return (op1 * op2) // 65536

class insF16Div(insArith):
    mnemonic = 'DIV_F16'
    symbol = "/"

    def arith(self, op1, op2):
        if op2 == 0:
            return 0

        else:
            return (op1 * 65536) // op2

class insVector(BaseInstruction):
    mnemonic = 'VECTOR'

    def __init__(self, target, value, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.value = value

        if not self.value.var.is_scalar:
            raise SyntaxError(f'Vector operations must use a scalar operand', lineno=self.lineno)

        self.is_global = self.target.var.ref.is_global

        self.length = self.target.var.ref.size

    def __str__(self):
        return "%s *%s %s= %s" % (self.mnemonic, self.target, self.symbol, self.value)

    def assemble(self):
        return OpcodeFormatVector(self.mnemonic, self.target.assemble(), self.is_global, self.value.assemble(), self.length, lineno=self.lineno)

class insVectorMov(insVector):
    mnemonic = 'VMOV'
    op = "mov"
    symbol = "="

    def execute(self, vm):
        value = vm.registers[self.value.reg]
        addr = vm.registers[self.target.reg]

        if self.is_global:
            array = vm.memory

        else:
            array = vm.locals

        for i in range(self.length):    
            array[addr] = value
        
            addr += 1

class insVectorAdd(insVector):
    mnemonic = 'VADD'
    op = "add"
    symbol = "+"

    def execute(self, vm):
        value = vm.registers[self.value.reg]
        addr = vm.registers[self.target.reg]

        if self.is_global:
            array = vm.memory

        else:
            array = vm.locals

        for i in range(self.length):
            array[addr] += value

            # coerce to int
            array[addr] = int(array[addr])
        
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
        return OpcodeFormat1AC(self.mnemonic, self.op1.assemble(), lineno=self.lineno)

class insPrint(BaseInstruction):
    mnemonic = 'PRINT'

    def __init__(self, op1, **kwargs):
        super().__init__(**kwargs)
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def execute(self, vm):
        value = vm.registers[self.op1.reg]

        print(value)
            
    def assemble(self):
        return OpcodeFormat1AC(self.mnemonic, self.op1.assemble(), lineno=self.lineno)

class insLoadRetVal(BaseInstruction):
    mnemonic = 'LOAD_RET_VAL'

    def __init__(self, target, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        
    def __str__(self):
        return "%s %s" % (self.mnemonic, self.target)

    def execute(self, vm):
        vm.registers[self.target.reg] = vm.ret_val

    def assemble(self):
        return OpcodeFormat1AC(self.mnemonic, self.target.assemble(), lineno=self.lineno)

class insCall(BaseInstruction):
    mnemonic = 'CALL'

    def __init__(self, target, params=[], **kwargs):
        super().__init__(**kwargs)
        self.target = OpcodeFunc(target, lineno=self.lineno)
        self.params = params

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s (%s)" % (self.mnemonic, self.target.func, params)

    def execute(self, vm):
        target = None
        for f in vm.funcs:
            if f.name == self.target.func:
                target = f
                break

        ret_val = target.run(*[vm.registers[p.reg] for p in self.params])

        vm.ret_val = ret_val

    def assemble(self):
        raise NotImplementedError

class insCall0(insCall):
    mnemonic = 'CALL0'

    def assemble(self):
        return OpcodeFormat1Imm(
            self.mnemonic,
            self.target, 
            lineno=self.lineno)

class insCall1(insCall):
    mnemonic = 'CALL1'

    def assemble(self):
        return OpcodeFormat1Imm1Reg(
            self.mnemonic,
            self.target,
            self.params[0].assemble(), 
            lineno=self.lineno)

class insCall2(insCall):
    mnemonic = 'CALL2'

    def assemble(self):
        return OpcodeFormat1Imm2Reg(
            self.mnemonic,
            self.target,
            self.params[0].assemble(), 
            self.params[1].assemble(), 
            lineno=self.lineno)

class insCall3(insCall):
    mnemonic = 'CALL3'

    def assemble(self):
        return OpcodeFormat1Imm3Reg(
            self.mnemonic,
            self.target,
            self.params[0].assemble(), 
            self.params[1].assemble(), 
            self.params[2].assemble(), 
            lineno=self.lineno)

class insCall4(insCall):
    mnemonic = 'CALL4'

    def assemble(self):
        return OpcodeFormat1Imm4Reg(
            self.mnemonic,
            self.target,
            self.params[0].assemble(), 
            self.params[1].assemble(), 
            self.params[2].assemble(), 
            self.params[3].assemble(), 
            lineno=self.lineno)

class insIndirectCall(BaseInstruction):
    mnemonic = 'ICALL'

    def __init__(self, ref, params=[], **kwargs):
        super().__init__(**kwargs)
        self.ref = ref
        self.params = params

    def __str__(self):
        params = ''
        for param in self.params:
            params += '%s, ' % (param)
        params = params[:len(params) - 2]

        return "%s %s (%s)" % (self.mnemonic, self.ref, params)

    def execute(self, vm):
        func = vm.registers[self.ref.reg]

        target = vm.funcs[func]

        vm.ret_val = target.run(*[vm.registers[p.reg] for p in self.params])

    def assemble(self):
        raise NotImplementedError

class insIndirectCall0(insIndirectCall):
    mnemonic = 'ICALL0'

    def assemble(self):
        return OpcodeFormat1AC(
            self.mnemonic, 
            self.ref.assemble(), 
            lineno=self.lineno)

class insIndirectCall1(insIndirectCall):
    mnemonic = 'ICALL1'

    def assemble(self):
        return OpcodeFormat2AC(
            self.mnemonic,
            self.ref.assemble(), 
            self.params[0].assemble(), 
            lineno=self.lineno)

class insIndirectCall2(insIndirectCall):
    mnemonic = 'ICALL2'

    def assemble(self):
        return OpcodeFormat3AC(
            self.mnemonic,
            self.ref.assemble(), 
            self.params[0].assemble(), 
            self.params[1].assemble(), 
            lineno=self.lineno)

class insIndirectCall3(insIndirectCall):
    mnemonic = 'ICALL3'

    def assemble(self):
        return OpcodeFormat4AC(
            self.mnemonic,
            self.ref.assemble(), 
            self.params[0].assemble(), 
            self.params[1].assemble(), 
            self.params[2].assemble(), 
            lineno=self.lineno)

class insIndirectCall4(insIndirectCall):
    mnemonic = 'ICALL4'

    def assemble(self):
        return OpcodeFormat5AC(
            self.mnemonic,
            self.ref.assemble(), 
            self.params[0].assemble(), 
            self.params[1].assemble(), 
            self.params[2].assemble(), 
            self.params[3].assemble(), 
            lineno=self.lineno)


class insPixelLookup(BaseInstruction):
    mnemonic = 'PLOOKUP'

    def __init__(self, result, pixel_ref, indexes, **kwargs):
        super().__init__(**kwargs)
            
        self.result = result
        self.pixel_ref = pixel_ref
        self.indexes = indexes

        if len(self.indexes) > 2:
            raise CompilerFatal(f'Too many indexes for a pixel array!')

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)

        return "%s %s = %s%s" % (self.mnemonic, self.result, self.pixel_ref, indexes)

    def execute(self, vm):
        indexes = []
        for i in range(len(self.indexes)):
            indexes.append(vm.registers[self.indexes[i].reg])

        ref = vm.registers[self.pixel_ref.reg]

        index = vm.calc_index(indexes, ref)

        vm.registers[self.result.reg] = index

    def assemble(self):
        raise NotImplementedError

class insPixelLookup1(insPixelLookup):
    mnemonic = 'PLOOKUP1'

    def assemble(self):
        return OpcodeFormat3AC(self.mnemonic, self.pixel_ref.reg, self.result.assemble(), self.indexes[0].assemble(), lineno=self.lineno)

class insPixelLookup2(insPixelLookup):
    mnemonic = 'PLOOKUP2'

    def assemble(self):
        return OpcodeFormat4AC(self.mnemonic, self.pixel_ref.reg, self.result.assemble(), self.indexes[0].assemble(), self.indexes[1].assemble(), lineno=self.lineno)


class insPixelStore(BaseInstruction):
    mnemonic = 'PSTORE'

    def __init__(self, pixel_ref, attr, value, **kwargs):
        super().__init__(**kwargs)
        
        self.pixel_ref = pixel_ref
        self.attr = attr
        self.value = value

    def __str__(self):
        return "%s %s.%s = %s" % (self.mnemonic, self.pixel_ref, self.attr, self.value)

    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]
        value = vm.registers[self.value.reg]

        assert self.attr in vm.gfx_data

        array = vm.gfx_data[self.attr]

        # clamp values:
        if value < 0:
            value = 0

        # note this also allows using the value 1.0 (which maps to 65536) as the maximum value 65535
        elif value > 65535:
            value = 65535

        assert isinstance(ref, int)

        # if we got an index, this is an indexed access
        array[ref] = value

    def assemble(self):
        # we don't encode attribute, the opcode itself will encode that
        return OpcodeFormat2AC(self.mnemonic, self.pixel_ref.reg, self.value.assemble(), lineno=self.lineno)

class insPixelStoreHue(insPixelStore):
    mnemonic = 'PSTORE_HUE'

    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]
        value = vm.registers[self.value.reg]

        assert self.attr in vm.gfx_data

        array = vm.gfx_data[self.attr]

        assert isinstance(ref, int)

        # if we got an index, this is an indexed access
        array[ref] = value

        # hue will wrap around
        array[ref] %= 65536        

class insPixelStoreVal(insPixelStore):
    mnemonic = 'PSTORE_VAL'

class insPixelStoreSat(insPixelStore):
    mnemonic = 'PSTORE_SAT'

class insPixelStoreHSFade(insPixelStore):
    mnemonic = 'PSTORE_HS_FADE'

class insPixelStoreVFade(insPixelStore):
    mnemonic = 'PSTORE_V_FADE'

class insVPixelStore(BaseInstruction):
    mnemonic = 'VSTORE'

    def __init__(self, pixel_ref, attr, value, **kwargs):
        super().__init__(**kwargs)
        
        self.pixel_ref = pixel_ref
        self.attr = attr
        self.value = value

    def __str__(self):
        return "%s %s.%s = %s" % (self.mnemonic, self.pixel_ref, self.attr, self.value)

    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]
        value = vm.registers[self.value.reg]

        pixel_array = vm.get_pixel_array(ref)

        # clamp values:
        if value < 0:
            value = 0

        # note this also allows using the value 1.0 (which maps to 65536) as the maximum value 65535
        elif value > 65535:
            value = 65535

        assert self.attr in vm.gfx_data
        array = vm.gfx_data[self.attr]

        # array reference, this is an array set
        for i in range(pixel_array['count']):
            idx = vm.calc_index(indexes=[i], pixel_array=pixel_array)
            array[idx] = value

    def assemble(self):
        # we don't encode attribute, the opcode itself will encode that
        return OpcodeFormat2AC(self.mnemonic, self.pixel_ref.reg, self.value.assemble(), lineno=self.lineno)


class insVPixelStoreHue(insVPixelStore):
    mnemonic = 'VSTORE_HUE'

    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]
        value = vm.registers[self.value.reg]

        pixel_array = vm.get_pixel_array(ref)

        # hue will wrap around
        value %= 65536

        assert self.attr in vm.gfx_data
        array = vm.gfx_data[self.attr]

        # array reference, this is an array set
        for i in range(pixel_array['count']):
            idx = vm.calc_index(indexes=[i], pixel_array=pixel_array)
            array[idx] = value

class insVPixelStoreVal(insVPixelStore):
    mnemonic = 'VSTORE_VAL'

class insVPixelStoreSat(insVPixelStore):
    mnemonic = 'VSTORE_SAT'

class insVPixelStoreHSFade(insVPixelStore):
    mnemonic = 'VSTORE_HS_FADE'

class insVPixelStoreVFade(insVPixelStore):
    mnemonic = 'VSTORE_V_FADE'
    
class insPixelLoad(BaseInstruction):
    mnemonic = 'PLOAD'

    def __init__(self, target, pixel_ref, attr, **kwargs):
        super().__init__(**kwargs)
        self.pixel_ref = pixel_ref
        self.attr = attr
        self.target = target

    def __str__(self):
        return "%s %s = %s.%s" % (self.mnemonic, self.target, self.pixel_ref, self.attr)

    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]

        if self.attr in vm.gfx_data:
            array = vm.gfx_data[self.attr]

            if isinstance(ref, int):
                # if we got an index, this is an indexed access
                value = array[ref]

            else:
                # array reference, this is an array get
                
                # not quite sure how to handle this, or if we should?
                # could be useful for some things, like, check if entire
                # array is off (all val == 0), or something like that.
                raise CompilerFatal(f'Load from entire array!')

            vm.registers[self.target.reg] = value

        
        else:
            # pixel attributes not settable in code for now
            assert False

    def assemble(self):
        # we don't encode attribute, the opcode itself will encode that
        return OpcodeFormat2AC(self.mnemonic, self.pixel_ref.reg, self.target.assemble(), lineno=self.lineno)


class insPixelLoadHue(insPixelLoad):
    mnemonic = 'PLOAD_HUE'

class insPixelLoadVal(insPixelLoad):
    mnemonic = 'PLOAD_VAL'

class insPixelLoadSat(insPixelLoad):
    mnemonic = 'PLOAD_SAT'

class insPixelLoadHSFade(insPixelLoad):
    mnemonic = 'PLOAD_HS_FADE'

class insPixelLoadVFade(insPixelLoad):
    mnemonic = 'PLOAD_V_FADE'

class insPixelLoadAttr(insPixelLoad):
    mnemonic = 'PLOAD_ATTR'
    
    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]

        pixel_array = vm.get_pixel_array(ref)

        vm.registers[self.target.reg] = pixel_array[self.attr]


class insPixelAdd(BaseInstruction):
    mnemonic = 'PADD'

    def __init__(self, pixel_ref, attr, value, **kwargs):
        super().__init__(**kwargs)
        
        self.pixel_ref = pixel_ref
        self.attr = attr
        self.value = value

    def __str__(self):
        return "%s %s.%s += %s" % (self.mnemonic, self.pixel_ref, self.attr, self.value)

    def execute(self, vm):
        ref = vm.registers[self.pixel_ref.reg]
        value = vm.registers[self.value.reg]

        if self.attr in vm.gfx_data:
            array = vm.gfx_data[self.attr]

            if isinstance(ref, int):
                # if we got an index, this is an indexed access
                array[ref] += value

            else:
                # array reference, this is an array set
                for i in range(len(array)):
                    array[i] += value

        else:
            # pixel attributes not settable in code for now
            assert False

    # def assemble(self):
    #     bc = [self.opcode]
    #     bc.append(insPixelArray(self.pixel_ref))

    #     index_x = self.indexes[0]
    #     bc.extend(index_x.assemble())
    #     index_y = self.indexes[1]
    #     bc.extend(index_y.assemble())

    #     bc.extend(self.value.assemble())

    #     return bc


class insPixelAddHue(insPixelAdd):
    mnemonic = 'PADD_HUE'





class insConvMov(insMov):
    mnemonic = 'MOV'

class insConvI32toF16(BaseInstruction):
    mnemonic = 'CONV_I32_TO_F16'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = F16(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.registers[self.dest.reg] = convert_to_f16(vm.registers[self.src.reg])

    def assemble(self):
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

    
class insConvF16toI32(BaseInstruction):
    mnemonic = 'CONV_F16_TO_I32'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = I32(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        vm.registers[self.dest.reg] = convert_to_i32(vm.registers[self.src.reg])

    def assemble(self):
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)


class insConvGFX16toF16(BaseInstruction):
    mnemonic = 'CONV_GFX16_TO_F16'

    def __init__(self, dest, src, **kwargs):
        super().__init__(**kwargs)
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s = F16(%s)" % (self.mnemonic, self.dest, self.src)

    def execute(self, vm):
        f16_value = vm.registers[self.src.reg]

        # when converting *gfx16* to f16, we map the integer representation of 65535 to 65536.
        # this is because 65535 is our maximum value and 1.0 technically maps to 0.0 in f16, but
        # generally when we use 1.0 what we mean is the maximum value, not the lowest.
        if f16_value == 65535:
            f16_value = 65536

        vm.registers[self.dest.reg] = f16_value

    def assemble(self):
        return OpcodeFormat2AC(self.mnemonic, self.dest.assemble(), self.src.assemble(), lineno=self.lineno)

