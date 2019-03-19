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

from instructions import *

from elysianfields import *
from catbus import *
from copy import deepcopy, copy
import pprint

source_code = []

VM_ISA_VERSION  = 10

FILE_MAGIC      = 0x20205846 # 'FX  '
PROGRAM_MAGIC   = 0x474f5250 # 'PROG'
FUNCTION_MAGIC  = 0x434e5546 # 'FUNC'
CODE_MAGIC      = 0x45444f43 # 'CODE'
DATA_MAGIC      = 0x41544144 # 'DATA'
KEYS_MAGIC      = 0x5359454B # 'KEYS'
META_MAGIC      = 0x4154454d # 'META'

VM_STRING_LEN = 32
DATA_LEN = 4


ARRAY_FUNCS = ['len', 'min', 'max', 'avg', 'sum']

DAY_OF_WEEK = {'monday':    1,
               'tuesday':   2,
               'wednesday': 3,
               'thursday':  4,
               'friday':    5,
               'saturday':  6,
               'sunday':    7}


MONTHS =      {'january':   1,
               'february':  2,
               'march':     3,
               'april':     4,
               'may':       5,
               'june':      6,
               'july':      7,
               'august':    8,
               'september': 9,
               'october':   10,
               'november':  11,
               'december':  12}

COMPARE_BINOPS = ['eq', 'neq', 'gt', 'gte', 'lt', 'lte']

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

        super(ProgramHeader, self).__init__(_name="program_header", _fields=fields, **kwargs)

class VMPublishVar(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="hash"),
                  Uint16Field(_name="addr"),
                  Uint8Field(_name="type"),
                  ArrayField(_name="padding", _length=1, _field=Uint8Field)]

        super(VMPublishVar, self).__init__(_name="vm_publish", _fields=fields, **kwargs)

class Link(StructField):
    def __init__(self, **kwargs):
        fields = [BooleanField(_name="send"),
                  ArrayField(_name="padding", _length=3, _field=Uint8Field),
                  CatbusHash(_name="source_hash"),
                  CatbusHash(_name="dest_hash"),
                  CatbusQuery(_name="query")]

        super(Link, self).__init__(_name="link", _fields=fields, **kwargs)


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

        super(CronItem, self).__init__(_name="cron_item", _fields=fields, **kwargs)


class SyntaxError(Exception):
    def __init__(self, message='', lineno=None):
        self.lineno = lineno

        message += ' (line %d)' % (self.lineno)

        super(SyntaxError, self).__init__(message)

class VariableAlreadyDeclared(SyntaxError):
    pass

class VariableNotDeclared(SyntaxError):
    def __init__(self, var, message='', lineno=None):
        super(VariableNotDeclared, self).__init__(message, lineno)

        self.var = var

class VMRuntimeError(Exception):
    def __init__(self, message=''):
        super(VMRuntimeError, self).__init__(message)


def params_to_string(params):
    s = ''

    if len(params) == 0:
        return s

    for p in params:
        try:
            s += '%s %s,' % (p.type, p.name)

        except AttributeError:
            s += '%s' % (p.name)            

    # strip last comma
    if s[-1] == ',':
        s = s[:-1]

    return s

class IR(object):
    def __init__(self, lineno=None):
        self.lineno = lineno

        assert self.lineno != None

    def generate(self):
        return BaseInstruction()

    def get_input_vars(self):
        return []

    def get_output_vars(self):
        return []

    def get_jump_target(self):
        return None

class irVar(IR):
    def __init__(self, name, type='i32', options=None, **kwargs):
        super(irVar, self).__init__(**kwargs)
        self.name = name
        self.type = type
        self.type_str = type
        self.length = 1
        self.addr = None
        self.is_global = False
        self.is_const = False
        self.default_value = 0

        self.publish = False
        self.persist = False

        if options != None:
            if 'publish' in options and options['publish']:
                self.publish = True

            if 'persist' in options and options['persist']:
                self.persist = True

            if 'init_val' in options:
                self.default_value = options['init_val']

    def __str__(self):
        if self.is_global:
            options = ''
            if self.publish:
                options += 'publish '

            if self.persist:
                options += 'persist '

            return "Global(%s, %s %s)" % (self.name, self.type_str, options)
        else:
            return "Var(%s, %s)" % (self.name, self.type_str)

    def generate(self):
        assert self.addr != None
        return insAddr(self.addr, self)

    def lookup(self, indexes):
        assert len(indexes) == 0
        return copy(self)

    def get_base_type(self):
        return self.type

class irVar_i32(irVar):
    def __init__(self, *args, **kwargs):
        kwargs['type'] = 'i32'
        super(irVar_i32, self).__init__(*args, **kwargs)
        
class irVar_f16(irVar):
    def __init__(self, *args, **kwargs):
        kwargs['type'] = 'f16'
        super(irVar_f16, self).__init__(*args, **kwargs)
        
class irVar_gfx16(irVar):
    def __init__(self, *args, **kwargs):
        kwargs['type'] = 'gfx16'
        super(irVar_gfx16, self).__init__(*args, **kwargs)

class irVar_str(irVar):
    def __init__(self, *args, **kwargs):
        kwargs['type'] = 'str'
        
        super(irVar_str, self).__init__(*args, **kwargs)

    def __str__(self):
        if self.is_global:
            options = ''
            if self.publish:
                options += 'publish '

            if self.persist:
                options += 'persist '

            return "Str(%s, %s %s)" % (self.name, self.type_str, options)
        else:
            return "Str(%s, %s)" % (self.name, self.type_str)

class irAddress(irVar):
    def __init__(self, name, target=None, **kwargs):
        super(irAddress, self).__init__(name, **kwargs)
        self.target = target

    def __str__(self):
        return "Addr (%s -> %s)" % (self.name, self.target) 

    @property
    def type(self):
        return self.target.type

    @type.setter
    def type(self, value):
        pass
    
    def generate(self):
        assert self.addr != None
        return insAddr(self.addr, self.target)

    def get_base_type(self):
        return self.target.get_base_type()

class irConst(irVar):
    def __init__(self, *args, **kwargs):
        super(irConst, self).__init__(*args, **kwargs)

        if self.name == 'True':
            self.value = 1
        elif self.name == 'False':
            self.value = 0
        elif self.type == 'f16':
            self.value = int(float(self.name) * 65536)
        else:
            self.value = int(self.name)

        self.default_value = self.value

        self.is_const = True

    def __str__(self):
        return "Const(%s, %s)" % (self.name, self.type)

class irArray(irVar):
    def __init__(self, name, type, dimensions=[], **kwargs):
        super(irArray, self).__init__(name, type, **kwargs)

        self.count = dimensions.pop(0)

        if len(dimensions) == 0:
            self.type = type

        else:
            self.type = irArray(name, type, dimensions, lineno=self.lineno)

        self.length = self.count * self.type.length

    def __str__(self):
        return "Array (%s, %s, %d:%d)" % (self.name, self.type, self.count, self.length)

    def lookup(self, indexes):
        if len(indexes) == 0:
            return copy(self)

        indexes = deepcopy(indexes)
        indexes.pop(0)
        
        return self.type.lookup(indexes)

    def get_base_type(self):
        data_type = self.type

        while isinstance(data_type, irVar):
            data_type = data_type.type

        return data_type

class irRecord(irVar):
    def __init__(self, name, data_type, fields, offsets, **kwargs):
        super(irRecord, self).__init__(name, **kwargs)        

        self.fields = fields
        self.type = data_type

        self.offsets = offsets
        self.length = 0
        for field in self.fields.values():
            self.length += field.length


        self.count = 0
            
    def __call__(self, name, dimensions=[], options=None, lineno=None):
        return irRecord(name, self.type, deepcopy(self.fields), self.offsets, options=options, lineno=lineno)

    def __str__(self):
        return "Record(%s, %s, %d)" % (self.name, self.type, self.length)

    def get_field_from_offset(self, offset): 
        for field_name, addr in self.offsets.items():
            if addr.name == offset.name:
                return self.fields[field_name]

        assert False

    def lookup(self, indexes):
        if len(indexes) == 0:
            return copy(self)

        indexes = deepcopy(indexes)
        index = indexes.pop(0)

        try:
            return self.fields[index.name].lookup(indexes)

        except KeyError:
            # try looking up by offset
            for field_name, addr in self.offsets.items():
                if addr.name == index.name:
                    return self.fields[field_name].lookup(indexes)

            raise

class irStrLiteral(IR):
    def __init__(self, name, **kwargs):
        super(irStrLiteral, self).__init__(**kwargs)
        self.name = name        
        self.strlen = len(self.name)
        self.type = 'str'

        if self.strlen == 0:
            raise SyntaxError("String %s has 0 characters" % (args[0]))

        # check for empty string (reserving storage padded with nulls)
        # we will want a more descriptive name
        if self.name[0] == '\0':
            self.name = '**empty %d chars**' % (self.strlen)

        self.addr = None
        self.length = 1 # this is a reference to a string, so the length is 1

        self.size = ((self.strlen - 1) / 4) + 2 # space for characters + 32 bit length
        
    def __str__(self):
        return 'StrLiteral("%s")[%d]' % (self.name, self.strlen)

    def get_base_type(self):
        return self

    def generate(self):
        return insAddr(self.addr)

class irField(IR):
    def __init__(self, name, obj, **kwargs):
        super(irField, self).__init__(**kwargs)
        self.name = name
        self.obj = obj
        
    def __str__(self):
        return "Field(%s.%s)" % (self.obj.name, self.name)

    def generate(self):
        return insAddr(self.obj.offsets[self.name].addr)

class irObject(IR):
    def __init__(self, name, data_type, args=[], kw={}, **kwargs):
        super(irObject, self).__init__(**kwargs)
        self.name = name
        self.type = data_type
        self.args = args
        self.kw = kw

    def __str__(self):
        return "Object{%s(%s)}" % (self.name, self.type)

    def get_base_type(self):
        return self.type

class irPixelArray(irObject):
    def __init__(self, name, args=[], kw={}, **kwargs):
        super(irPixelArray, self).__init__(name, "PixelArray", args, kw, **kwargs)

        try:
            index = args[0].value
        except AttributeError:
            index = args[0]
        except IndexError:
            index = 0 

        try:
            count = args[1].value
        except AttributeError:
            count = args[1]
        except IndexError:
            count = 0 

        try:
            self.fields = {
                'size_x': 0,
                'size_y': 0,
                'index': index,
                'count': count,
                'reverse': 0,
            }

        except IndexError:
            raise SyntaxError("Missing arguments for PixelArray", lineno=self.lineno)            

        for k, v in kw.items():
            if k not in self.fields:
                raise SyntaxError("Invalid argument for PixelArray: %s" % (k), lineno=self.lineno)

            self.fields[k] = v.name

        self.length = len(self.fields) * DATA_LEN
        
    def __str__(self):
        return "PixelArray(%s)" % (self.name)

    def lookup(self, indexes):
        return self

