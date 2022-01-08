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
    'LDM':                  0x04,
    # 'LDG':                  0x04,
    # 'LDL':                  0x05,
    'REF':                  0x06,
    'LDGI':                 0x07,
    'STM':                  0x08,
    # 'STG':                  0x08,
    # 'STL':                  0x09,
    'STGI':                 0x0A,
    'LDSTR':                0x0B,
    
    'HALT':                 0x10,
    'ASSERT':               0x11,
    'PRINT':                0x12,

    'RET':                  0x13,
    'JMP':                  0x14,
    'JMPZ':                 0x15,
    'LOOP':                 0x16,
    'LOAD_RET_VAL':         0x17,
    
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
    'MUL':                  0x2B,
    'DIV':                  0x2C,
    'MOD':                  0x2D,

    'MUL_F16':              0x2E,
    'DIV_F16':              0x2F,

    'CONV_I32_TO_F16':      0x30,
    'CONV_F16_TO_I32':      0x31,
    'CONV_GFX16_TO_F16':    0x32,

    'PLOOKUP1':             0x38,
    'PLOOKUP2':             0x39,
    'PLOAD_ATTR':           0x3A,
    'PSTORE_ATTR':          0x3B,

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

    'PSTORE_HUE':           0x60,
    'PSTORE_SAT':           0x61,
    'PSTORE_VAL':           0x62,
    'PSTORE_HS_FADE':       0x63,
    'PSTORE_V_FADE':        0x64,
    'VSTORE_HUE':           0x68,
    'VSTORE_SAT':           0x69,
    'VSTORE_VAL':           0x6A,
    'VSTORE_HS_FADE':       0x6B,
    'VSTORE_V_FADE':        0x6C,

    'PLOAD_HUE':            0x70,
    'PLOAD_SAT':            0x71,
    'PLOAD_VAL':            0x72,
    'PLOAD_HS_FADE':        0x73,
    'PLOAD_V_FADE':         0x74,
    # 'VLOAD_HUE':            0x78,
    # 'VLOAD_SAT':            0x79,
    # 'VLOAD_VAL':            0x7A,
    # 'VLOAD_HS_FADE':        0x7B,
    # 'VLOAD_V_FADE':         0x7C,
    
    'PADD_HUE':             0x80,
    'PADD_SAT':             0x81,
    'PADD_VAL':             0x82,
    'PADD_HS_FADE':         0x83,
    'PADD_V_FADE':          0x84,
    'VADD_HUE':             0x88,
    'VADD_SAT':             0x89,
    'VADD_VAL':             0x8A,
    'VADD_HS_FADE':         0x8B,
    'VADD_V_FADE':          0x8C,
    
    'PSUB_HUE':             0x90,
    'PSUB_SAT':             0x91,
    'PSUB_VAL':             0x92,
    'PSUB_HS_FADE':         0x93,
    'PSUB_V_FADE':          0x94,
    'VSUB_HUE':             0x98,
    'VSUB_SAT':             0x99,
    'VSUB_VAL':             0x9A,
    'VSUB_HS_FADE':         0x9B,
    'VSUB_V_FADE':          0x9C,

    'PMUL_HUE':             0xA0,
    'PMUL_SAT':             0xA1,
    'PMUL_VAL':             0xA2,
    'PMUL_HS_FADE':         0xA3,
    'PMUL_V_FADE':          0xA4,
    'VMUL_HUE':             0xA8,
    'VMUL_SAT':             0xA9,
    'VMUL_VAL':             0xAA,
    'VMUL_HS_FADE':         0xAB,
    'VMUL_V_FADE':          0xAC,

    'PDIV_HUE':             0xB0,
    'PDIV_SAT':             0xB1,
    'PDIV_VAL':             0xB2,
    'PDIV_HS_FADE':         0xB3,
    'PDIV_V_FADE':          0xB4,
    'VDIV_HUE':             0xB8,
    'VDIV_SAT':             0xB9,
    'VDIV_VAL':             0xBA,
    'VDIV_HS_FADE':         0xBB,
    'VDIV_V_FADE':          0xBC,

    'PMOD_HUE':             0xC0,
    'PMOD_SAT':             0xC1,
    'PMOD_VAL':             0xC2,
    'PMOD_HS_FADE':         0xC3,
    'PMOD_V_FADE':          0xC4,
    'VMOD_HUE':             0xC8,
    'VMOD_SAT':             0xC9,
    'VMOD_VAL':             0xCA,
    'VMOD_HS_FADE':         0xCB,
    'VMOD_V_FADE':          0xCC,

    'VMOV':                 0xE0,
    'VADD':                 0xE1,
    'VSUB':                 0xE2,
    'VMUL':                 0xE3,
    'VDIV':                 0xE4,
    'VMOD':                 0xE5,
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

    def assign_addresses(self, labels, functions, objects):
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

            elif isinstance(item, OpcodeString):
                assert item.addr is None
                # item.addr = objects[item.obj]
                # replace func with actual address
                # self.items[i] = item.render()
                self.items[i] = 0

            elif isinstance(item, OpcodeObject):
                assert item.addr is None
                # item.addr = objects[item.obj]
                # replace func with actual address
                # self.items[i] = item.render()
                self.items[i] = 0

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

