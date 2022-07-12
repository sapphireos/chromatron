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

VM_ISA_VERSION  = 13

FILE_MAGIC      = 0x20205846 # 'FX  '
PROGRAM_MAGIC   = 0x474f5250 # 'PROG'
FUNCTION_MAGIC  = 0x434e5546 # 'FUNC'
CODE_MAGIC      = 0x45444f43 # 'CODE'
# INIT_MAGIC      = 0x54494E49 # 'INIT'
POOL_MAGIC      = 0x4C4F4F50 # 'POOL'
KEYS_MAGIC      = 0x5359454B # 'KEYS'
META_MAGIC      = 0x4154454d # 'META'

VM_STRING_LEN = 32

DATA_LEN = 4



class ProgramHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="file_magic"),
                  Uint32Field(_name="prog_magic"),
                  Uint16Field(_name="isa_version"),
                  CatbusHash(_name="program_name_hash"),
                  Uint16Field(_name="code_len"),
                  Uint16Field(_name="func_len"),
                  Uint16Field(_name="padding"),
                  Uint16Field(_name="local_data_len"),
                  Uint16Field(_name="global_data_len"),
                  Uint16Field(_name="constant_len"),
                  Uint16Field(_name="read_keys_len"),
                  Uint16Field(_name="write_keys_len"),
                  Uint16Field(_name="publish_len"),
                  Uint16Field(_name="link_len"),
                  Uint16Field(_name="db_len"),
                  Uint16Field(_name="cron_len"),
                  Uint16Field(_name="pix_obj_len"),
                  Uint16Field(_name="init_start"),
                  Uint16Field(_name="loop_start")]

        super().__init__(_name="program_header", _fields=fields, **kwargs)

class FunctionInfo(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="addr"),
                  Uint16Field(_name="frame_size")]

        super().__init__(_name="function_info", _fields=fields, **kwargs)

