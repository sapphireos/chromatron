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
from elysianfields import *
from .opcode import *

class ProgramHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="file_magic"),
                  Uint32Field(_name="prog_magic"),
                  Uint16Field(_name="isa_version"),
                  CatbusHash(_name="program_name_hash"),
                  Uint16Field(_name="code_len"),
                  Uint16Field(_name="data_len"),
                  Uint16Field(_name="read_keys_len"),
                  Uint16Field(_name="write_keys_len"),
                  Uint16Field(_name="publish_len"),
                  Uint16Field(_name="pix_obj_len"),
                  Uint16Field(_name="link_len"),
                  Uint16Field(_name="db_len"),
                  Uint16Field(_name="cron_len"),
                  Uint16Field(_name="init_start"),
                  Uint16Field(_name="loop_start")]

        super().__init__(_name="program_header", _fields=fields, **kwargs)

class VMPublishVar(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="hash"),
                  Uint16Field(_name="addr"),
                  Uint8Field(_name="type"),
                  ArrayField(_name="padding", _length=1, _field=Uint8Field)]

        super().__init__(_name="vm_publish", _fields=fields, **kwargs)

class Link(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="mode"),
                  Uint8Field(_name="aggregation"),
                  Uint16Field(_name="rate"),
                  CatbusHash(_name="source_hash"),
                  CatbusHash(_name="dest_hash"),
                  CatbusQuery(_name="query")]

        super().__init__(_name="link", _fields=fields, **kwargs)


class CronItem(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="func"),
                  Int8Field(_name="second"),
                  Int8Field(_name="minute"),
                  Int8Field(_name="hour"),
                  Int8Field(_name="day_of_month"),
                  Int8Field(_name="day_of_week"),
                  Int8Field(_name="month"),
                  ArrayField(_name="padding", _length=4, _field=Uint8Field)]

        super().__init__(_name="cron_item", _fields=fields, **kwargs)


class FXImage(object):
    def __init__(self, program, funcs={}):
        self.program = program
        self.funcs = funcs

    def __str__(self):
        s = f'FX Image: {self.program.name}\n'

        for func, code in self.funcs.items():
            s += f'\tFunction: {func}:\n'
            for c in code:
                s += f'\t\t{c}\n'

        return s

    def render(self, filename=None):
        function_addrs = {}
        label_addrs = {}
        opcodes = []

        for func, code in self.funcs.items():
            function_addrs[func] = len(opcodes)

            for op in code:
                if isinstance(op, OpcodeLabel):
                    label_addrs[op.label.name] = len(opcodes)

                else:
                    opcodes.append(op)



        bytecode = []

        for op in opcodes:
            bc = op.render()

            if len(bc) % 4 != 0:
                raise CompilerFatal('Bytecode is not 32 bit aligned!')

            bytecode.append(bc)

        # return opcodes       
        return bytecode

