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

import struct
from .exceptions import *


opcodes = {
    'MOV':                  0x01,
    'LDI':                  0x02,
    'LDC':                  0x03,
    'LDG':                  0x04,
    'LDL':                  0x05,
    'REF':                  0x06,
    'LDGI':                 0x07,
    'STG':                  0x08,
    'STL':                  0x09,
    'STGI':                 0x0A,
    
    'HALT':                 0x10,
    'ASSERT':               0x11,
    'PRINT':                0x12,

    'RET':                  0x13,
    'JMP':                  0x14,
    'JMPZ':                 0x15,
    'JMPZ_INC':             0x16,
    
    'COMP_EQ':              0x20,
    'COMP_NEQ':             0x21,
    'COMP_GT':              0x22,
    'COMP_GTE':             0x23,
    'COMP_LT':              0x24,
    'COMP_LTE':             0x25,

    'NOT':                  0x26,
    'AND':                  0x27,
    'OR':                   0x28,

    'ADD':                  0x29,
    'SUB':                  0x2A,
    'SUB':                  0x2B,
    'MUL':                  0x2C,
    'DIV':                  0x2D,
    'MOD':                  0x2E,

    'MUL_F16':              0x2F,
    'DIV_F16':              0x30,

    'CONV_I32_TO_F16':      0x31,
    'CONV_F16_TO_I32':      0x32,

    'LKP0':                 0x40,
    'LKP1':                 0x41,
    'LKP2':                 0x42,
    'LKP3':                 0x43,

    'CALL0':                0x48,
    'CALL1':                0x49,
    'CALL2':                0x4A,
    'CALL3':                0x4B,
    'CALL4':                0x4C,

    'ICALL0':               0x50,
    'ICALL1':               0x51,
    'ICALL2':               0x52,
    'ICALL3':               0x53,
    'ICALL4':               0x54,

    'LCALL0':               0x58,
    'LCALL1':               0x59,
    'LCALL2':               0x5A,
    'LCALL3':               0x5B,
    'LCALL4':               0x5C,

    'PLOOKUP1':             0x60,
    'PLOOKUP2':             0x61,

    'PSTORE_HUE':           0x68,
    'PSTORE_SAT':           0x69,
    'PSTORE_VAL':           0x6A,
    'PSTORE_HS_FADE':       0x6B,
    'PSTORE_V_FADE':        0x6C,

    'PLOAD_HUE':            0x70,
    'PLOAD_SAT':            0x71,
    'PLOAD_VAL':            0x72,
    'PLOAD_HS_FADE':        0x73,
    'PLOAD_V_FADE':         0x74,
    
    'PADD_HUE':             0x78,
    'PADD_SAT':             0x79,
    'PADD_VAL':             0x7A,
    'PADD_HS_FADE':         0x7B,
    'PADD_V_FADE':          0x7C,

    'PSUB_HUE':             0x80,
    'PSUB_SAT':             0x81,
    'PSUB_VAL':             0x82,
    'PSUB_HS_FADE':         0x83,
    'PSUB_V_FADE':          0x84,

    'PMUL_HUE':             0x88,
    'PMUL_SAT':             0x89,
    'PMUL_VAL':             0x8A,
    'PMUL_HS_FADE':         0x8B,
    'PMUL_V_FADE':          0x8C,

    'PDIV_HUE':             0x90,
    'PDIV_SAT':             0x91,
    'PDIV_VAL':             0x92,
    'PDIV_HS_FADE':         0x93,
    'PDIV_V_FADE':          0x94,

    'PMOD_HUE':             0x98,
    'PMOD_SAT':             0x99,
    'PMOD_VAL':             0x9A,
    'PMOD_HS_FADE':         0x9B,
    'PMOD_V_FADE':          0x9C,
    
    'VMOV':                 0xA0,
    'VADD':                 0xA1,
    'VSUB':                 0xA2,
    'VMUL':                 0xA3,
    'VDIV':                 0xA4,
    'VMOD':                 0xA5,
}

class Opcode(object):
    def __init__(self, opcode=None, lineno=None):
        assert lineno is not None
        self.lineno = lineno
        self.opcode = opcode
        self.format = None
        self.items = []

    def __str__(self):
        return f'Opcode: {self.opcode:16} Line: {self.lineno:3} Bytecode: {" ".join([format(a, "02x") for a in self.render()])}'

    @property
    def length(self):
        raise NotImplementedError

    @property
    def byte_length(self):
        return 1 + struct.calcsize(self.format)

    def get_opcode(self):
        return opcodes[self.opcode]

    def assign_addresses(self, labels, functions):
        for i in range(len(self.items)):
            item = self.items[i]

            if isinstance(item, OpcodeLabel):
                assert item.addr is None
                item.addr = labels[item.label.name]
                # replace label with actual address
                self.items[i] = item.render()

            elif isinstance(item, OpcodeFunc):
                assert item.addr is None
                item.addr = functions[item.func]
                # replace func with actual address
                self.items[i] = item.render()

    def render(self):
        if self.opcode not in opcodes:
            raise CompilerFatal(f'{self.opcode} not defined!')

        for item in self.items:
            if isinstance(item, OpcodeLabel) and item.addr is None:
                raise CompilerFatal(f'Label {item.label.name} in opcode {self} does not have an address!')

        packed = struct.pack(f'<B{self.format}', self.get_opcode(), *self.items)

        if len(packed) > self.length:
            raise CompilerFatal(f'{self} is improper length! {len(packed)} > {self.length}')

        elif len(packed) < self.length:
            # pad opcode to 32 bits:
            padding = self.length - len(packed)
            packed += struct.pack(f'<{padding}B', *[0xfe] * padding)

        return packed

