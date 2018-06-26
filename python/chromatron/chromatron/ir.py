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

from instructions import *

from copy import deepcopy

class SyntaxError(Exception):
    def __init__(self, message='', lineno=None):
        self.lineno = lineno

        message += ' (line %d)' % (self.lineno)

        super(SyntaxError, self).__init__(message)

class VMRuntimeError(Exception):
    def __init__(self, message=''):
        super(VMRuntimeError, self).__init__(message)


def params_to_string(params):
    s = ''

    for p in params:
        s += '%s %s,' % (p.type, p.name)

    # strip last comma
    s = s[:len(s) - 1]

    return s

class IR(object):
    def __init__(self, lineno=None):
        self.lineno = lineno

        assert self.lineno != None

    def generate(self):
        return BaseInstruction()

class irVar(IR):
    def __init__(self, name, type='i32', dimensions=[], **kwargs):
        super(irVar, self).__init__(**kwargs)
        self.name = name
        self.type = type
        self.length = 1
        self.dimensions = dimensions
        self.addr = None

    def __str__(self):
        return "Var (%s, %s)" % (self.name, self.type)

    def generate(self):
        return insAddr(self.addr, self)

class irVar_i32(irVar):
    def __init__(self, *args, **kwargs):
        super(irVar_i32, self).__init__(*args, **kwargs)
        self.type = 'i32'

class irVar_f16(irVar):
    def __init__(self, *args, **kwargs):
        super(irVar_f16, self).__init__(*args, **kwargs)
        self.type = 'f16'

class irAddress(irVar):
    def __init__(self, *args, **kwargs):
        super(irAddress, self).__init__(*args, **kwargs)
        self.type = 'addr_i32'

    def __str__(self):
        return "Addr (%s)" % (self.name)


class irConst(irVar):
    def __str__(self):
        value = self.name
        if self.type == 'f16':
            # convert internal to float for printing
            value = (self.name >> 16) + (self.name & 0xffff) / 65536.0

        return "Const (%s, %s)" % (value, self.type)

class irArray(irVar):
    def __init__(self, *args, **kwargs):
        super(irArray, self).__init__(*args, **kwargs)

        self.type_length = self.type.length
        self.type = self.type.type        

        self.length = self.dimensions[0] * self.type_length
        for i in xrange(len(self.dimensions) - 1):
            self.length *= self.dimensions[i + 1]

        self.strides = [0] * len(self.dimensions)
        self.strides[len(self.dimensions) - 1] = self.type_length

        # calculate stride lengths of each dimension
        for i in reversed(xrange(len(self.dimensions) - 1)):
            self.strides[i] = self.strides[i + 1] * self.dimensions[i + 1]            


    def __str__(self):
        return "Array (%s, %s, %d)" % (self.name, self.type, self.length)

class irRecord(irVar):
    def __init__(self, name, data_type, fields, **kwargs):
        super(irRecord, self).__init__(name, **kwargs)        

        self.fields = fields
        self.type = data_type

        self.length = 0
        for field in self.fields.values():
            self.length += field.length

    def __call__(self, name, dimensions=[], lineno=None):
        # creating an instance of a record type.
        # need to create copies of all variables, and then attach record name to them.
        fields = deepcopy(self.fields)

        for field in fields.values():
            field.name = '%s.%s' % (name, field.name)

        return irRecord(name, self.type, fields, lineno=lineno)

    def __str__(self):
        return "Record (%s, %s, %d)" % (self.name, self.type, self.length)

class irField(IR):
    def __init__(self, name, **kwargs):
        super(irField, self).__init__(**kwargs)
        self.name = name
        
    def __str__(self):
        return "Field (%s)" % (self.name)


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

    def __str__(self):
        params = params_to_string(self.params)

        s = "Func %s(%s) -> %s\n" % (self.name, params, self.ret_type)

        for node in self.body:
            s += '%d\t\t%s\n' % (node.lineno, node)

        return s

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
        }
        
        return ops[self.result.type][self.op](self.result.generate(), self.left.generate(), self.right.generate())


class irUnaryNot(IR):
    def __init__(self, target, value, **kwargs):
        super(irUnaryNot, self).__init__(**kwargs)
        self.target = target
        self.value = value

    def __str__(self):
        return "%s = NOT %s" % (self.target, self.value)

    def generate(self):
        return insNot(self.target.generate(), self.value.generate())


