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
    'NOT':                  0x02,
    'LDI':                  0x02,
    'LDC':                  0xBB,
    'LDG':                  0x03,
    'LDL':                  0x03,
    'REF':                  0x03,
    'LDGI':                 0x03,
    'STG':                  0x03,
    'STL':                  0x03,
    'STGI':                 0x03,
    'LKP':                  0x03,
    'JMP':                  0x03,
    'JMPZ':                 0xAA,
    'RET':                  0x05,

    'COMP_EQ':              0x04,
    'COMP_NEQ':             0x04,
    'COMP_GT':              0x04,
    'COMP_GTE':             0x04,
    'COMP_LT':              0x04,
    'COMP_LTE':             0x04,

    'AND':                  0x04,
    'OR':                   0x04,

    'ADD':                  0x04,
    'SUB':                  0x04,
    'SUB':                  0x04,
    'MUL':                  0x04,
    'DIV':                  0x04,
    'MOD':                  0x04,

    'MUL_F16':              0x04,
    'DIV_F16':              0x04,

    'CONV_I32_TO_F16':      0x04,
    'CONV_F16_TO_I32':      0x04,

    'HALT':                 0x04,
    'ASSERT':               0x04,
    'PRINT':                0x04,

    'CALL':                 0x04,
    'ICALL':                0x04,
    'LCALL':                0x04,

    'PLOOKUP':              0x04,
    'PSTORE_HUE':           0x04,
    'PSTORE_SAT':           0x04,
    'PSTORE_VAL':           0x04,
    'PSTORE_HS_FADE':       0x04,
    'PSTORE_V_FADE':        0x04,

    'PLOAD_HUE':            0x04,
    'PLOAD_SAT':            0x04,
    'PLOAD_VAL':            0x04,
    'PLOAD_HS_FADE':        0x04,
    'PLOAD_V_FADE':         0x04,
    
    'PADD_HUE':             0x04,
    'PADD_SAT':             0x04,
    'PADD_VAL':             0x04,
    'PADD_HS_FADE':         0x04,
    'PADD_V_FADE':          0x04,

    'PSUB_HUE':             0x04,
    'PSUB_SAT':             0x04,
    'PSUB_VAL':             0x04,
    'PSUB_HS_FADE':         0x04,
    'PSUB_V_FADE':          0x04,

    'PMUL_HUE':             0x04,
    'PMUL_SAT':             0x04,
    'PMUL_VAL':             0x04,
    'PMUL_HS_FADE':         0x04,
    'PMUL_V_FADE':          0x04,

    'PDIV_HUE':             0x04,
    'PDIV_SAT':             0x04,
    'PDIV_VAL':             0x04,
    'PDIV_HS_FADE':         0x04,
    'PDIV_V_FADE':          0x04,

    'PMOD_HUE':             0x04,
    'PMOD_SAT':             0x04,
    'PMOD_VAL':             0x04,
    'PMOD_HS_FADE':         0x04,
    'PMOD_V_FADE':          0x04,
    

    'VMOV':                 0x00,
    'VADD':                 0x00,
    'VSUB':                 0x00,
    'VMUL':                 0x00,
    'VDIV':                 0x00,
    'VMOD':                 0x00,


}

class Opcode(object):
    def __init__(self, opcode=None, lineno=None):
        assert lineno is not None
        self.lineno = lineno
        self.opcode = opcode
        self.format = None
        self.items = []

    def __str__(self):
        return f'Opcode: {self.opcode} Line: {self.lineno}'

    @property
    def length(self):
        raise NotImplementedError

    @property
    def byte_length(self):
        return 1 + struct.calcsize(self.format)

    def get_opcode(self):
        return opcodes[self.opcode]

    def assign_addresses(self, labels):
        for i in range(len(self.items)):
            item = self.items[i]

            if isinstance(item, OpcodeLabel):
                assert item.addr is None
                item.addr = labels[item.label.name]
                # replace label with actual address
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

class OpcodeFormat2Imm(Opcode32):
    def __init__(self, opcode, reg, value, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [reg, value]
        self.format = 'BH'

class OpcodeFormat3AC(Opcode32):
    def __init__(self, opcode, dest, op1, op2, **kwargs):
        super().__init__(opcode, **kwargs)

        self.items = [dest, op1, op2]
        self.format = 'BBB'

class OpcodeLabel(Opcode):
    def __init__(self, label, **kwargs):
        super().__init__(opcode='LABEL', **kwargs)
        self.label = label
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


        