class OpcodePlaceholder(Opcode):
    pass

class OpcodeLabel(OpcodePlaceholder):
    def __init__(self, label, **kwargs):
        super().__init__(opcode='LABEL', **kwargs)
        self.label = label
        self.addr = None

    def __str__(self):
        return f'LABEL: {self.label.name:16} Line: {self.lineno:3}'

    def render(self):
        return self.addr

class OpcodeFunc(OpcodePlaceholder):
    def __init__(self, func, **kwargs):
        super().__init__(opcode='FUNC', **kwargs)
        self.func = func
        self.addr = None

    def render(self):
        return self.addr

class OpcodeString(OpcodePlaceholder):
    def __init__(self, s, **kwargs):
        super().__init__(opcode='STR', **kwargs)
        self.s = s
        self.addr = None

    def render(self):
        return self.addr        

class OpcodeObject(OpcodePlaceholder):
    def __init__(self, obj, **kwargs):
        super().__init__(opcode='OBJ', **kwargs)
        self.obj = obj
        self.addr = None

    def render(self):
        return self.addr

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

class OpcodeFormat1Imm2RegS(Opcode32):
    def __init__(self, opcode, imm1, reg1, reg2, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [imm1, reg1, reg2]
        self.format = 'BBB'

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

class OpcodeFormatVector(Opcode64):
    def __init__(self, opcode, target, value, length, is_global, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [target, is_global, value, length]
        self.format = 'BBHH'

class OpcodeFormatLookup0(Opcode):
    def __init__(self, opcode, dest, base_addr, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, base_addr]
        self.format = 'BH'
    
    @property
    def length(self):
        return 4

class OpcodeFormatLookup1(Opcode):
    def __init__(self, opcode, dest, base_addr, indexes=[], counts=[], strides=[], **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [
            dest,
            base_addr, 
            indexes[0], 
            counts[0], 
            strides[0],
        ]

        self.format = 'BHBBB'
    
    @property
    def length(self):
        return 8

class OpcodeFormatLookup2(Opcode):
    def __init__(self, opcode, dest, base_addr, indexes=[], counts=[], strides=[], **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [
            dest,
            base_addr, 
            indexes[0], 
            counts[0], 
            strides[0],
            indexes[1], 
            counts[1], 
            strides[1],
        ]

        self.format = 'BHBBBBBB'
    
    @property
    def length(self):
        return 12

class OpcodeFormatLookup3(Opcode):
    def __init__(self, opcode, dest, base_addr, indexes=[], counts=[], strides=[], **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [
            dest,
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

        self.format = 'BHBBBBBBBBB'
    
    @property
    def length(self):
        return 16


        