# convert value to result's type and store in result
class irConvertType(IR):
    def __init__(self, result, value, **kwargs):
        super(irConvertType, self).__init__(**kwargs)
        self.result = result
        self.value = value

        assert self.result.type != self.value.type
    
    def __str__(self):
        s = '%s = %s(%s)' % (self.result, self.result.type, self.value)

        return s

    def generate(self):
        type_conversions = {
            ('i32', 'f16'): insConvF16toI32,
            ('f16', 'i32'): insConvI32toF16,
        }

        return type_conversions[(self.result.type, self.value.type)](self.result.generate(), self.value.generate())


class irAugAssign(IR):
    def __init__(self, op, target, value, **kwargs):
        super(irAugAssign, self).__init__(**kwargs)
        self.op = op
        self.target = target
        self.value = value
        
    def __str__(self):
        s = '%s = %s %s(vector) %s' % (self.target, self.target, self.op, self.value)

        return s

    def generate(self):
        ops = {
            'add': insVectorAdd,
            'sub': insVectorSub,
            'mul': insVectorMul,
            'div': insVectorDiv,
            'mod': insVectorMod,
        }

        return ops[self.op](self.target.generate(), self.value.generate())

class irAssign(IR):
    def __init__(self, target, value, **kwargs):
        super(irAssign, self).__init__(**kwargs)
        self.target = target
        self.value = value
        
    def __str__(self):
        if self.target.length > 1:
            s = '%s =(vector) %s' % (self.target, self.value)

        else:
            s = '%s = %s' % (self.target, self.value)

        return s

    def generate(self):
        if self.target.length == 1:    
            return insMov(self.target.generate(), self.value.generate())

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

    def generate(self):        
        params = [a.generate() for a in self.params]

        # call func
        call_ins = insLibCall(self.target, self.result, params)

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
    

class irJumpLessPreInc(IR):
    def __init__(self, target, op1, op2, **kwargs):
        super(irJumpLessPreInc, self).__init__(**kwargs)        
        self.target = target
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        s = 'JMP (%s++) < %s -> %s' % (self.op1, self.op2, self.target.name)

        return s    

    def generate(self):
        return insJmpIfLessThanPreInc(self.op1.generate(), self.op2.generate(), self.target.generate())

class irAssert(IR):
    def __init__(self, value, **kwargs):
        super(irAssert, self).__init__(**kwargs)        
        self.value = value

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

    def generate(self):
        indexes = [i.generate() for i in self.indexes]

        return insIndex(self.result.generate(), self.target.generate(), indexes)


class irIndexLoad(IR):
    def __init__(self, result, address, **kwargs):
        super(irIndexLoad, self).__init__(**kwargs)        
        self.result = result
        self.address = address

    def __str__(self):
        s = '%s = *%s' % (self.result, self.address)

        return s    

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

    def generate(self):
        return insIndirectStore(self.value.generate(), self.address.generate())


