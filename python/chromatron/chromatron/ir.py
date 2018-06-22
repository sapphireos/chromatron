
from copy import deepcopy

class SyntaxError(Exception):
    def __init__(self, message='', lineno=None):
        self.lineno = lineno

        message += ' (line %d)' % (self.lineno)

        super(SyntaxError, self).__init__(message)


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

class irVar(IR):
    def __init__(self, name, type='i32', dimensions=[], **kwargs):
        super(irVar, self).__init__(**kwargs)
        self.name = name
        self.type = type
        self.length = 1
        self.dimensions = dimensions

    def __str__(self):
        return "Var (%s, %s)" % (self.name, self.type)

class irVar_i32(irVar):
    def __init__(self, *args, **kwargs):
        super(irVar_i32, self).__init__(*args, **kwargs)
        self.type = 'i32'
        
class irConst(irVar):
    def __str__(self):
        return "Const (%s, %s)" % (self.name, self.type)

class irArray(irVar):
    def __init__(self, *args, **kwargs):
        super(irArray, self).__init__(*args, **kwargs)        

        self.length = self.dimensions[0] * self.type.length
        for i in xrange(len(self.dimensions) - 1):
            self.length *= self.dimensions[i + 1]

    def __str__(self):
        return "Array (%s, %s, %d)" % (self.name, self.type.type, self.length)       

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

class irReturn(IR):
    def __init__(self, ret_var, **kwargs):
        super(irReturn, self).__init__(**kwargs)
        self.ret_var = ret_var

    def __str__(self):
        return "RET %s" % (self.ret_var)

class irNop(IR):
    def __str__(self, **kwargs):
        return "NOP" 

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


class irAugAssign(IR):
    def __init__(self, op, target, value, **kwargs):
        super(irAugAssign, self).__init__(**kwargs)
        self.op = op
        self.target = target
        self.value = value
        
    def __str__(self):
        s = '%s = %s %s(vector) %s' % (self.target, self.target, self.op, self.value)

        return s

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


class irClr(IR):
    def __init__(self, target, **kwargs):
        super(irClr, self).__init__(**kwargs)
        self.target = target
        
    def __str__(self):
        s = '%s = 0' % (self.target)

        return s

class irCall(IR):
    def __init__(self, target, params, result, **kwargs):
        super(irCall, self).__init__(**kwargs)
        self.target = target
        self.params = params
        self.result = result

    def __str__(self):
        params = params_to_string(self.params)
        s = 'CALL %s(%s) -> %s' % (self.target, params, self.result)

        return s

class irLabel(IR):
    def __init__(self, name, **kwargs):
        super(irLabel, self).__init__(**kwargs)        
        self.name = name

    def __str__(self):
        s = 'LABEL %s' % (self.name)

        return s


class irBranch(IR):
    def __init__(self, value, target, **kwargs):
        super(irBranch, self).__init__(**kwargs)        
        self.value = value
        self.target = target


class irBranchZero(irBranch):
    def __init__(self, *args, **kwargs):
        super(irBranchZero, self).__init__(*args, **kwargs)        

    def __str__(self):
        s = 'BR Z %s -> %s' % (self.value, self.target.name)

        return s    

class irBranchNotZero(irBranch):
    def __init__(self, *args, **kwargs):
        super(irBranchNotZero, self).__init__(*args, **kwargs)        

    def __str__(self):
        s = 'BR NZ %s -> %s' % (self.value, self.target.name)

        return s    

class irJump(IR):
    def __init__(self, target, **kwargs):
        super(irJump, self).__init__(**kwargs)        
        self.target = target

    def __str__(self):
        s = 'JMP -> %s' % (self.target.name)

        return s    

class irJumpLessPreInc(IR):
    def __init__(self, target, op1, op2, **kwargs):
        super(irJumpLessPreInc, self).__init__(**kwargs)        
        self.target = target
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        s = 'JMP (%s++) < %s -> %s' % (self.op1, self.op2, self.target.name)

        return s    

class irAssert(IR):
    def __init__(self, value, **kwargs):
        super(irAssert, self).__init__(**kwargs)        
        self.value = value

    def __str__(self):
        s = 'ASSERT %s' % (self.value)

        return s   

class irLookupIndex(IR):
    def __init__(self, result, array, index, **kwargs):
        super(irLookupIndex, self).__init__(**kwargs)        
        self.result = result
        self.array = array
        self.index = index

    def __str__(self):
        s = '%s = INDEX %s[%s]' % (self.result, self.array, self.index)

        return s   


# class irIndex(IR):
#     def __init__(self, target, indexes, **kwargs):
#         super(irIndex, self).__init__(**kwargs)        
#         self.target = target
#         self.indexes = indexes

#     def __str__(self):
#         indexes = ''
#         for i in self.indexes:
#             indexes += '[%s]' % (i.name)

#         s = '%s%s' % (self.target, indexes)

#         return s     