class irObjectAttr(irAddress):
    def __init__(self, obj, attr, **kwargs):
        super(irObjectAttr, self).__init__(obj.name, target=attr, **kwargs)      

        self.obj = obj
        self.attr = attr.name
        self.type = obj.type

    def __str__(self):
        return "ObjAttr(%s.%s)" % (self.name, self.attr)



PIX_ATTR_TYPES = {
    'hue': 'gfx16',
    'sat': 'gfx16',
    'val': 'gfx16',
    'hs_fade': 'i32',
    'v_fade': 'i32',
    'count': 'i32',
    'size_x': 'i32',
    'size_y': 'i32',
    'index': 'i32',
    'is_hs_fading': 'i32',
    'is_v_fading': 'i32',
}


class irPixelAttr(irObjectAttr):
    def __init__(self, obj, attr, **kwargs):
        lineno = kwargs['lineno']

        if attr in ['hue', 'val', 'sat']:
            attr = irArray(attr, irVar_gfx16(attr, lineno=lineno), dimensions=[65535, 65535], lineno=lineno)

        elif attr in ['hs_fade', 'v_fade']:
            attr = irArray(attr, irVar_i32(attr, lineno=lineno), dimensions=[65535, 65535], lineno=lineno)

        elif attr in ['count', 'size_x', 'size_y', 'index', 'is_hs_fading', 'is_v_fading']:
            attr = irVar_i32(attr, lineno=lineno)

        else:
            raise SyntaxError("Unknown pixel array attribute: %s" % (attr), lineno=lineno)

        super(irPixelAttr, self).__init__(obj, attr, **kwargs)      

        self.indexes = []

    def __str__(self):
        return "PixelAttr(%s.%s)" % (self.name, self.attr)

    def generate(self):
        return self

class irDBAttr(irVar):
    def __init__(self, obj, attr, **kwargs):
        super(irDBAttr, self).__init__('%s.%s' % (obj, attr), **kwargs)

        self.type = 'db'
        self.attr = attr

    def __str__(self):
        return "DBAttr(%s)" % (self.name)

    def generate(self):
        return self.attr

    def lookup(self, indexes):
        return self

class irFunc(IR):
    def __init__(self, name, ret_type='i32', params=None, body=None, **kwargs):
        super(irFunc, self).__init__(**kwargs)
        self.name = name
        self.ret_type = ret_type
        self.params = params
        self.body = body

        if self.params == None:
            self.params = []

        if self.body == None:
            self.body = []

    def append(self, node):
        self.body.append(node)

    def insert(self, index, node):
        self.body.insert(index, node)

    def get(self, index):
        return self.body[index]

    def remove_dead_labels(self):
        labels = self.labels()

        keep = []

        # get list of labels that are used
        for label in labels:
            for node in self.body:
                target = node.get_jump_target()

                if target != None:
                    if target.name == label:
                        keep.append(label)
                        break

        # remove unused labels from instruction list
        self.body = [a for a in self.body \
                        if not isinstance(a, irLabel) or 
                        (a.name in keep)]

    def __str__(self):
        global source_code
        params = params_to_string(self.params)

        s = "\n######## Line %4d       ########\n" % (self.lineno)
        s += "Func %s(%s) -> %s\n" % (self.name, params, self.ret_type)
        s += "--------------------------------\n"

        labels = self.labels()

        current_line = -1
        for node in self.body:
            
            # interleave source code
            if node.lineno > current_line:
                current_line = node.lineno
                s += "%d\t%s\n" % (current_line, source_code[current_line - 1].strip())

            if isinstance(node, irLabel):
                s += '%s\n' % (node)

            else:    
                label = node.get_jump_target()

                if label != None:
                    s += '\t\t\t%s (Line %d)\n' % (node, label.lineno)

                else:
                    s += '\t\t\t%s\n' % (node)

        return s

    def labels(self):
        labels = {}

        for i in xrange(len(self.body)):
            ins = self.body[i]

            if isinstance(ins, irLabel):
                labels[ins.name] = i

        return labels

    def generate(self):
        params = [a.generate() for a in self.params]
        func = insFunction(self.name, params)
        ins = [func]
        for ir in self.body:
            code = ir.generate()

            try:
                ins.extend(code)

            except TypeError:
                ins.append(code)

        return ins

class irReturn(IR):
    def __init__(self, ret_var, **kwargs):
        super(irReturn, self).__init__(**kwargs)
        self.ret_var = ret_var

    def __str__(self):
        return "RET %s" % (self.ret_var)

    def generate(self):
        return insReturn(self.ret_var.generate())

    def get_input_vars(self):
        return [self.ret_var]

class irNop(IR):
    def __str__(self, **kwargs):
        return "NOP" 

    def generate(self):
        return insNop()

class irBinop(IR):
    def __init__(self, result, op, left, right, **kwargs):
        super(irBinop, self).__init__(**kwargs)
        self.result = result
        self.op = op
        self.left = left
        self.right = right
    
    def __str__(self):
        s = '%s = %s %s %s' % (self.result, self.left, self.op, self.right)

        return s

    def get_input_vars(self):
        return [self.left, self.right]

    def get_output_vars(self):
        return [self.result]

    def generate(self):
        ops = {
            'i32':
                {'eq': insCompareEq,
                'neq': insCompareNeq,
                'gt': insCompareGt,
                'gte': insCompareGtE,
                'lt': insCompareLt,
                'lte': insCompareLtE,
                'logical_and': insAnd,
                'logical_or': insOr,
                'add': insAdd,
                'sub': insSub,
                'mul': insMul,
                'div': insDiv,
                'mod': insMod},
            'f16':
                {'eq': insF16CompareEq,
                'neq': insF16CompareNeq,
                'gt': insF16CompareGt,
                'gte': insF16CompareGtE,
                'lt': insF16CompareLt,
                'lte': insF16CompareLtE,
                'logical_and': insF16And,
                'logical_or': insF16Or,
                'add': insF16Add,
                'sub': insF16Sub,
                'mul': insF16Mul,
                'div': insF16Div,
                'mod': insF16Mod},
            'str':
                # placeholders for now
                # need actual string instructions
                {'eq': insBinop, # compare
                'neq': insBinop, # compare not equal
                'in': insBinop,  # search
                'add': insBinop, # concatenate
                'mod': insBinop} # formatted output
        }

        data_type = self.left.type

        # gfx16 type can just default to i32
        if isinstance(self.left, irVar_gfx16):
            data_type = 'i32'
        
        return ops[data_type][self.op](self.result.generate(), self.left.generate(), self.right.generate())


class irUnaryNot(IR):
    def __init__(self, target, value, **kwargs):
        super(irUnaryNot, self).__init__(**kwargs)
        self.target = target
        self.value = value

    def __str__(self):
        return "%s = NOT %s" % (self.target, self.value)

    def generate(self):
        return insNot(self.target.generate(), self.value.generate())

    def get_input_vars(self):
        return [self.value]

    def get_output_vars(self):
        return [self.target]

type_conversions = {
    ('i32', 'f16'): insConvF16toI32,
    ('f16', 'i32'): insConvI32toF16,
}
        
# convert value to result's type and store in result
class irConvertType(IR):
    def __init__(self, result, value, **kwargs):
        super(irConvertType, self).__init__(**kwargs)
        self.result = result
        self.value = value

        # assert self.result.type != self.value.type

    def __str__(self):
        if self.skip_conversion():
            s = '%s = %s' % (self.result, self.value)
        else:
            s = '%s = %s(%s)' % (self.result, self.result.type, self.value)

        return s

    def get_input_vars(self):
        return [self.value]

    def get_output_vars(self):
        return [self.result]

    def skip_conversion(self):
        return isinstance(self.value, irVar_gfx16)

    def generate(self):
        try:
            if self.skip_conversion():
                raise KeyError

            return type_conversions[(self.result.type, self.value.type)](self.result.generate(), self.value.generate())

        except KeyError:
            ins = insConvMov(self.result.generate(), self.value.generate())
            return ins

class irConvertTypeInPlace(IR):
    def __init__(self, target, src_type, **kwargs):
        super(irConvertTypeInPlace, self).__init__(**kwargs)
        self.target = target
        self.src_type = src_type
    
    def __str__(self):
        s = '%s = %s(%s)' % (self.target, self.target.type, self.target)

        return s

    def get_input_vars(self):
        return [self.target]

    def get_output_vars(self):
        return [self.target]

    def generate(self):
        return type_conversions[(self.target.type, self.src_type)](self.target.generate(), self.target.generate())


class irVectorOp(IR):
    def __init__(self, op, target, value, **kwargs):
        super(irVectorOp, self).__init__(**kwargs)
        self.op = op
        self.target = target
        self.value = value
        
    def __str__(self):
        s = '*%s %s=(vector) %s' % (self.target, self.op, self.value)

        return s

    def get_input_vars(self):
        if isinstance(self.target, irPixelAttr):
            return [self.value]
        else:
            return [self.value, self.target]

    def get_output_vars(self):
        if isinstance(self.target, irPixelAttr):
            return []

        else:
            return [self.target]

    def generate(self):
        ops = {
            'add': insVectorAdd,
            'sub': insVectorSub,
            'mul': insVectorMul,
            'div': insVectorDiv,
            'mod': insVectorMod,
        }

        pixel_ops = {
            'add': insPixelVectorAdd,
            'sub': insPixelVectorSub,
            'mul': insPixelVectorMul,
            'div': insPixelVectorDiv,
            'mod': insPixelVectorMod,
        }

        if isinstance(self.target, irPixelAttr):
            target = self.target.generate()
            return pixel_ops[self.op](target.name, target.attr, self.value.generate())

        else:
            return ops[self.op](self.target.generate(), self.value.generate())

class irClear(IR):
    def __init__(self, target, **kwargs):
        super(irClear, self).__init__(**kwargs)
        self.target = target

        assert self.target.length == 1
        
    def __str__(self):
        return '%s = 0' % (self.target)

    def get_output_vars(self):
        return [self.target]

    def generate(self):
        return insClr(self.target.generate())


class irAssign(IR):
    def __init__(self, target, value, **kwargs):
        super(irAssign, self).__init__(**kwargs)
        self.target = target
        self.value = value

        assert self.target.length == 1
        
    def __str__(self):
        return '%s = %s' % (self.target, self.value)

    def get_input_vars(self):
        return [self.value]

    def get_output_vars(self):
        return [self.target]

    def generate(self):
        return insMov(self.target.generate(), self.value.generate())