class Builder(object):
    def __init__(self):
        self.funcs = {}
        self.locals = {}
        self.globals = {}
        self.objects = {}
        self.labels = {}

        self.data_table = []

        self.loop_top = None
        self.loop_end = None

        self.next_temp = 0

        self.compound_lookup = []

        self.current_func = None

        self.data_types = {
            'i32': irVar_i32,
            'f16': irVar_f16,
            'addr': irAddress,
        }

        self.record_types = {
        }

        # optimizations
        self.optimizations = {
            'fold_constants': False
        }

        # make sure we always have 0 const
        self.add_const(0, lineno=0)

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

        s += 'Functions:\n'
        for func in self.funcs.values():
            s += '%d\t%s\n' % (func.lineno, func)

        return s

    def add_type(self, name, data_type, lineno=None):
        if name in self.data_types:
            raise SyntaxError("Type '%s' already defined" % (name), lineno=lineno)

        self.data_types[name] = data_type

    def create_record(self, name, fields, lineno=None):
        new_fields = {}
        for field_name, field in fields.items():
            new_fields[field_name] = self.build_var(field_name, field['type'], field['dimensions'], lineno=lineno)

        ir = irRecord(name, name, new_fields, lineno=lineno)

        self.record_types[name] = ir

    def get_type(self, name, lineno=None):
        if name not in self.data_types:

            # try records
            if name not in self.record_types:
                raise SyntaxError("Type '%s' not defined" % (name), lineno=lineno)

            return self.record_types[name]

        return self.data_types[name]

    def build_var(self, name, data_type, dimensions=[], lineno=None):
        data_type = self.get_type(data_type, lineno=lineno)

        if len(dimensions) == 0:
            ir = data_type(name, dimensions=dimensions, lineno=lineno)

        else:
            ir = irArray(name, data_type(name, lineno=lineno), dimensions=dimensions, lineno=lineno)

        return ir

    def add_global(self, name, data_type='i32', dimensions=[], lineno=None):
        if name in self.globals:
            return self.globals[name]

        ir = self.build_var(name, data_type, dimensions, lineno=lineno)
        
        try:   
            for v in ir:
                self.globals[v.name] = v

        except TypeError:
            self.globals[name] = ir

        return ir

    def add_local(self, name, data_type='i32', dimensions=[], lineno=None):
        # check if this is already in the globals
        if name in self.globals:
            return self.globals[name]

        if name in self.locals[self.current_func]:
            return self.locals[self.current_func][name]

        ir = self.build_var(name, data_type, dimensions, lineno=lineno)

        try:
            for v in ir:
                self.locals[self.current_func][v.name] = v

        except TypeError:
            self.locals[self.current_func][name] = ir

        return ir

    def get_var(self, name, lineno=None):
        if name in self.globals:
            return self.globals[name]

        try:
            return self.locals[self.current_func][name]

        except KeyError:
            raise SyntaxError("Variable '%s' not declared" % (name), lineno=lineno)

    def get_obj_var(self, obj_name, attr, lineno=None):
        var_name = '%s.%s' % (obj_name, attr)

        if obj_name in self.globals:
            var = self.globals[obj_name]

        else:
            try:
                var = self.locals[self.current_func][obj_name]

            except KeyError:
                raise SyntaxError("Object '%s' not declared" % (obj_name), lineno=lineno)


        if attr not in var.fields:
            raise SyntaxError("Attribute '%s' not declared" % (attr), lineno=lineno)            

        return var.fields[attr]

    def add_const(self, name, data_type='i32', length=1, lineno=None):
        if name in self.globals:
            return self.globals[name]

        ir = irConst(name, data_type, length, lineno=lineno)

        self.globals[name] = ir

        return ir

    def add_temp(self, data_type='i32', lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = self.build_var(name, data_type, [], lineno=lineno)
        self.locals[self.current_func][name] = ir

        return ir

    def add_func_arg(self, func, arg):
        # if arg.name in self.globals:
            # raise SyntaxError("Argument name '%s' already declared as global" % (arg.name), lineno=func.lineno)

        arg.name = '$%s.%s' % (func.name, arg.name)

        func.params.append(arg)

    def func(self, *args, **kwargs):
        func = irFunc(*args, **kwargs)
        self.funcs[func.name] = func
        self.locals[func.name] = {}
        self.current_func = func.name
        self.next_temp = 0 

        return func

    def append_node(self, node):
        self.funcs[self.current_func].append(node)

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

        # if either type is fixed16, we do the whole thing as fixed16.
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

    def assign(self, target, value, lineno=None):        
        if isinstance(target, irAddress):
            self.store_indirect(target, value, lineno=lineno)

        else:
            # check target type
            if target.type != value.type:
                ir = irConvertType(target, value, lineno=lineno)

            else:
                ir = irAssign(target, value, lineno=lineno)

            self.append_node(ir)

            return ir

    def augassign(self, op, target, value, lineno=None):
        if isinstance(target, irAddress):
            result = self.load_indirect(target, lineno=lineno)

        else:
            result = target

        if target.length == 1:
            # if so, we can replace with a binop and assign
            result = self.binop(op, result, value, lineno=lineno)
            ir = self.assign(target, result, lineno=lineno)

        else:
            ir = irAugAssign(op, target, value, lineno=lineno)        
            self.append_node(ir)

        return ir

    def load_indirect(self, address, lineno=None):
        result = self.add_temp(lineno=lineno)
        ir = irIndexLoad(result, address, lineno=lineno)
    
        self.append_node(ir)

        return result  

    def store_indirect(self, address, value, lineno=None):
        ir = irIndexStore(address, value, lineno=lineno)
    
        self.append_node(ir)

        return ir
        
    def call(self, target, params, lineno=None):
        result = self.add_temp(lineno=lineno)

        try:
            args = self.funcs[target].params
            ir = irCall(target, params, args, result, lineno=lineno)

        except KeyError:
            ir = irLibCall(target, params, result, lineno=lineno)
        
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

        branch = irBranchZero(test, else_label, lineno=lineno)
        self.append_node(branch)

        return body_label, else_label, end_label

    def position_label(self, label):
        self.append_node(label)


    def begin_for(self, iterator, lineno=None):
        begin_label = self.label('for.begin', lineno=lineno) # we don't actually need this label, but it is helpful for reading the IR
        self.position_label(begin_label)
        top_label = self.label('for.top', lineno=lineno)
        continue_label = self.label('for.cont', lineno=lineno)
        end_label = self.label('for.end', lineno=lineno)

        self.loop_top = continue_label
        self.loop_end = end_label

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

        self.loop_top = None
        self.loop_end = None

    def jump(self, target, lineno=None):
        ir = irJump(target, lineno=lineno)
        self.append_node(ir)

    def loop_break(self, lineno=None):
        assert self.loop_end != None
        self.jump(self.loop_end, lineno=lineno)


    def loop_continue(self, lineno=None):
        assert self.loop_top != None    
        self.jump(self.loop_top, lineno=lineno)

    def assertion(self, test, lineno=None):
        ir = irAssert(test, lineno=lineno)

        self.append_node(ir)

    def lookup_attribute(self, obj, attr, lineno=None):
        if len(self.compound_lookup) == 0:
            self.compound_lookup.append(obj)  

        self.compound_lookup.append(irField(attr, lineno=lineno))

    def lookup_subscript(self, target, index, lineno=None):
        if len(self.compound_lookup) == 0:
            self.compound_lookup.append(target)  

        self.compound_lookup.append(index)  

    def resolve_lookup(self, load=True, lineno=None):    
        target = self.compound_lookup.pop(0)
        result = self.add_temp(lineno=lineno, data_type='addr')

        ir = irIndex(result, target, lineno=lineno)

        indexes = []

        for index in self.compound_lookup:
            indexes.append(index)
        
        self.compound_lookup = []


        ir.indexes = indexes

        self.append_node(ir)

        if load:
            return self.load_indirect(result, lineno=lineno)
        
        return result

    def generic_object(self, name, data_type, args, kw, lineno=None):
        print name, data_type, args, kw

    def _fold_constants(self, op, left, right, lineno):
        val = None

        if op == 'eq':
            val = left.name == right.name

        elif op == 'neq':
            val = left.name != right.name

        elif op == 'gt':
            val = left.name > right.name

        elif op == 'gte':
            val = left.name >= right.name

        elif op == 'lt':
            val = left.name < right.name

        elif op == 'lte':
            val = left.name <= right.name

        elif op == 'logical_and':
            val = left.name and right.name

        elif op == 'logical_or':
            val = left.name or right.name

        elif op == 'add':
            val = left.name + right.name

        elif op == 'sub':
            val = left.name - right.name

        elif op == 'mult':
            val = left.name * right.name

        elif op == 'div':
            val = left.name / right.name

        elif op == 'mod':
            val = left.name % right.name

        assert val != None

        # make sure we only emit integers
        return self.add_const(int(val), lineno=lineno)

    def allocate(self):
        self.data_table = []

        ret_var = irVar('$return', lineno=0)
        ret_var.addr = 0

        self.data_table.append(ret_var)

        addr = 1
        for i in self.globals.values():
            i.addr = addr
            addr += i.length

            self.data_table.append(i)

        for func_name, local in self.locals.items():
            for i in local.values():
                i.addr = addr
                addr += i.length

                # assign func name to var
                i.name = '%s.%s' % (func_name, i.name)

                self.data_table.append(i)

        return self.data_table

    def print_data_table(self, data):
        print "DATA: "
        for i in data:
            print '\t%3d: %s' % (i.addr, i)

    def print_instructions(self, instructions):
        print "INSTRUCTIONS: "
        i = 0
        for ins in instructions:
            s = '\t%3d: %s' % (i, str(ins))
            print s
            i += 1

    def generate_instructions(self):
        # check if there is no loop function
        if 'loop' not in self.funcs:
            self.func('loop', lineno=0)
            self.ret(self.get_var(0), lineno=0)

        ins = []

        for func in self.funcs.values():
            ins.extend(func.generate())


        return ins



class VM(object):
    def __init__(self, code, data, pix_size_x=4, pix_size_y=4):
        self.code = code
        self.data = data

        # init memory
        self.memory = []

        for var in data:
            if isinstance(var, irConst):
                self.memory.append(var.name)

            else:
                for i in xrange(var.length):
                    self.memory.append(0)

    def dump_registers(self):
        registers = {}
        for var in self.data:
            # don't dump consts
            if isinstance(var, irConst):
                continue

            if isinstance(var, irArray):
                # print var
                value = []
                addr = var.addr
                for i in xrange(var.length):
                    value.append(self.memory[addr])
                    addr += 1

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

        # scan code stream and get offsets for all functions and labels
        for i in xrange(len(self.code)):
            ins = self.code[i]
            if isinstance(ins, insFunction) or isinstance(ins, insLabel):
                offsets[ins.name] = i


        # setup PC
        try:
            pc = offsets[func]
        
        except KeyError:
            raise VMRuntimeError("Function '%s' not found" % (func))

        while True:
            cycles += 1

            ins = self.code[pc]

            # print cycles, pc, ins

            pc += 1

            try:
                ret_val = ins.execute(self.memory)

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





