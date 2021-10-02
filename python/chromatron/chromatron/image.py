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
DATA_MAGIC      = 0x41544144 # 'DATA'
KEYS_MAGIC      = 0x5359454B # 'KEYS'
META_MAGIC      = 0x4154454d # 'META'

VM_STRING_LEN = 32



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
    def __init__(self, program, funcs={}, constants=[]):
        self.program = program
        self.funcs = funcs
        self.constants = constants

        self.stream = None
        self.header = None

    def __str__(self):
        s = f'FX Image: {self.program.name}\n'

        for func, code in self.funcs.items():
            s += f'\tFunction: {func}:\n'
            for c in code:
                s += f'\t\t{c}\n'

        return s

    def render(self, filename='meow.fxb'):
        function_addrs = {}
        label_addrs = {}
        opcodes = []
        read_keys = []
        write_keys = []
        pixel_arrays = {}
        links = []
        db_entries = {}
        cron_tab = {}

        # set up label and func addresses

        addr = 0
        for func, code in self.funcs.items():
            function_addrs[func] = addr

            for op in code:
                if isinstance(op, OpcodeLabel):
                    label_addrs[op.label.name] = addr

                else:
                    opcodes.append(op)
                    addr += op.length

        # set label and func addresses in opcodes
        for op in opcodes:
            op.assign_addresses(label_addrs, function_addrs)
            print(op)


        bytecode = bytes()

        # render bytecode:
        for op in opcodes:
            bc = op.render()

            if len(bc) % 4 != 0:
                raise CompilerFatal('Bytecode is not 32 bit aligned!')

            bytecode += bc

            # print(bc)


        ################################
        # Generate binary program image:
        ################################

        stream = bytes()
        meta_names = []

        code_len = len(bytecode)
        data_len = 0
        # data_len = self.data_count * DATA_LEN

        # set up padding so data start will be on 32 bit boundary
        # padding_len = 4 - (code_len % 4)
        # code_len += padding_len
        assert code_len % 4 == 0

        # set up pixel arrays
        pix_obj_len = 0
        for pix in list(pixel_arrays.values()):
            pix_obj_len += pix.length

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
        # for var in self.data_table:
        #     if var.publish:
        #         published_var = VMPublishVar(
        #                             hash=catbus_string_hash(var.name), 
        #                             addr=var.addr,
        #                             type=get_type_id(var.type))
        #         packed_publish += published_var.pack()
                
        #         meta_names.append(var.name)
        #         published_var_count += 1

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
                    data_len=data_len,
                    pix_obj_len=pix_obj_len,
                    read_keys_len=len(packed_read_keys),
                    write_keys_len=len(packed_write_keys),
                    publish_len=len(packed_publish),
                    link_len=len(packed_links),
                    db_len=len(packed_db),
                    cron_len=len(packed_cron))
                    # init_start=function_addrs['init'],
                    # loop_start=function_addrs['loop'])

        stream += header.pack()
        stream += packed_read_keys  
        stream += packed_write_keys
        stream += packed_publish
        stream += packed_links
        stream += packed_db
        stream += packed_cron

        # print header

        # add code stream
        stream += struct.pack('<L', CODE_MAGIC)

        for b in bytecode:
            stream += struct.pack('<B', b)

        # add padding if necessary to make sure data is 32 bit aligned
        # stream += bytes([0] * padding_len)

        # ensure padding is correct
        assert len(stream) % 4 == 0

        # add data table
        stream += struct.pack('<L', DATA_MAGIC)

        addr = 0
        data_table = []
        # data_table = sorted(self.data_table, key=lambda a: a.addr)
        # data_table.extend(self.strings)

        for var in data_table:
            # print var, addr
            if var.addr < addr:
                continue

            if addr != var.addr:
                raise CompilerFatal("Data address error: %d != %d" % (addr, var.addr))

            if isinstance(var, irStrLiteral):
                # pack string meta data
                # u16 addr in data table : u16 max length in characters
                stream += struct.pack('<HH', addr, var.strlen)
                stream += var.strdata.encode('utf-8')

                padding_len = (4 - (var.strlen % 4)) % 4
                # add padding if necessary to make sure data is 32 bit aligned
                stream += bytes([0] * padding_len)

                addr += var.size

            elif var.length == 1:
                try:
                    default_value = var.default_value

                    if isinstance(default_value, irStrLiteral):
                        default_value = 0

                    stream += struct.pack('<l', default_value)
                    addr += var.length

                except struct.error:
                    print("*********************************")
                    print("packing error: var: %s type: %s default: %s type: %s" % (var, var.type, default_value, type(default_value)))
                    print("*********************************")

                    raise

            else:
                try:
                    for i in range(var.length):
                        try:
                            default_value = var.default_value[i]

                        except TypeError:
                            default_value = var.default_value

                        try:
                            stream += struct.pack('<l', default_value)

                        except struct.error:
                            for val in default_value:
                                if isinstance(val, float):
                                    stream += struct.pack('<l', int(val * 65536))
                                else:
                                    stream += struct.pack('<l', val)

                except struct.error:
                    print("*********************************")
                    print("packing error: var: %s type: %s default: %s type: %s index: %d" % (var, var.type, default_value, type(default_value), i))

                    raise

                addr += var.length

        # ensure our address counter lines up with the data length
        try:
            assert addr * 4 == data_len

        except AssertionError:
            print("Bad data length. Last addr: %d data len: %d" % (addr * 4, data_len))
            raise

        # create hash of stream
        stream_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', stream_hash)
        self.stream_hash = stream_hash

        # but wait, that's not all!
        # now we're going attach meta data.
        # first, prepend 32 bit (signed) program length to stream.
        # this makes it trivial to read the VM image length from the file.
        # this also gives us the offset into the file where the meta data begins.
        # note all strings will be padded to the VM_STRING_LEN.
        prog_len = len(stream)
        stream = struct.pack('<l', prog_len) + stream

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