class irVectorAssign(IR):
    def __init__(self, target, value, **kwargs):
        super(irVectorAssign, self).__init__(**kwargs)
        self.target = target
        self.value = value
        
    def __str__(self):
        return '*%s =(vector) %s' % (self.target, self.value)

    def get_input_vars(self):
        if isinstance(self.target, irPixelAttr):
            return [self.value]
        else:
            return [self.value, self.target]

    def get_output_vars(self):
        if isinstance(self.target, irPixelAttr):
            return []
            
        else:
            return [self.target]

    def generate(self):
        if isinstance(self.target, irPixelAttr):
            target = self.target.generate()
            return insPixelVectorMov(target.name, target.attr, self.value.generate())

        else:
            return insVectorMov(self.target.generate(), self.value.generate())

class irCall(IR):
    def __init__(self, target, params, args, result, **kwargs):
        super(irCall, self).__init__(**kwargs)
        self.target = target
        self.params = params
        self.args = args
        self.result = result

    def __str__(self):
        params = params_to_string(self.params)
        s = 'CALL %s(%s) -> %s' % (self.target, params, self.result)

        return s

    def get_input_vars(self):
        return self.params

    def get_output_vars(self):
        return [self.result]

    def generate(self):        
        params = [a.generate() for a in self.params]
        args = [a.generate() for a in self.args]

        # call func
        call_ins = insCall(self.target, params, args)

        # move return value to result register
        mov_ins = insMov(self.result.generate(), insAddr(0))

        return [call_ins, mov_ins]

class irLibCall(IR):
    def __init__(self, target, params, result, **kwargs):
        super(irLibCall, self).__init__(**kwargs)
        self.target = target
        self.params = params
        self.result = result

    def __str__(self):
        params = params_to_string(self.params)
        s = 'LCALL %s(%s) -> %s' % (self.target, params, self.result)
        
        return s

    def get_input_vars(self):
        return self.params

    def get_output_vars(self):
        return [self.result]

    def generate(self):        
        params = [a.generate() for a in self.params]

        if len(self.params) > 0 and isinstance(self.params[0], irDBAttr):
            db_item = params.pop(0)
            call_ins = insDBCall(self.target, db_item, self.result.generate(), params)

        else:
            call_ins = insLibCall(self.target, self.result.generate(), params)

        return call_ins


class irLabel(IR):
    def __init__(self, name, **kwargs):
        super(irLabel, self).__init__(**kwargs)        
        self.name = name

    def __str__(self):
        s = 'LABEL %s' % (self.name)

        return s

    def generate(self):
        return insLabel(self.name)


class irBranchConditional(IR):
    def __init__(self, value, target, **kwargs):
        super(irBranchConditional, self).__init__(**kwargs)        
        self.value = value
        self.target = target

    def get_input_vars(self):
        return [self.value]

    def get_jump_target(self):
        return self.target

class irBranchZero(irBranchConditional):
    def __init__(self, *args, **kwargs):
        super(irBranchZero, self).__init__(*args, **kwargs)        

    def __str__(self):
        s = 'BR Z %s -> %s' % (self.value, self.target.name)

        return s    

    def generate(self):
        return insJmpIfZero(self.value.generate(), self.target.generate())
        


class irBranchNotZero(irBranchConditional):
    def __init__(self, *args, **kwargs):
        super(irBranchNotZero, self).__init__(*args, **kwargs)        

    def __str__(self):
        s = 'BR NZ %s -> %s' % (self.value, self.target.name)

        return s    

    def generate(self):
        return insJmpIfNotZero(self.value.generate(), self.target.generate())
    

class irJump(IR):
    def __init__(self, target, **kwargs):
        super(irJump, self).__init__(**kwargs)        
        self.target = target

    def __str__(self):
        s = 'JMP -> %s' % (self.target.name)

        return s    

    def generate(self):
        return insJmp(self.target.generate())

    def get_jump_target(self):
        return self.target

class irJumpLessPreInc(IR):
    def __init__(self, target, op1, op2, **kwargs):
        super(irJumpLessPreInc, self).__init__(**kwargs)        
        self.target = target
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        s = 'JMP (%s++) < %s -> %s' % (self.op1, self.op2, self.target.name)

        return s    

    def get_input_vars(self):
        return [self.op1, self.op2]

    def generate(self):
        return insJmpIfLessThanPreInc(self.op1.generate(), self.op2.generate(), self.target.generate())

    def get_jump_target(self):
        return self.target


class irAssert(IR):
    def __init__(self, value, **kwargs):
        super(irAssert, self).__init__(**kwargs)        
        self.value = value

    def get_input_vars(self):
        return [self.value]

    def __str__(self):
        s = 'ASSERT %s' % (self.value)

        return s   


class irIndex(IR):
    def __init__(self, result, target, indexes=[], **kwargs):
        super(irIndex, self).__init__(**kwargs)        
        self.result = result
        self.target = target
        self.indexes = indexes

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.name)

        s = '%s = INDEX %s%s' % (self.result, self.target, indexes)

        return s

    def get_input_vars(self):
        temp = [self.target]
        temp.extend(self.indexes)

        return temp

    def get_output_vars(self):
        return [self.result]

    def generate(self):
        indexes = [i.generate() for i in self.indexes]
        counts = []
        strides = []

        target = self.target

        for i in xrange(len(self.indexes)):
            count = target.count
            counts.append(count)
            
            if isinstance(target, irRecord):
                target = target.get_field_from_offset(self.indexes[i])
                stride = 0

            else:
                target = target.type
                stride = target.length

            strides.append(stride)
        

        return insIndex(self.result.generate(), self.target.generate(), indexes, counts, strides)


class irPixelIndex(IR):
    def __init__(self, target, indexes=[], **kwargs):
        super(irPixelIndex, self).__init__(**kwargs)        

        self.name = target.name
        self.target = target
        self.indexes = indexes
        self.attr = None

        assert len(self.indexes) == 2

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.name)

        s = 'PIXEL INDEX %s.%s%s' % (self.name, self.attr, indexes)

        return s

    @property
    def type(self):
        return self.get_base_type()

    def get_input_vars(self):
        return self.indexes

    def get_base_type(self):    
        return PIX_ATTR_TYPES[self.attr]




class irDBIndex(IR):
    def __init__(self, target, indexes=[], **kwargs):
        super(irDBIndex, self).__init__(**kwargs)        

        self.name = target.name
        self.target = target
        self.attr = target.attr
        self.indexes = indexes
        self.type = 'db'

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.name)

        s = 'DB INDEX %s%s' % (self.name, indexes)

        return s

    def get_input_vars(self):
        return self.indexes

    def get_base_type(self):
        return self.type

class irDBStore(IR):
    def __init__(self, target, value, **kwargs):
        super(irDBStore, self).__init__(**kwargs)
        self.target = target
        self.value = value

        try:
            self.indexes = self.target.indexes

        except AttributeError:
            self.indexes = []
        
    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.name)

        return '%s%s = %s' % (self.target.name, indexes, self.value)

    def get_input_vars(self):
        temp = [self.value]
        temp.extend(self.indexes)

        return temp

    def generate(self):
        indexes = [a.generate() for a in self.indexes]
        return insDBStore(self.target.attr, indexes, self.value.generate())


class irDBLoad(IR):
    def __init__(self, target, value, **kwargs):
        super(irDBLoad, self).__init__(**kwargs)
        self.target = target
        self.value = value

        try:
            self.indexes = self.value.indexes

        except AttributeError:
            self.indexes = []
                
    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index.name)

        return '%s = %s%s' % (self.target, self.value.name, indexes)

    def get_input_vars(self):
        return self.indexes

    def get_output_vars(self):
        return [self.target]

    def generate(self):
        indexes = [a.generate() for a in self.indexes]
        return insDBLoad(self.target.generate(), self.value.attr, indexes)



class irPixelStore(IR):
    def __init__(self, target, value, **kwargs):
        super(irPixelStore, self).__init__(**kwargs)
        self.target = target
        self.value = value
        
    def __str__(self):
        indexes = ''
        for index in self.target.indexes:
            indexes += '[%s]' % (index.name)

        return '%s.%s%s = %s' % (self.target.name, self.target.attr, indexes, self.value)

    def get_input_vars(self):
        temp = [self.value]
        temp.extend(self.target.indexes)

        return temp

    def generate(self):
        ins = {
            'hue': insPixelStoreHue,
            'sat': insPixelStoreSat,
            'val': insPixelStoreVal,
            'hs_fade': insPixelStoreHSFade,
            'v_fade': insPixelStoreVFade,
        }

        indexes = []
        for index in self.target.indexes:
            indexes.append(index.generate())

        return ins[self.target.attr](self.target.name, self.target.attr, indexes, self.value.generate())


class irPixelLoad(IR):
    def __init__(self, target, value, **kwargs):
        super(irPixelLoad, self).__init__(**kwargs)
        self.target = target
        self.value = value
        
    def __str__(self):
        indexes = ''
        for index in self.value.indexes:
            indexes += '[%s]' % (index.name)

        return '%s = %s.%s%s' % (self.target, self.value.name, self.value.attr, indexes)

    def get_input_vars(self):
        return self.value.indexes

    def get_output_vars(self):
        return [self.target]

    def generate(self):
        ins_table = {
            'hue': insPixelLoadHue,
            'sat': insPixelLoadSat,
            'val': insPixelLoadVal,
            'hs_fade': insPixelLoadHSFade,
            'v_fade': insPixelLoadVFade,
            'is_hs_fading': insPixelIsHSFade,
            'is_v_fading': insPixelIsVFade,
        }

        ins = ins_table[self.value.attr]

        indexes = []
        for index in self.value.indexes:
            indexes.append(index.generate())

        if insPixelFading in ins.__bases__:
            # fade detection instructions need both indexes
            while len(indexes) < 2:
                indexes.append(CONST65535.generate())

        return ins(self.target.generate(), self.value.name, self.value.attr, indexes)


class irIndexLoad(IR):
    def __init__(self, result, address, **kwargs):
        super(irIndexLoad, self).__init__(**kwargs)        
        self.result = result
        self.address = address

    def __str__(self):
        s = '%s = *%s' % (self.result, self.address)

        return s    

    def get_input_vars(self):
        return [self.address]

    def get_output_vars(self):
        return [self.result]

    def generate(self):
        return insIndirectLoad(self.result.generate(), self.address.generate())