class Opcode32(Opcode):
    @property
    def length(self):
        return 4

class Opcode64(Opcode):
    @property
    def length(self):
        return 8

    def get_opcode(self):
        opcode = super().get_opcode()

        # if opcode & 0x40 == 0:
            # raise CompilerFatal(f'Incorrect length bit on opcode {self.opcode}')

        return opcode

class OpcodeFormatNop(Opcode32):
    def __init__(self, opcode, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = []
        self.format = ''

class OpcodeFormat1AC(Opcode32):
    def __init__(self, opcode, op1, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [op1]
        self.format = 'B'

class OpcodeFormat2AC(Opcode32):
    def __init__(self, opcode, dest, op1, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, op1]
        self.format = 'BB'

class OpcodeFormat1Imm(Opcode32):
    def __init__(self, opcode, value, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [value]
        self.format = 'H'

# class OpcodeFormat2Imm(Opcode32):
#     def __init__(self, opcode, value1, value2, **kwargs):
#         super().__init__(opcode, **kwargs)

#         self.items = [value1, value2]
#         self.format = 'HH'

class OpcodeFormat3AC(Opcode32):
    def __init__(self, opcode, dest, op1, op2, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, op1, op2]
        self.format = 'BBB'

class OpcodeFormat4AC(Opcode64):
    def __init__(self, opcode, dest, op1, op2, op3, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, op1, op2, op3]
        self.format = 'BBBB'

class OpcodeFormat5AC(Opcode64):
    def __init__(self, opcode, dest, op1, op2, op3, op4, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, op1, op2, op3, op4]
        self.format = 'BBBBB'

class OpcodeFormat6AC(Opcode64):
    def __init__(self, opcode, dest, op1, op2, op3, op4, op5, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, op1, op2, op3, op4, op5]
        self.format = 'BBBBBB'

class OpcodeFormat1Imm1Reg(Opcode32):
    def __init__(self, opcode, imm1, reg1, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [imm1, reg1]
        self.format = 'HB'

class OpcodeFormat1Imm2Reg(Opcode64):
    def __init__(self, opcode, imm1, reg1, reg2, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [imm1, reg1, reg2]
        self.format = 'HBB'

class OpcodeFormat1Imm3Reg(Opcode64):
    def __init__(self, opcode, imm1, reg1, reg2, reg3, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [imm1, reg1, reg2, reg3]
        self.format = 'HBBB'

class OpcodeFormat1Imm4Reg(Opcode64):
    def __init__(self, opcode, imm1, reg1, reg2, reg3, reg4, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [imm1, reg1, reg2, reg3, reg4]
        self.format = 'HBBBB'

class OpcodeFormat1Imm5Reg(Opcode64):
    def __init__(self, opcode, imm1, reg1, reg2, reg3, reg4, reg5, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [imm1, reg1, reg2, reg3, reg4, reg5]
        self.format = 'HBBBBB'

class OpcodeLabel(Opcode):
    def __init__(self, label, **kwargs):
        super().__init__(opcode='LABEL', **kwargs)
        self.label = label
        self.addr = None

    def render(self):
        return self.addr

class OpcodeFunc(Opcode):
    def __init__(self, func, **kwargs):
        super().__init__(opcode='FUNC', **kwargs)
        self.func = func
        self.addr = None

    def render(self):
        return self.addr

class OpcodeFormatVector(Opcode64):
    def __init__(self, opcode, target, value, length, is_global, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [target, is_global, value, length]
        self.format = 'BBHH'

class OpcodeFormatLookup0(Opcode):
    def __init__(self, opcode, base_addr, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [base_addr]
        self.format = 'H'
    
    @property
    def length(self):
        return 4

class OpcodeFormatLookup1(Opcode):
    def __init__(self, opcode, base_addr, indexes=[], counts=[], strides=[], **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [
            base_addr, 
            indexes[0], 
            counts[0], 
            strides[0],
        ]

        self.format = 'HBBB'
    
    @property
    def length(self):
        return 8

class OpcodeFormatLookup2(Opcode):
    def __init__(self, opcode, base_addr, indexes=[], counts=[], strides=[], **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [
            base_addr, 
            indexes[0], 
            counts[0], 
            strides[0],
            indexes[1], 
            counts[1], 
            strides[1],
        ]

        self.format = 'HBBBBBB'
    
    @property
    def length(self):
        return 12

class OpcodeFormatLookup3(Opcode):
    def __init__(self, opcode, base_addr, indexes=[], counts=[], strides=[], **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [
            base_addr, 
            indexes[0], 
            counts[0], 
            strides[0],
            indexes[1], 
            counts[1], 
            strides[1],
            indexes[2], 
            counts[2], 
            strides[2],
        ]

        self.format = 'HBBBBBBBBB'
    
    @property
    def length(self):
        return 16


        