class PixelArray(StructField):
    def __init__(self, **kwargs):
        fields = [Int32Field(_name="count"),
                  Int32Field(_name="index"),
                  Int32Field(_name="mirror"),
                  Int32Field(_name="offset"),
                  Int32Field(_name="palette"),
                  Int32Field(_name="reverse"),
                  Int32Field(_name="size_x"),
                  Int32Field(_name="size_y"),
                  Int32Field(_name="reserved0"),
                  Int32Field(_name="reserved1"),
                  Int32Field(_name="reserved2"),
                  Int32Field(_name="reserved3")]

        super().__init__(_name="pixel_array", _fields=fields, **kwargs)


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
    def __init__(self, program, func_bytecode={}):
        self.program = program
        self.funcs = program.funcs
        self.func_bytecode = func_bytecode
        self.constants = program.constants

        self.stream = None
        self.header = None

    def __str__(self):
        s = f'FX Image: {self.program.name}\n'

        for func, code in self.func_bytecode.items():
            s += f'\tFunction: {func}:\n'
            for c in code:
                s += f'\t\t{c}\n'

        return s

    def render(self, filename=None):
        function_addrs = {}
        function_indexes = {}
        function_table = []
        label_addrs = {}
        opcodes = []
        read_keys = []
        write_keys = []
        pixel_arrays = []
        links = []
        db_entries = {}
        cron_tab = {}
        constant_pool = self.constants

        if filename is None:
            filename = self.program.name.replace('.fx', '') + '.fxb'


        # set up label and func addresses
        func_opcodes = {}
        addr = 0
        for func_name, code in self.func_bytecode.items():
            function_addrs[func_name] = addr

            func_opcodes[func_name] = []

            for op in code:
                if isinstance(op, OpcodeLabel):
                    label_addrs[op.label.name] = addr

                else:
                    opcodes.append(op)
                    addr += op.length

                func_opcodes[func_name].append(op)

        # check if loop is not present
        if 'loop' not in function_addrs:
            function_addrs['loop'] = 65535

        # set up function table
        for func in self.funcs.values():
            info = FunctionInfo(addr=function_addrs[func.name], frame_size=func.local_memory_size * DATA_LEN)

            function_table.append(info)

            function_indexes[func.name] = len(function_table) - 1


        objects = {}

        # set label and func addresses in opcodes
        for op in opcodes:
            op.assign_addresses(label_addrs, function_indexes, objects)

        # for func_name, func_opcodes in func_opcodes.items():
        #     print(func_name)
        #     for op in func_opcodes:
        #         print(' ' * 4, op)

        bytecode = bytes()

        # render bytecode:
        for op in opcodes:
            bc = op.render()

            if len(bc) % 4 != 0:
                raise CompilerFatal('Bytecode is not 32 bit aligned!')

            bytecode += bc

        # set up pixel arrays
        for array in self.program.pixel_arrays:
            pix_array = PixelArray(**array.keywords)

            pixel_arrays.append(pix_array)

        ################################
        # Generate binary program image:
        ################################

        stream = bytes()
        meta_names = []

        code_len = len(bytecode)
        local_data_len = self.program.maximum_stack_depth
        local_data_len *= DATA_LEN

        global_data_len = self.program.global_memory_size
        global_data_len *= DATA_LEN

        assert code_len % 4 == 0
        assert local_data_len % 4 == 0
        assert global_data_len % 4 == 0

        # set up function table
        packed_function_table = bytes()
        for func in function_table:
            packed_function_table += func.pack()

        func_table_len = len(function_table) * DATA_LEN

        # set up constant pool and init data
        constant_len = len(constant_pool) * DATA_LEN

        # set up pixel arrays
        packed_pixel_arrays = bytes()
        for pix in pixel_arrays:
            packed_pixel_arrays += pix.pack()
        
        pix_obj_len = len(packed_pixel_arrays)

        # set up read keys
        packed_read_keys = bytes()
        for key in read_keys:
            packed_read_keys += struct.pack('<L', catbus_string_hash(key))

        # set up write keys
        packed_write_keys = bytes()
        for key in write_keys:
            packed_write_keys += struct.pack('<L', catbus_string_hash(key))

        # set up published registers
        published_var_count = 0
        packed_publish = bytes()
        for var in self.program.globals:
            if 'publish' in var.keywords and var.keywords['publish'] == True:
                published_var = VMPublishVar(
                                    hash=catbus_string_hash(var.name), 
                                    addr=var.addr.addr,
                                    type=get_type_id(var.data_type))
                packed_publish += published_var.pack()
                
                meta_names.append(var.name)
                published_var_count += 1

        # set up links
        packed_links = bytes()
        for link in links:
            source_hash = catbus_string_hash(link['source'])
            dest_hash = catbus_string_hash(link['dest'])
            query = [catbus_string_hash(a) for a in link['query']]

            meta_names.append(link['source'])
            meta_names.append(link['dest'])
            for q in link['query']:
                meta_names.append(q)

            packed_links += Link(mode=link['mode'],
                                 aggregation=link['aggregation'],
                                 rate=link['rate'],
                                 source_hash=source_hash, 
                                 dest_hash=dest_hash, 
                                 query=query).pack()

        # set up DB entries
        packed_db = bytes()
        for name, entry in list(db_entries.items()):
            packed_db += entry.pack()

            meta_names.append(name)
        
        # set up cron entries
        packed_cron = bytes()
        for func_name, entries in list(cron_tab.items()):
            
            for entry in entries:
                item = CronItem(
                        func=self.function_addrs[func_name],
                        second=entry['seconds'],
                        minute=entry['minutes'],
                        hour=entry['hours'],
                        day_of_month=entry['day_of_month'],
                        day_of_week=entry['day_of_week'],
                        month=entry['month'])

                packed_cron += item.pack()


        # build program header
        header = ProgramHeader(
                    file_magic=FILE_MAGIC,
                    prog_magic=PROGRAM_MAGIC,
                    isa_version=VM_ISA_VERSION,
                    program_name_hash=catbus_string_hash(self.program.name),
                    code_len=code_len,
                    func_len=func_table_len,
                    local_data_len=local_data_len,
                    global_data_len=global_data_len,
                    constant_len=constant_len,
                    pix_obj_len=pix_obj_len,
                    read_keys_len=len(packed_read_keys),
                    write_keys_len=len(packed_write_keys),
                    publish_len=len(packed_publish),
                    link_len=len(packed_links),
                    db_len=len(packed_db),
                    cron_len=len(packed_cron),
                    init_start=function_addrs['init'],
                    loop_start=function_addrs['loop'])

        stream += header.pack()
        stream += packed_function_table
        stream += packed_pixel_arrays
        stream += packed_read_keys  
        stream += packed_write_keys
        stream += packed_publish
        stream += packed_links
        stream += packed_db
        stream += packed_cron

        # add constant pool
        stream += struct.pack('<L', POOL_MAGIC)

        for const in constant_pool:
            try:
                if const > 2147483647:
                    stream += struct.pack('<L', const)

                else:
                    stream += struct.pack('<l', const)

            except struct.error:
                logging.error(f"Invalid item in constant pool: {const} type: {type(const)}")
                raise

        # ensure alignment is correct
        assert len(stream) % 4 == 0


        # add code stream
        stream += struct.pack('<L', CODE_MAGIC)

        for b in bytecode:
            stream += struct.pack('<B', b)

        # ensure alignment is correct
        assert len(stream) % 4 == 0
        

        # addr = 0
        # data_table = []
        # data_table = sorted(self.data_table, key=lambda a: a.addr)
        # data_table.extend(self.strings)

        # for var in data_table:
        #     # print var, addr
        #     if var.addr < addr:
        #         continue

        #     if addr != var.addr:
        #         raise CompilerFatal("Data address error: %d != %d" % (addr, var.addr))

        #     if isinstance(var, irStrLiteral):
        #         # pack string meta data
        #         # u16 addr in data table : u16 max length in characters
        #         stream += struct.pack('<HH', addr, var.strlen)
        #         stream += var.strdata.encode('utf-8')

        #         padding_len = (4 - (var.strlen % 4)) % 4
        #         # add padding if necessary to make sure data is 32 bit aligned
        #         stream += bytes([0] * padding_len)

        #         addr += var.size

        #     elif var.length == 1:
        #         try:
        #             default_value = var.default_value

        #             if isinstance(default_value, irStrLiteral):
        #                 default_value = 0

        #             stream += struct.pack('<l', default_value)
        #             addr += var.length

        #         except struct.error:
        #             print("*********************************")
        #             print("packing error: var: %s type: %s default: %s type: %s" % (var, var.type, default_value, type(default_value)))
        #             print("*********************************")

        #             raise

        #     else:
        #         try:
        #             for i in range(var.length):
        #                 try:
        #                     default_value = var.default_value[i]

        #                 except TypeError:
        #                     default_value = var.default_value

        #                 try:
        #                     stream += struct.pack('<l', default_value)

        #                 except struct.error:
        #                     for val in default_value:
        #                         if isinstance(val, float):
        #                             stream += struct.pack('<l', int(val * 65536))
        #                         else:
        #                             stream += struct.pack('<l', val)

        #         except struct.error:
        #             print("*********************************")
        #             print("packing error: var: %s type: %s default: %s type: %s index: %d" % (var, var.type, default_value, type(default_value), i))

        #             raise

        #         addr += var.length

        # # ensure our address counter lines up with the data length
        # try:
        #     assert addr * 4 == data_len

        # except AssertionError:
        #     print("Bad data length. Last addr: %d data len: %d" % (addr * 4, data_len))
        #     raise

        # create hash of stream
        stream_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', stream_hash)
        self.stream_hash = stream_hash

        # but wait, that's not all!
        # now we're going attach meta data.
        # note all strings will be padded to the VM_STRING_LEN.
        prog_len = len(stream)
        
        self.prog_len = prog_len

        meta_data = bytes()

        # marker for meta
        meta_data += struct.pack('<L', META_MAGIC)

        # first string is script name
        # note the conversion with str(), this is because from a CLI
        # we might end up with unicode, when we really, really need ASCII.
        padded_string = self.program.name.encode('utf-8')
        # pad to string len
        padded_string += bytes([0] * (VM_STRING_LEN - len(padded_string)))

        meta_data += padded_string

        # next, attach names of stuff
        for s in meta_names:
            padded_string = s.encode('utf-8')

            if len(padded_string) > VM_STRING_LEN:
                raise SyntaxError("%s exceeds %d characters" % (padded_string, VM_STRING_LEN))

            padded_string += bytes([0] * (VM_STRING_LEN - len(padded_string)))
            meta_data += padded_string


        # add meta data to end of stream
        stream += meta_data
        
        # attach hash of entire file
        file_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', file_hash)


        if filename:
            # write to file
            with open(filename, 'wb') as f:
                f.write(stream)

        self.stream = stream
        self.header = header

        if filename:
            with open(filename, 'wb') as f:
                f.write(stream)

        return stream