class irIndexStore(IR):
    def __init__(self, address, value, **kwargs):
        super(irIndexStore, self).__init__(**kwargs)        
        self.address = address
        self.value = value

    def __str__(self):
        s = '*%s = %s' % (self.address, self.value)

        return s    

    def get_input_vars(self):
        return [self.address]

    def get_output_vars(self):
        return [self.value]

    def generate(self):
        return insIndirectStore(self.value.generate(), self.address.generate())


CONST65535 = irConst(65535, lineno=0)


class Builder(object):
    def __init__(self, script_name='fx_script'):
        global source_code
        self.script_name = script_name

        # load source code for debug
        source_code = []
        try:
            with open(script_name, 'r') as f:
                source_code = f.readlines()

        except IOError: # cannot read file
            pass

        self.funcs = {}
        self.locals = {}
        self.globals = {}
        self.objects = {}
        self.pixel_arrays = {}
        self.labels = {}

        self.data_table = []
        self.data_count = 0
        self.code = []
        self.bytecode = []
        self.function_addrs = {}
        self.label_addrs = {}
        self.stream = ''
        self.header = None
        self.published_var_count = 0

        self.strings = {}

        self.pixel_array_indexes = ['pixels']
        self.read_keys = []
        self.write_keys = []

        self.links = []
        self.db_entries = {}

        self.cron_tab = {}

        self.loop_top = []
        self.loop_end = []

        self.next_temp = 0

        self.compound_lookup = []

        self.current_func = None

        self.data_types = {
            'i32': irVar_i32,
            'f16': irVar_f16,
            'gfx16': irVar_gfx16,
            'addr': irAddress,
            'db': irVar,
            'str': irVar_str,
        }

        self.record_types = {}

        # optimizations
        self.optimizations = {
            'fold_constants': True,
            'optimize_register_usage': True,
            'remove_unreachable_code': True,
        }

        # make sure we always have 0 and 65535 const
        self.add_const(0, lineno=0)
        self.globals['65535'] = CONST65535

        pixarray = irPixelArray('temp', lineno=0)
        pixfields = {}
        for field, value in pixarray.fields.items():
            pixfields[field] = {'type': 'i32', 'dimensions': []}

        self.create_record('PixelArray', pixfields, lineno=0)

        # create main pixels object
        self.pixelarray_object('pixels', args=[0, 65535], lineno=0)

    def __str__(self):
        s = "FX IR:\n"

        s += 'Globals:\n'
        for i in self.globals.values():
            s += '%d\t%s\n' % (i.lineno, i)

        s += 'Locals:\n'
        for fname in sorted(self.locals.keys()):
            if len(self.locals[fname].values()) > 0:
                s += '\t%s\n' % (fname)

                for l in sorted(self.locals[fname].values()):
                    s += '%d\t\t%s\n' % (l.lineno, l)

        s += 'PixelArrays:\n'
        for i in self.pixel_arrays.values():
            s += '%d\t%s\n' % (i.lineno, i)

        s += 'Functions:\n'
        for func in self.funcs.values():
            s += '%s\n' % (func)

        return s

    def finish_module(self):
        # clean up stuff after first pass is done

        for func in self.funcs.values():
            func.remove_dead_labels()

        for func in self.funcs.values():
            prev_line = 0
            for ir in func.body:
                if isinstance(ir, irLabel):
                    ir.lineno = prev_line

                else:
                    if ir.lineno > prev_line:
                        prev_line = ir.lineno
        return self

    def link(self, send, source, dest, query, lineno=None):
        new_link = {'send': send,
                    'source': source,
                    'dest': dest,
                    'query': query}

        self.links.append(new_link)

    def db(self, name, type, count, lineno=None):
        db_entry = CatbusMeta(hash=catbus_string_hash(name), 
                              type=type,
                              array_len=count - 1)

        self.db_entries[name] = db_entry

    def add_type(self, name, data_type, lineno=None):
        if name in self.data_types:
            raise SyntaxError("Type '%s' already defined" % (name), lineno=lineno)

        self.data_types[name] = data_type

    def create_record(self, name, fields, lineno=None):
        new_fields = {}
        offsets = {}
        offset = 0
        for field_name, field in fields.items():
            field_type = field['type']
            field_dims = field['dimensions']
            
            new_fields[field_name] = self.build_var(field_name, field_type, field_dims, lineno=lineno)

            offsets[field_name] = self.add_const(offset, lineno=lineno)
            offset += new_fields[field_name].length

        ir = irRecord(name, name, new_fields, offsets, lineno=lineno)

        self.record_types[name] = ir

    def get_type(self, name, lineno=None):
        if name not in self.data_types:

            # try records
            if name not in self.record_types:
                raise SyntaxError("Type '%s' not defined" % (name), lineno=lineno)

            return self.record_types[name]

        return self.data_types[name]

    def build_var(self, name, data_type, dimensions=[], keywords=None, lineno=None):
        data_type = self.get_type(data_type, lineno=lineno)

        if len(dimensions) == 0:
            ir = data_type(name, options=keywords, lineno=lineno)

        else:
            ir = irArray(name, data_type(name, lineno=lineno), dimensions=dimensions, options=keywords, lineno=lineno)

        return ir

    def add_global(self, name, data_type='i32', dimensions=[], keywords=None, lineno=None):
        if name in self.globals:
            # return self.globals[name]
            raise SyntaxError("Global variable '%s' already declared" % (name), lineno=lineno)

        ir = self.build_var(name, data_type, dimensions, keywords=keywords, lineno=lineno)
        ir.is_global = True

        try:   
            for v in ir:
                self.globals[v.name] = v

        except TypeError:
            self.globals[name] = ir

        return ir

    def add_local(self, name, data_type='i32', dimensions=[], keywords=None, lineno=None):
        ir = self._add_local_var(name, data_type=data_type, dimensions=dimensions, keywords=keywords, lineno=lineno)

        if isinstance(ir, irVar_str):
            self.assign(ir, ir.default_value, lineno=lineno)

        else:
            # add init to 0
            self.clear(ir, lineno=lineno)

        return ir

    def add_func_arg(self, func, name, data_type='i32', dimensions=[], lineno=None):
        ir = self._add_local_var(name, data_type=data_type, dimensions=dimensions, lineno=lineno)

        ir.name = '$%s.%s' % (func.name, name)
        func.params.append(ir)

        return ir

    def _add_local_var(self, name, data_type='i32', dimensions=[], keywords=None, lineno=None):
        # check if this is already in the globals
        if name in self.globals:
            # return self.globals[name]
            raise VariableAlreadyDeclared("Variable '%s' already declared as global" % (name), lineno=lineno)

        if name in self.locals[self.current_func]:
            # return self.locals[self.current_func][name]
            raise VariableAlreadyDeclared("Local variable '%s' already declared" % (name), lineno=lineno)

        if keywords != None:
            if 'publish' in keywords:
                raise SyntaxError("Cannot publish a local variable: %s" % (name), lineno=lineno)            

            if 'persist' in keywords:
                raise SyntaxError("Cannot persist a local variable: %s" % (name), lineno=lineno)            

        ir = self.build_var(name, data_type, dimensions, keywords=keywords, lineno=lineno)

        try:
            for v in ir:
                self.locals[self.current_func][v.name] = v

        except TypeError:
            self.locals[self.current_func][name] = ir

        return ir

    def get_var(self, name, lineno=None):
        name = str(name)

        # map true and false to 1/0 respectively
        if name == 'True':
            return self.globals['1']

        if name == 'False':
            return self.globals['0']

        if name in self.pixel_arrays:
            return self.pixel_arrays[name]

        if name in self.globals:
            return self.globals[name]

        try:
            return self.locals[self.current_func][name]

        except KeyError:
            raise VariableNotDeclared(name, "Variable '%s' not declared" % (name), lineno=lineno)

    def get_obj_var(self, obj_name, attr, lineno=None):
        name = '%s.%s' % (obj_name, attr)
        if name in self.globals:
            return self.globals[name]

        elif obj_name in self.pixel_arrays:
            try:
                if obj_name in self.globals:
                    return self.globals[obj_name].fields[attr]

            except KeyError:
                # attribute not found in pixel array record, 
                # so it must be an array
                pass                

            obj = self.pixel_arrays[obj_name]
            
            ir = irPixelAttr(obj, attr, lineno=lineno)

            return ir

        elif obj_name == 'db':
            ir = irDBAttr(obj_name, attr, lineno=lineno)

            return ir

        else:
            raise VariableNotDeclared(obj_name, "Object '%s' not declared" % (obj_name), lineno=lineno)            

    def add_const(self, name, data_type='i32', lineno=None):
        name = str(name)

        if name in self.globals:
            return self.globals[name]

        ir = irConst(name, data_type, lineno=lineno)

        self.globals[name] = ir

        return ir

    def add_temp(self, data_type='i32', lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = self.build_var(name, data_type, [], lineno=lineno)
        self.locals[self.current_func][name] = ir

        return ir

    def add_string(self, string, lineno=None):
        try:
            ir = self.strings[string]

        except KeyError:
            ir = irStrLiteral(string, lineno=lineno)

            self.strings[string] = ir

        return ir

    def remove_local_var(self, var):
        del self.locals[self.current_func][var.name]

    def func(self, *args, **kwargs):
        func = irFunc(*args, **kwargs)
        self.funcs[func.name] = func
        self.locals[func.name] = {}
        self.current_func = func.name
        self.next_temp = 0 

        return func

    def append_node(self, node):
        self.funcs[self.current_func].append(node)

    def get_current_node(self):
        return self.funcs[self.current_func].get(-1)

    def ret(self, value, lineno=None):
        ir = irReturn(value, lineno=lineno)

        self.append_node(ir)

        return ir

    def nop(self, lineno=None):
        ir = irNop(lineno=lineno)

        self.append_node(ir)

        return ir

    def binop(self, op, left, right, lineno=None):
        if self.optimizations['fold_constants'] and \
            isinstance(left, irConst) and \
            isinstance(right, irConst):

            return self._fold_constants(op, left, right, lineno)

        # resolve indirect accesses, if any
        if isinstance(left, irAddress) or \
            isinstance(left, irPixelIndex) or\
            isinstance(left, irDBAttr)  or \
            isinstance(left, irDBIndex):

            left = self.load_indirect(left, lineno=lineno)

        if isinstance(right, irAddress) or \
            isinstance(right, irPixelIndex) or\
            isinstance(right, irDBAttr)  or \
            isinstance(right, irDBIndex):
            right = self.load_indirect(right, lineno=lineno)


        if left.length != 1:
            raise SyntaxError("Binary operand must be scalar: %s" % (left.name), lineno=lineno)

        if right.length != 1:
            raise SyntaxError("Binary operand must be scalar: %s" % (right.name), lineno=lineno)

        # if both types are gfx16, use i32
        if left.type == 'gfx16' and right.type == 'gfx16':
            # if either type is fixed16, we do the whole thing as fixed16.
            data_type = 'i32'

        else:
            # if left is gfx16, use right type
            if left.type == 'gfx16':
                data_type = right.type
            else:
                data_type = left.type

            if right.type == 'f16':
                data_type = right.type

        left_result = left
        right_result = right
        
        # perform any conversions as needed
        # since we are prioritizing fixed16, we only need to convert i32 to f16
        if data_type == 'f16':
            if left.type != 'f16':
                left_result = self.add_temp(data_type=data_type, lineno=lineno)

                ir = irConvertType(left_result, left, lineno=lineno)
                self.append_node(ir)

            if right.type != 'f16':
                right_result = self.add_temp(data_type=data_type, lineno=lineno)

                ir = irConvertType(right_result, right, lineno=lineno)
                self.append_node(ir)

        if op in COMPARE_BINOPS:
            # need i32 for comparisons
            data_type = 'i32'

        # generate result register with target data type
        result = self.add_temp(data_type=data_type, lineno=lineno)

        ir = irBinop(result, op, left_result, right_result, lineno=lineno)

        self.append_node(ir)

        return result

    def unary_not(self, value, lineno=None):
        # generate result register with target data type
        result = self.add_temp(data_type=value.type, lineno=lineno)
    
        ir = irUnaryNot(result, value, lineno=lineno)        

        self.append_node(ir)

        return result

    def clear(self, target, lineno=None):   
        # check that we aren't clearing a string, which makes no sense
        assert not isinstance(target, irVar_str)
        
        if target.length == 1:
            ir = irClear(target, lineno=lineno)
            self.append_node(ir)
        else:
            self.assign(target, self.get_var(0, lineno=lineno), lineno=lineno)
        
    def assign(self, target, value, lineno=None):     
        # check types
        # don't do conversion if value is an address, or a pixel/db index
        if target.get_base_type() != value.get_base_type() and \
            not isinstance(value, irAddress) and \
            not isinstance(target, irAddress) and \
            not isinstance(target, irPixelIndex) and \
            not isinstance(value, irPixelIndex) and \
            not isinstance(target, irPixelAttr) and \
            not isinstance(target, irDBAttr) and \
            not isinstance(value, irDBAttr) and \
            not isinstance(target, irDBIndex) and \
            not isinstance(value, irDBIndex) and \
            not isinstance(value, irVar_str) and \
            not isinstance(value, irStrLiteral):
            # in normal expressions, f16 will take precedence over i32.
            # however, for the assign, the assignment target will 
            # have priority.

            # also note we skip this conversion for pixel array accesses,
            # as the gfx16 type works seamlessly as i32 and f16 without conversions.
            
            # check if value is const 0
            # if so, we don't need to convert
            if isinstance(value, irConst) and value.value == 0:
                pass

            else:
                # convert value to target type and replace value with result
                conv_result = self.add_temp(lineno=lineno, data_type=target.get_base_type())
                ir = irConvertType(conv_result, value, lineno=lineno)
                self.append_node(ir)
                value = conv_result

        if isinstance(value, irAddress):
            if value.target.length > 1:
                raise SyntaxError("Cannot assign from compound type '%s' to '%s'" % (value.target.name, target.name), lineno=lineno)

            self.load_indirect(value, target, lineno=lineno)

            # check types
            if target.get_base_type() != value.get_base_type():
                # mismatch.
                # in this case, we've already done the indirect load into the target, but 
                # it has the wrong type. we're going to do the conversion on top of itself.
                ir = irConvertTypeInPlace(target, value.get_base_type(), lineno=lineno)
                self.append_node(ir)

        elif isinstance(target, irAddress):
            if target.target.length == 1:
                self.store_indirect(target, value, lineno=lineno)

            else:
                ir = irVectorAssign(target, value, lineno=lineno)
                self.append_node(ir)

        elif isinstance(target, irArray):
            result = self.add_temp(lineno=lineno, data_type='addr')
            ir = irIndex(result, target, lineno=lineno)
            self.append_node(ir)
            result.target = target

            ir = irVectorAssign(result, value, lineno=lineno)
            self.append_node(ir)

        elif isinstance(target, irDBIndex) or isinstance(target, irDBAttr):
            ir = irDBStore(target, value, lineno=lineno)
            self.append_node(ir)

        elif isinstance(value, irDBIndex) or isinstance(value, irDBAttr):
            ir = irDBLoad(target, value, lineno=lineno)
            self.append_node(ir)

        elif isinstance(target, irPixelIndex):
            ir = irPixelStore(target, value, lineno=lineno)
            self.append_node(ir)

        elif isinstance(value, irPixelIndex):
            ir = irPixelLoad(target, value, lineno=lineno)
            self.append_node(ir) 

        else:
            try:
                # check if previous instruction has a result
                previous_ir = self.get_current_node()

                result = previous_ir.result
                
                # check if previous result is the same as the
                # value in this assignment.
                if result == value:
                    # match!
                    # replace previous result with the assignment
                    # target
                    previous_ir.result = target

                    # we can get rid of the temp result as it is
                    # now unused
                    self.remove_local_var(result)

                    return

            except IndexError:
                # no previous instruction, don't do anything
                pass

            except AttributeError:
                # no result, don't do anything
                pass
                
            ir = irAssign(target, value, lineno=lineno)
            self.append_node(ir)

    def augassign(self, op, target, value, lineno=None):
        # print op, target, value
        # check types
        if target.get_base_type() != value.get_base_type() and \
            not isinstance(target, irPixelIndex) and \
            not isinstance(value, irPixelIndex) and \
            not isinstance(target, irPixelAttr) and \
            not isinstance(target, irDBAttr) and \
            not isinstance(value, irDBAttr) and \
            not isinstance(target, irDBIndex) and \
            not isinstance(value, irDBIndex):
            # in normal expressions, f16 will take precedence over i32.
            # however, for the augassign, the assignment target will 
            # have priority.

            # also note we skip this conversion for pixel array accesses,
            # as the gfx16 type works seamlessly as i32 and f16 without conversions.

            # check if value is const 0
            # if so, we don't need to convert
            if isinstance(value, irConst) and value.value == 0:
                pass

            else:
                # convert value to target type and replace value with result
                conv_result = self.add_temp(lineno=lineno, data_type=target.get_base_type())
                ir = irConvertType(conv_result, value, lineno=lineno)
                self.append_node(ir)
                value = conv_result

        if isinstance(target, irAddress):
            if target.target.length == 1:
                # not a vector op, but we have a target address and not a value
    
                # need to load indirect first
                result = self.load_indirect(target, lineno=lineno)
                result = self.binop(op, result, value, lineno=lineno)

                self.assign(target, result, lineno=lineno)

            else:
                result = target
                ir = irVectorOp(op, result, value, lineno=lineno)        
                self.append_node(ir)

        elif isinstance(target, irDBAttr) or isinstance(target, irDBIndex):
            result = self.add_temp(lineno=lineno, data_type=value.type)
            ir = irDBLoad(result, target, lineno=lineno)
            self.append_node(ir)

            result = self.binop(op, result, value, lineno=lineno)

            ir = irDBStore(target, result, lineno=lineno)
            self.append_node(ir)

        elif isinstance(value, irDBAttr) or isinstance(value, irDBIndex):
            result = self.add_temp(lineno=lineno, data_type=target.type)
            ir = irDBLoad(result, value, lineno=lineno)
            self.append_node(ir)

            result = self.binop(op, result, target, lineno=lineno)

            self.assign(target, result, lineno=lineno)

        elif isinstance(target, irPixelIndex):
            result = self.add_temp(lineno=lineno, data_type=target.type)
            ir = irPixelLoad(result, target, lineno=lineno)
            self.append_node(ir)

            result = self.binop(op, result, value, lineno=lineno)

            ir = irPixelStore(target, result, lineno=lineno)
            self.append_node(ir)

        elif isinstance(value, irPixelIndex):
            result = self.add_temp(lineno=lineno, data_type=target.type)
            ir = irPixelLoad(result, value, lineno=lineno)
            self.append_node(ir)

            # check if we need to convert
            if result.get_base_type() != value.get_base_type():
                ir = irConvertTypeInPlace(result, value.get_base_type(), lineno=lineno)
                self.append_node(ir)                

            result = self.binop(op, result, target, lineno=lineno)

            self.assign(target, result, lineno=lineno)
            
        elif target.length == 1:
            # if so, we can replace with a binop and assign
            result = self.binop(op, target, value, lineno=lineno)
            self.assign(target, result, lineno=lineno)

        else:
            # index address of target
            result = self.add_temp(lineno=lineno, data_type='addr')
            ir = irIndex(result, target, lineno=lineno)
            self.append_node(ir)
            result.target = target

            ir = irVectorOp(op, result, value, lineno=lineno)        
            self.append_node(ir)

    def load_indirect(self, address, result=None, lineno=None):
        if isinstance(address, irPixelIndex) or \
            isinstance(address, irDBAttr) or \
            isinstance(address, irDBIndex):

            try:
                data_type = address.type

            except KeyError:
                raise SyntaxError("Attribute %s not recognized" % (address.attr), lineno=lineno)

        else:
            data_type = address.target.type


        if result is None:
            result = self.add_temp(data_type=data_type, lineno=lineno)


        if isinstance(address, irPixelIndex) or isinstance(address, irPixelAttr):
            ir = irPixelLoad(result, address, lineno=lineno)            
        
        elif isinstance(address, irDBAttr) or isinstance(address, irDBIndex):
            ir = irDBLoad(result, address, lineno=lineno)            

        else:
            ir = irIndexLoad(result, address, lineno=lineno)
    
        self.append_node(ir)

        return result  

    def store_indirect(self, address, value, lineno=None):
        ir = irIndexStore(address, value, lineno=lineno)
    
        self.append_node(ir)

        return ir
        
    def call(self, func_name, params, lineno=None):
        result = self.add_temp(data_type='gfx16', lineno=lineno)

        try:
            args = self.funcs[func_name].params
            ir = irCall(func_name, params, args, result, lineno=lineno)

        except KeyError:
            if func_name in ARRAY_FUNCS and not isinstance(params[0], irDBAttr):
                # note we skip this check for DB items, we don't know their
                # length beforehand.

                if len(params) == 1:
                    try:
                        # array function.
                        # we have the array address in the first parameter.
                        # we'll put the array length in the second.
                        array_len = self.add_const(params[0].count, lineno=lineno)

                    except AttributeError:
                        array_len = self.add_const(1, lineno=lineno)
                    

                    # check if function is array length
                    if func_name == 'len':
                        # since arrays are fixed length, we don't need a libcall, we 
                        # can just do an assignment.
                        self.assign(result, array_len, lineno=lineno)

                        return result

                    else:
                        params.append(array_len)

                else:
                    raise SyntaxError("Array functions take one argument", lineno=lineno)                    
            
            if func_name == 'pix_count':
                raise SyntaxError("pix_count() is deprecated, use pixels.count", lineno=lineno)        
                
            ir = irLibCall(func_name, params, result, lineno=lineno)
        
        self.append_node(ir)        

        return result

    def label(self, name, lineno=None):
        if name not in self.labels:
            self.labels[name] = 0

        else:
            self.labels[name] += 1

        name += '.%d' % (self.labels[name])

        ir = irLabel(name, lineno=lineno)
        return ir

    def ifelse(self, test, lineno=None):
        body_label = self.label('if.then', lineno=lineno)
        else_label = self.label('if.else', lineno=lineno)
        end_label = self.label('if.end', lineno=lineno)

        if not isinstance(test, irBinop):
            result = self.add_temp(lineno=lineno)
            self.assign(result, test, lineno=lineno)
            test = result

        branch = irBranchZero(test, else_label, lineno=lineno)
        self.append_node(branch)

        return body_label, else_label, end_label

    def position_label(self, label):
        self.append_node(label)
        
    def begin_while(self, lineno=None):
        top_label = self.label('while.top', lineno=lineno)
        end_label = self.label('while.end', lineno=lineno)
        self.position_label(top_label)

        self.loop_top.append(top_label)
        self.loop_end.append(end_label)

    def test_while(self, test, lineno=None):
        ir = irBranchZero(test, self.loop_end[-1], lineno=lineno)
        self.append_node(ir)

    def end_while(self, lineno=None):
        ir = irJump(self.loop_top[-1], lineno=lineno)
        self.append_node(ir)

        self.position_label(self.loop_end[-1])

        self.loop_top.pop(-1)
        self.loop_end.pop(-1)

    def begin_for(self, iterator, lineno=None):
        begin_label = self.label('for.begin', lineno=lineno) # we don't actually need this label, but it is helpful for reading the IR
        self.position_label(begin_label)
        top_label = self.label('for.top', lineno=lineno)
        continue_label = self.label('for.cont', lineno=lineno)
        end_label = self.label('for.end', lineno=lineno)

        self.loop_top.append(continue_label)
        self.loop_end.append(end_label)

        # set up iterator code (init to -1, as first pass will increment before the body) 
        init_value = self.add_const(-1, lineno=lineno)
        ir = irAssign(iterator, init_value, lineno=lineno)
        self.append_node(ir)

        ir = irJump(continue_label, lineno=lineno)
        self.append_node(ir)

        return top_label, continue_label, end_label

    def end_for(self, iterator, stop, top, lineno=None):
        if stop.length != 1:
            raise SyntaxError("Invalid loop iteration count for '%s'" % (stop.name), lineno=lineno)

        ir = irJumpLessPreInc(top, iterator, stop, lineno=lineno)
        self.append_node(ir)

        self.loop_top.pop(-1)
        self.loop_end.pop(-1)

    def jump(self, target, lineno=None):
        ir = irJump(target, lineno=lineno)
        self.append_node(ir)

    def loop_break(self, lineno=None):
        assert self.loop_end[-1] != None
        self.jump(self.loop_end[-1], lineno=lineno)


    def loop_continue(self, lineno=None):
        assert self.loop_top[-1] != None    
        self.jump(self.loop_top[-1], lineno=lineno)

    def assertion(self, test, lineno=None):
        ir = irAssert(test, lineno=lineno)

        self.append_node(ir)

    def lookup_subscript(self, target, index, lineno=None):
        if len(self.compound_lookup) == 0:
            self.compound_lookup.append(target)

        if isinstance(index, irStrLiteral):
            resolved_target = target.lookup(self.compound_lookup[1:])

            if isinstance(resolved_target, irPixelArray):
                raise SyntaxError("Invalid syntax for PixelArray type, should be: %s.%s" % (target.name, index.name), lineno=lineno)

            if not isinstance(resolved_target, irRecord):
                raise SyntaxError("Invalid index: %s" % (index.name), lineno=lineno)

            # convert index to offset adress for record
            try:
                index = resolved_target.offsets[index.name]

            except KeyError:
                raise SyntaxError("Field '%s' not found in '%s'" % (index.name, target.name), lineno=lineno)

        elif isinstance(index, irDBAttr):
            # need to load from the db access

            result = self.add_temp(lineno=lineno)
            self.assign(result, index, lineno=lineno)
            index = result

        self.compound_lookup.append(index)

    def resolve_lookup(self, lineno=None):    
        target = self.compound_lookup.pop(0)

        indexes = []
        for index in self.compound_lookup:
            indexes.append(index)
        
        self.compound_lookup = []

        if isinstance(target, irDBAttr):
            return irDBIndex(target, indexes, lineno=lineno)

        elif isinstance(target, irPixelArray):
            # pixel index access requires 2D
            # default 65535 for y value if index not provided.
            # the gfx system knows how to handle this.
            if len(indexes) == 1:
                indexes.append(self.get_var(65535))

            return irPixelIndex(target, indexes, lineno=lineno)

        else:
            result = self.add_temp(lineno=lineno, data_type='addr')

            result.target = target.lookup(indexes)

            ir = irIndex(result, target, indexes, lineno=lineno)

            self.append_node(ir)

            return result

    def pixelarray_object(self, name, args=[], kw={}, lineno=None):    
        if name in self.pixel_arrays:
            raise SyntaxError("PixelArray '%s' already defined" % (name), lineno=lineno)

        self.pixel_arrays[name] = irPixelArray(name, args, kw, lineno=lineno)

        self.add_global(name, 'PixelArray', lineno=lineno)

    def generic_object(self, name, data_type, args=[], kw={}, lineno=None):
        if data_type == 'PixelArray':
            self.pixelarray_object(name, args, kw, lineno=lineno)

            return

        if name in self.objects:
            raise SyntaxError("Object '%s' already defined" % (name), lineno=lineno)

        self.objects[name] = irObject(name, data_type, args, kw, lineno=lineno)

    def _fold_constants(self, op, left, right, lineno):
        assert left.get_base_type() == right.get_base_type()

        val = None

        if op == 'eq':
            val = left.value == right.value

        elif op == 'neq':
            val = left.value != right.value

        elif op == 'gt':
            val = left.value > right.value

        elif op == 'gte':
            val = left.value >= right.value

        elif op == 'lt':
            val = left.value < right.value

        elif op == 'lte':
            val = left.value <= right.value

        elif op == 'logical_and':
            val = left.value and right.value

        elif op == 'logical_or':
            val = left.value or right.value

        elif op == 'add':
            val = left.value + right.value

        elif op == 'sub':
            val = left.value - right.value

        elif op == 'mul':
            val = left.value * right.value

            if left.get_base_type() == 'f16':
                val /= 65536

        elif op == 'div':
            if left.get_base_type() == 'f16':
                val = (left.value * 65536) / right.value

            else:
                val = left.value / right.value

        elif op == 'mod':
            val = left.value % right.value


        if left.get_base_type() == 'f16':
            return self.add_const(float(val) / 65536.0, data_type='f16', lineno=lineno)

        else:
            return self.add_const(val, lineno=lineno)


    def schedule(self, func, params, lineno=None):
        if func not in self.cron_tab:
            self.cron_tab[func] = []

        # convert parameters from const objects into raw integers
        for k, v in params.items():
            try:
                params[k] = v.name

            except AttributeError:
                params[k] = v.s                

        # check parameters
        if 'seconds' in params:
            if params['seconds'] < 0 or params['seconds'] > 59:
                raise SyntaxError("Seconds must be within 0 - 59, got %d" % (params['seconds']), lineno=lineno)
        
        if 'minutes' in params:
            if params['minutes'] < 0 or params['minutes'] > 59:
                raise SyntaxError("Minutes must be within 0 - 59, got %d" % (params['minutes']), lineno=lineno)

        if 'hours' in params:
            if params['hours'] < 0 or params['hours'] > 23:
                raise SyntaxError("Hours must be within 0 - 23, got %d" % (params['hours']), lineno=lineno)
        
        if 'day_of_month' in params:
            if params['day_of_month'] < 0 or params['day_of_month'] > 31:
                raise SyntaxError("Day of month must be within 0 - 31, got %d" % (params['day_of_month']), lineno=lineno)
        
        if 'day_of_week' in params:
            if isinstance(params['day_of_week'], basestring):
                params['day_of_week'] = DAY_OF_WEEK[params['day_of_week'].lower()]

            if params['day_of_week'] < 1 or params['day_of_week'] > 7:
                raise SyntaxError("Day of week must be within 1 - 7, got %d" % (params['day_of_week']), lineno=lineno)
        
        if 'month' in params:
            if isinstance(params['month'], basestring):
                params['month'] = MONTHS[params['month'].lower()]

            if params['month'] < 1 or params['month'] > 12:
                raise SyntaxError("Month must be within 1 - 12, got %d" % (params['month']), lineno=lineno)
                        

        for i in ['seconds', 'minutes', 'hours', 'day_of_month', 'day_of_week', 'month']:
            if i not in params:
                params[i] = -1

        self.cron_tab[func].append(params)

    def usedef(self, func):
        use = []
        define = []

        for ins in self.funcs[func].body:
            used = ins.get_input_vars()

            # look for address references, if so, include their target
            for v in copy(used):
                if isinstance(v, irAddress):
                    if v.target.name not in self.globals:
                        used.append(v.target)

            # use.append([a.name for a in used])
            use.append([a for a in used])


            defined = ins.get_output_vars()

            # filter globals and consts
            defined = [a for a in defined if not a.is_global and not a.is_const]

            # look for address references, if so, include their target
            for v in copy(defined):
                if isinstance(v, irAddress):
                    if v.target.name not in self.globals:
                        defined.append(v.target)

            # define.append([a.name for a in defined])
            define.append([a for a in defined])

        # attach function params
        # define[0].extend([a.name for a in self.funcs[func].params])
        define[0].extend([a for a in self.funcs[func].params])

        # define[0].extend(self.globals.keys())

        return use, define

    def control_flow(self, func, sequence=None, cfg=None, pc=None, jumps_taken=None):
        if sequence == None:
            sequence =[]

        if cfg == None:
            cfg = []

        if pc == None:
            pc = 0

        if jumps_taken == None:
            jumps_taken = []


        code = self.funcs[func]
        labels = code.labels()
        
        cfg.append(sequence)

        while True:
            ins = code.body[pc]

            sequence.append(pc)
            
            if isinstance(ins, irReturn):
                break

            jump = ins.get_jump_target()

            # check if unconditional jump
            if isinstance(ins, irJump) and ins not in jumps_taken:
                pc = labels[jump.name]

                jumps_taken.append(ins)

            elif jump != None:
                if ins not in jumps_taken:
                    jumps_taken.append(ins)

                    self.control_flow(func, sequence=copy(sequence), cfg=cfg, pc=labels[jump.name], jumps_taken=jumps_taken)

                pc += 1

            else:
                pc += 1


        return cfg

    def unreachable(self, func, cfg=None):
        if cfg == None:
            cfg = self.control_flow(func)


        unreachable = range(len(self.funcs[func].body))

        for sequence in cfg:
            for line in sequence:
                if line in unreachable:
                    unreachable.remove(line)

        return unreachable

    def liveness(self, func, pc=None):
        if pc == None:
            pc = 0

        """
        1. Gather inputs and outputs used by each instruction
            a. Returns are exit points: globals/consts are treated as inputs to returns,
               so that they do not get removed.

        2. Walk tree ->
            construct list of inputs/outputs for this particular iteration of the program
    
        3. Iterate list for this tree:
            now we have the linear sequence what gets used where, based on a particular
            set of branches taken.

            we can iterate backwards
    
            each time the variable shows up, add to liveness list.
            
            once we get to start of program (this version), we know at which
            lines each variable is alive.        

        """

        # inputs = use
        # outputs = define

        # from pprint import pprint


        use, define = self.usedef(func)

        cfgs = self.control_flow(func)

        self.debug_print('unreachable')
        unreachable = self.unreachable(func, cfgs)
        for r in unreachable:
            self.debug_print('\t%s' % (r))
        self.debug_print('\n')

        code = self.funcs[func].body


        liveness = [[] for i in xrange(len(code))]

        for line in unreachable:
            liveness[line] = None

        # print 'CFG:'
        for cfg in cfgs:

            # print cfg
    
            prev = []
            for i in reversed(cfg):

                # add previous live variables
                liveness[i].extend(prev)

                # add current variables being used
                liveness[i].extend(use[i])

                # uniqueify
                liveness[i] = list(set(liveness[i]))

                prev = liveness[i]

        for cfg in cfgs:
            defined = []
            for i in cfg:
                # check if variables have been defined yet
                for v in copy(liveness[i]):

                    if v in define[i]:
                        defined.append(v)

                    elif v not in defined:
                        # remove from liveness as this variable has not been defined yet
                        liveness[i].remove(v)




        # print '------', func, '---------'

        # print 'use'
        # for i in use:
        #     print [a.name for a in i]
        # print 'define'
        # for i in define:
        #     print [a.name for a in i]
                
        # pc = 0
        
        # for l in liveness:
        #     temp = 5
        #     print pc, ': ',
            
        #     if l == None:
        #         print 'UNREACHABLE',

        #     else:
        #         for a in l:
        #             print a.name,
        #             temp -= 1

        #     print '\t' * temp, code[pc]

        #     pc += 1

        # print '------'

        return liveness

    def debug_print(self, s):
        # print s
        pass

    def allocate(self):
        self.remove_unreachable()        

        self.data_table = []
        self.data_count = 0

        addr = 0

        ret_var = irVar('$return', lineno=0)
        ret_var.addr = addr
        addr += 1

        self.data_table.append(ret_var)

        self.pixel_array_indexes = ['pixels']

        # allocate pixel array data
        # start with master array
        global_pixels = self.globals['pixels']
        for field_name in sorted(global_pixels.fields.keys()):
            field = global_pixels.fields[field_name]

            field.name = '%s.%s' % (global_pixels.name, field_name)
            field.addr = addr
            addr += 1

            # set default value
            field.default_value = self.pixel_arrays['pixels'].fields[field_name]
            
            self.data_table.append(field)

        # look for additional pixel arrays
        for i in self.globals.values():
            if isinstance(i, irRecord) and i.type == 'PixelArray' and i.name != 'pixels':
                self.pixel_array_indexes.append(i.name)

                for field_name in sorted(i.fields.keys()):
                    field = i.fields[field_name]

                    field.name = '%s.%s' % (i.name, field_name)
                    field.addr = addr
                    addr += 1

                    # set default value
                    field.default_value = self.pixel_arrays[i.name].fields[field_name]

                    self.data_table.append(field)

        # allocate all other globals
        for i in self.globals.values():
            if isinstance(i, irRecord) and i.type == 'PixelArray':
                continue

            i.addr = addr
            addr += i.length

            self.data_table.append(i)


        if self.optimizations['optimize_register_usage']:
            self.debug_print('optimize_register_usage')
            for func in self.funcs:
                self.debug_print('optimize %s' % func)

                registers = {}
                address_pool = []

                liveness = self.liveness(func)

                line_no = 0

                for line in liveness:
                    self.debug_print('line %s %s' % (line_no, [a.name for a in line]))
                    line_no += 1

                    # check if line is marked None, this means
                    # we need to skip it in the analysis (probably unreachable code)
                    if line == None:
                        continue

                    # remove anything that is no longer live
                    for var in registers.values():
                        # check for arrays with obviously bogus sizes
                        assert var.length < 65535

                        if var not in line:
                            self.debug_print('remove %s %s' % (var, var.addr))

                            del registers[var.name]

                            var_addr = var.addr
                            for i in xrange(var.length):
                                address_pool.append(var_addr)
                                var_addr += 1


                    for var in line:
                        if (var.addr != None) and (var.addr in address_pool):
                            self.debug_print('unpool %s %s' % (var, var.addr))
                            address_pool.remove(var.addr)


                    for var in line:
                        if var.addr == None:
                            if var.name not in registers:
                                registers[var.name] = var

                                var_addr = None

                                # search pool for addresses
                                if len(address_pool) >= var.length:
                                    
                                    # easy mode, length=1 vars: pop address
                                    if var.length == 1:
                                        var_addr = address_pool.pop()

                                else:
                                    # placeholder for arrays.  need to find 
                                    # contiguous block in address pool.
                                    pass

                                if var_addr == None:
                                    var_addr = addr

                                    addr += var.length
                                    self.debug_print('alloc %s %s' % (var, var_addr))

                                else:
                                    self.debug_print('pool %s %s' % (var, var_addr))

                                # assign address to var
                                var.addr = var_addr

                                
                    self.debug_print('%s %s %s\n' % (line, registers, address_pool))
                    

                # Trash vars:
                # Some instructions return a value that doesn't get used by anything.
                # A common example is a lib call who's return value isn't being assigned
                # to a variable.
                # These instructions still have to store their result somewhere,
                # so we create a dummy var called trash for them.

                trash_var = None

                for ins in self.funcs[func].body:
                    unallocated = [a for a in ins.get_output_vars() if a.addr == None]

                    for a in unallocated:
                        if trash_var == None:
                            trash_var = irVar_i32('$trash', lineno=ins.lineno)
                            trash_var.addr = addr
                            addr += 1

                            self.data_table.append(trash_var)

                        a.addr = trash_var.addr

                    
            for func_name, local in self.locals.items():
                for i in local.values():
                    # assign func name to var
                    i.name = '%s.%s' % (func_name, i.name)

                    self.data_table.append(i)

        else:
            for func_name, local in self.locals.items():
                for i in local.values():
                    i.addr = addr
                    addr += i.length

                    # assign func name to var
                    i.name = '%s.%s' % (func_name, i.name)

                    self.data_table.append(i)

        # scan instructions for referenced string literals
        used_strings = []
        for func in self.funcs:
            for ins in self.funcs[func].body:
                for var in ins.get_input_vars():
                    if isinstance(var, irStrLiteral) and var not in used_strings:
                        used_strings.append(var)

        global_strings = []
        # do the same thing for global vars
        for g in self.globals.values():
            if isinstance(g, irVar_str):
                global_strings.append(g)
                if g.default_value not in used_strings:
                    used_strings.append(g.default_value)


        # only allocating storage for strings referenced by instructions
        # mainly this will filter out string literals coming in from record
        # subscripts.  we aren't using those computationally, so they don't
        # need to be here.
        self.strings = used_strings

        # allocate storage for strings
        for s in self.strings:
            s.addr = addr
            addr += s.size

        # now update global strings to map to their default values to point
        # to their string literal's address
        for g in global_strings:
            g.default_value = g.default_value.addr

        self.data_count = addr

        return self.data_table

    def print_data_table(self, data):
        print "DATA: "
        for i in sorted(data, key=lambda d: d.addr):
            default_value = ''

            if i.length == 1:
                if i.get_base_type() == 'f16':
                    default_value += '%f ' % (float(i.default_value / 65536.0))

                else:
                    default_value += '%s ' % (i.default_value)

            else:
                default_value = '['
                for n in xrange(i.length):
                    try:
                        val = i.default_value[n]    
                    except TypeError: # no default value given, so this will be all 0s
                        val = 0
                    
                    if i.get_base_type() == 'f16':
                        default_value += '%f, ' % (float(val / 65536.0))

                    else:
                        default_value += '%s, ' % (val)


                default_value += ']'
            
            print '\t%3d: %s = %s' % (i.addr, i, default_value)

        print "STRINGS: "
        if len(self.strings) == 0:
            print "\t None"

        else:
            for s in self.strings:
                print '\t%3d: [%3d] %s' % (s.addr, s.strlen, s.name)

    def print_instructions(self, instructions):
        print "INSTRUCTIONS: "
        i = 0
        for func in instructions:
            print '\t%s:' % (func)

            for ins in instructions[func]:
                s = '\t\t%3d: %s' % (i, str(ins))
                print s
                i += 1

    def remove_unreachable(self):
        if self.optimizations['remove_unreachable_code']:
            for func in self.funcs.values():
                unreachable = self.unreachable(func.name)

                new_code = []
                for i in xrange(len(func.body)):
                    if i not in unreachable:
                        new_code.append(func.body[i])

                func.body = new_code        

    def generate_instructions(self):
        self.code = {}

        # check if there is no init function
        if 'init' not in self.funcs:
            self.func('init', lineno=0)
            self.ret(self.get_var(0), lineno=0)

        # check if there is no loop function
        if 'loop' not in self.funcs:
            self.func('loop', lineno=0)
            self.ret(self.get_var(0), lineno=0)
        
        for func in self.funcs.values():
            ins = []
                    
            code = func.generate()
            ins.extend(code)

            self.code[func.name] = ins

        return self.code

    def assemble(self):
        self.function_addrs = {}
        self.label_addrs = {}
        self.bytecode = []
        self.write_keys = []
        self.read_keys = []

        # generate byte code, store addresses of functions and labels
        for func in self.code:
            assert isinstance(self.code[func][0], insFunction)

            for ins in self.code[func]:
                if isinstance(ins, insFunction):
                    self.function_addrs[func] = len(self.bytecode)

                elif isinstance(ins, insLabel):
                    self.label_addrs[ins.name] = len(self.bytecode)

                else:
                    if isinstance(ins, insDBStore):
                        if ins.db_item not in self.write_keys:
                            self.write_keys.append(ins.db_item)

                    elif isinstance(ins, insDBLoad) or isinstance(ins, insDBCall):
                        if ins.db_item not in self.read_keys:
                            self.read_keys.append(ins.db_item)

                    try:
                        self.bytecode.extend(ins.assemble())

                    except Exception:
                        print "Assembly failed for %s" % (ins)
                        raise

        # go through byte code and replace labels with addresses
        i = 0
        while i < len(self.bytecode):
            if isinstance(self.bytecode[i], insLabel):
                name = self.bytecode[i].name

                l = self.label_addrs[name] & 0xff
                h = (self.label_addrs[name] >> 8) & 0xff 

                self.bytecode[i] = l
                i += 1
                self.bytecode[i] = h

            elif isinstance(self.bytecode[i], insFuncTarget):
                name = self.bytecode[i].name

                l = self.function_addrs[name] & 0xff
                h = (self.function_addrs[name] >> 8) & 0xff 

                self.bytecode[i] = l
                i += 1
                self.bytecode[i] = h

            elif isinstance(self.bytecode[i], insPixelArray):
                # replace array with index
                self.bytecode[i] = self.pixel_array_indexes.index(self.bytecode[i].name)


            i += 1

        return self.bytecode


    def generate_binary(self, filename=None):
        stream = ''
        meta_names = []

        code_len = len(self.bytecode)
        data_len = self.data_count * DATA_LEN

        # set up padding so data start will be on 32 bit boundary
        padding_len = 4 - (code_len % 4)
        code_len += padding_len


        # set up pixel arrays
        pix_obj_len = 0
        for pix in self.pixel_arrays.values():
            pix_obj_len += pix.length

        # set up read keys
        packed_read_keys = ''
        for key in self.read_keys:
            packed_read_keys += struct.pack('<L', catbus_string_hash(key))

        # set up write keys
        packed_write_keys = ''
        for key in self.write_keys:
            packed_write_keys += struct.pack('<L', catbus_string_hash(key))

        # set up published registers
        self.published_var_count = 0
        packed_publish = ''
        for var in self.data_table:
            if var.publish:
                packed_publish += VMPublishVar(
                                    hash=catbus_string_hash(var.name), 
                                    addr=var.addr,
                                    type=get_type_id(var.type)).pack()

                meta_names.append(var.name)
                self.published_var_count += 1

        # set up links
        packed_links = ''
        for link in self.links:
            source_hash = catbus_string_hash(link['source'])
            dest_hash = catbus_string_hash(link['dest'])
            query = [catbus_string_hash(a) for a in link['query']]

            meta_names.append(link['source'])
            meta_names.append(link['dest'])
            for q in link['query']:
                meta_names.append(q)

            packed_links += Link(send=link['send'],
                                 source_hash=source_hash, 
                                 dest_hash=dest_hash, 
                                 query=query).pack()

        # set up DB entries
        packed_db = ''
        for name, entry in self.db_entries.items():
            packed_db += entry.pack()

            meta_names.append(name)
        
        # set up cron entries
        packed_cron = ''
        for func_name, entries in self.cron_tab.items():
            
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
                    program_name_hash=catbus_string_hash(self.script_name),
                    code_len=code_len,
                    data_len=data_len,
                    pix_obj_len=pix_obj_len,
                    read_keys_len=len(packed_read_keys),
                    write_keys_len=len(packed_write_keys),
                    publish_len=len(packed_publish),
                    link_len=len(packed_links),
                    db_len=len(packed_db),
                    cron_len=len(packed_cron),
                    init_start=self.function_addrs['init'],
                    loop_start=self.function_addrs['loop'])

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

        for b in self.bytecode:
            stream += struct.pack('<B', b)

        # add padding if necessary to make sure data is 32 bit aligned
        stream += '\0' * padding_len

        # ensure padding is correct
        assert len(stream) % 4 == 0

        # add data table
        stream += struct.pack('<L', DATA_MAGIC)

        addr = -1
        for var in sorted(self.data_table, key=lambda a: a.addr):
            if var.addr <= addr:
                continue

            addr = var.addr

            for i in xrange(var.length):
                try:
                    default_value = var.default_value[i]
                except TypeError:
                    default_value = var.default_value

                # if default is pointing to a string literal,
                # just set to 0.  globals already have their default
                # addresses mapped, so this is a local, and they require
                # an assignment anyway, so the default doesn't matter.
                if isinstance(default_value, irStrLiteral):
                    default_value = 0
                    assert not var.is_global

                stream += struct.pack('<l', default_value)

            addr += var.length - 1

        # create hash of stream
        stream_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', stream_hash)

        # but wait, that's not all!
        # now we're going attach meta data.
        # first, prepend 32 bit (signed) program length to stream.
        # this makes it trivial to read the VM image length from the file.
        # this also gives us the offset into the file where the meta data begins.
        # note all strings will be padded to the VM_STRING_LEN.
        prog_len = len(stream)
        stream = struct.pack('<l', prog_len) + stream

        meta_data = ''

        # marker for meta
        meta_data += struct.pack('<L', META_MAGIC)

        # first string is script name
        # note the conversion with str(), this is because from a CLI
        # we might end up with unicode, when we really, really need ASCII.
        padded_string = str(self.script_name)
        # pad to string len
        padded_string += '\0' * (VM_STRING_LEN - len(padded_string))

        meta_data += padded_string

        # next, attach names of stuff
        for s in meta_names:
            padded_string = s

            if len(padded_string) > VM_STRING_LEN:
                raise SyntaxError("%s exceeds %d characters" % (padded_string, VM_STRING_LEN))

            padded_string += '\0' * (VM_STRING_LEN - len(padded_string))
            meta_data += padded_string


        # add meta data to end of stream
        stream += meta_data
        
        # attach hash of entire file
        file_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', file_hash)


        if filename:
            # write to file
            with open(filename, 'w') as f:
                f.write(stream)

        self.stream = stream
        self.header = header

        return stream