class irIndexLoad(IR):
    def __init__(self, result, address, **kwargs):
        super(irIndexLoad, self).__init__(**kwargs)        
        self.result = result
        self.address = address

    def __str__(self):
        s = '%s = *%s' % (self.result, self.address)

        return s    

class irIndexStore(IR):
    def __init__(self, address, value, **kwargs):
        super(irIndexStore, self).__init__(**kwargs)        
        self.address = address
        self.value = value

    def __str__(self):
        s = '*%s = %s' % (self.address, self.value)

        return s    

# class irIndexStore(IR):
#     def __init__(self, target, indexes, value, **kwargs):
#         super(irIndexStore, self).__init__(**kwargs)        
#         self.target = target
#         self.indexes = indexes
#         self.value = value

#     def __str__(self):
#         indexes = ''
#         for i in self.indexes:
#             indexes += '[%s]' % (i.name)

#         s = '%s%s = %s' % (self.target, indexes, self.value)

#         return s    


class Builder(object):
    def __init__(self):
        self.funcs = {}
        self.locals = {}
        self.temps = {}
        self.globals = {}
        self.objects = {}
        self.labels = {}

        self.loop_top = None
        self.loop_end = None

        self.next_temp = 0

        self.current_func = None

        self.data_types = {
            'i32': irVar_i32,
        }

        self.record_types = {
        }

        # optimizations
        self.optimizations = {
            'fold_constants': False
        }


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

        s += 'Temps:\n'
        for fname in sorted(self.temps.keys()):
            if len(self.temps[fname].values()) > 0:
                s += '\t%s\n' % (fname)

                for l in sorted(self.temps[fname].values()):
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

    def add_const(self, name, type='i32', length=1, lineno=None):
        if name in self.locals[self.current_func]:
            return self.locals[self.current_func][name]

        ir = irConst(name, type, length, lineno=lineno)
        self.locals[self.current_func][name] = ir

        return ir

    def add_temp(self, data_type='i32', lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = self.build_var(name, data_type, [], lineno=lineno)
        self.temps[self.current_func][name] = ir

        return ir

    def add_func_arg(self, func, arg):
        if arg.name in self.globals:
            raise SyntaxError("Argument name '%s' already declared as global" % (arg.name), lineno=func.lineno)

        func.params.append(arg)

    def func(self, *args, **kwargs):
        func = irFunc(*args, **kwargs)
        self.funcs[func.name] = func
        self.locals[func.name] = {}
        self.temps[func.name] = {}
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

        result = self.add_temp(lineno=lineno)
        ir = irBinop(result, op, left, right, lineno=lineno)

        self.append_node(ir)

        return result

    def assign(self, target, value, lineno=None):        
        ir = irAssign(target, value, lineno=lineno)

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
        
    def augassign(self, op, target, value, lineno=None):
        # check if storing to indexed location
        if isinstance(target, irIndex): 
            result = self.add_temp(lineno=lineno)
            ir = irIndexLoad(result, target.target, target.indexes, lineno=lineno)
            self.append_node(ir)

            result = self.binop(op, result, value, lineno=lineno)

            ir = self.assign(target, result, lineno=lineno)

        # check if scalar
        elif target.length == 1:
            # if so, we can replace with a binop and assign
            result = self.binop(op, target, value, lineno=lineno)
            ir = self.assign(target, result, lineno=lineno)

        else:
            ir = irAugAssign(op, target, value, lineno=lineno)        
            self.append_node(ir)

        return ir

    def call(self, target, params, lineno=None):
        result = self.add_temp(lineno=lineno)

        ir = irCall(target, params, result, lineno=lineno)
    
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

        branch = irBranchZero(test, else_label, lineno=lineno)
        self.append_node(branch)

        return body_label, else_label

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
        init_value = self.add_const('-1', lineno=lineno)
        ir = irAssign(iterator, init_value, lineno=lineno)
        self.append_node(ir)

        ir = irJump(continue_label, lineno=lineno)
        self.append_node(ir)

        return top_label, continue_label, end_label

    def end_for(self, iterator, stop, top, lineno=None):
        ir = irJumpLessPreInc(top, iterator, stop, lineno=lineno)
        self.append_node(ir)

        self.loop_top = None
        self.loop_end = None

    def loop_break(self, lineno=None):
        assert self.loop_end != None

        ir = irJump(self.loop_end, lineno=lineno)
        self.append_node(ir)

    def loop_continue(self, lineno=None):
        assert self.loop_top != None    

        ir = irJump(self.loop_top, lineno=lineno)
        self.append_node(ir)

    def assertion(self, test, lineno=None):
        ir = irAssert(test, lineno=lineno)

        self.append_node(ir)

    def lookup_index(self, array, index, lineno=None):
        result = self.add_temp(lineno=lineno)
        ir = irLookupIndex(result, array, index, lineno=lineno)

        self.append_node(ir)

        return result

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