class VM(object):
    def __init__(self, builder=None, code=None, data=None, pix_size_x=4, pix_size_y=4):
        self.pixel_arrays = {}

        if builder == None:
            self.code = code
            self.data = data

        else:
            self.code = builder.code
            self.data = builder.data_table

            for k, v in builder.pixel_arrays.items():
                self.pixel_arrays[k] = v.fields

        # set up pixel arrays
        self.pix_count = pix_size_x * pix_size_y

        self.hue        = [0 for i in xrange(self.pix_count)]
        self.sat        = [0 for i in xrange(self.pix_count)]
        self.val        = [0 for i in xrange(self.pix_count)]
        self.hs_fade    = [0 for i in xrange(self.pix_count)]
        self.v_fade     = [0 for i in xrange(self.pix_count)]

        self.gfx_data   = {'hue': self.hue,
                           'sat': self.sat,
                           'val': self.val,
                           'hs_fade': self.hs_fade,
                           'v_fade': self.v_fade}


        # init db
        self.db = {}
        self.db['pix_size_x'] = pix_size_x
        self.db['pix_size_y'] = pix_size_y
        self.db['pix_count'] = self.pix_count
        self.db['kv_test_array'] = [0] * 4
        self.db['kv_test_key'] = 0

        # set up master pixel array
        self.pixel_arrays['pixels']['count'] = self.pix_count
        self.pixel_arrays['pixels']['index'] = 0
        self.pixel_arrays['pixels']['size_x'] = pix_size_x
        self.pixel_arrays['pixels']['size_y'] = pix_size_y
        self.pixel_arrays['pixels']['reverse'] = 0


        # init memory
        self.memory = []

        # sort data by addresses
        data = [a for a in sorted(self.data, key=lambda data: data.addr)]

        # for a in data:
            # print a.addr, a.length

        addr = -1
        for var in data:
            if var.addr <= addr:
                continue

            addr = var.addr

            for i in xrange(var.length):
                try:
                    self.memory.append(var.default_value[i])
                except TypeError:
                    self.memory.append(var.default_value)

            addr += var.length - 1

    def calc_index(self, x, y, pixel_array='pixels'):
        count = self.pixel_arrays[pixel_array]['count']
        size_x = self.pixel_arrays[pixel_array]['size_x']
        size_y = self.pixel_arrays[pixel_array]['size_y']

        if y == 65535:
            i = x % count

        else:
            x %= size_x
            y %= size_y

            i = x + (y * size_x)

        return i

    def dump_hsv(self):
        return self.gfx_data

    def dump_registers(self):
        registers = {}
        for var in self.data:
            # don't dump consts
            if isinstance(var, irConst):
                continue

            if isinstance(var, irArray):
                value = []
                addr = var.addr
                for i in xrange(var.length):
                    value.append(self.memory[addr])
                    addr += 1

            elif isinstance(var, irRecord):
                value = {}

                for f in var.fields:
                    index = self.memory[var.offsets[f].addr]
                    value[f] = self.memory[index + var.addr]

            else:
                value = self.memory[var.addr]

            # convert fixed16 to float
            if var.type == 'f16':
                value = (value >> 16) + (value & 0xffff) / 65536.0

            registers[var.name] = value

        return registers

    def run_once(self):
        self.run('init')
        self.run('loop')

    def run(self, func):
        cycles = 0
        pc = 0

        return_stack = []

        offsets = {}

        # linearize code stream
        code = []
        for v in self.code.values():
            code.extend(v)

        # scan code stream and get offsets for all functions and labels
        for i in xrange(len(code)):
            ins = code[i]
            if isinstance(ins, insFunction) or isinstance(ins, insLabel):
                offsets[ins.name] = i


        # setup PC
        try:
            pc = offsets[func]
        
        except KeyError:
            raise VMRuntimeError("Function '%s' not found" % (func))

        while True:
            cycles += 1

            ins = code[pc]

            # print cycles, pc, ins

            pc += 1

            try:    
                ret_val = ins.execute(self)

                if isinstance(ins, insCall):
                    # push PC to return stack
                    return_stack.append(pc)

                # if returning label to jump to
                if ret_val != None:
                    # jump to target
                    pc = offsets[ret_val.name]
                
            except ReturnException:
                if len(return_stack) == 0:
                    # program is complete
                    break

                # pop PC from return stack
                pc = return_stack.pop(-1)


        # clean up





