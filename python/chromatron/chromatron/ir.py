
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
    def __init__(self, name, type='i32', length=1, **kwargs):
        super(irVar, self).__init__(**kwargs)
        self.name = name
        self.type = type
        self.length = length

        assert length > 0

    def __str__(self):
        return "Var (%s, %s, %d)" % (self.name, self.type, self.length)

class irVari32(irVar):
    def __init__(self, *args, **kwargs):
        super(irVari32, self).__init__(*args, **kwargs)
        self.type = 'i32'
        
class irConst(irVar):
    def __str__(self):
        return "Const (%s, %s, %d)" % (self.name, self.type, self.length)

class irRecord(irVar):
    def __init__(self, *args, **kwargs):
        super(irRecord, self).__init__(*args, **kwargs)
            
        self.length = 0
        for field in self.fields.values():
            self.length += field['length']

    def __str__(self):
        return "%s (%s, %d)" % (self.name, self.typename, self.length)

    @classmethod
    def create(cls, name, fields, lineno):
        new_class = type(name, (cls,), {'typename': name, 'fields': fields, 'lineno': lineno})

        globals()[name] = new_class

        return new_class

    def get_field(self, field):
        return self.fields[field]

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


class irIndex(IR):
    def __init__(self, target, index, **kwargs):
        super(irIndex, self).__init__(**kwargs)        
        self.target = target
        self.index = index

    def __str__(self):
        s = '%s[%s]' % (self.target, self.index)

        return s     

class irIndexLoad(IR):
    def __init__(self, result, target, index, **kwargs):
        super(irIndexLoad, self).__init__(**kwargs)        
        self.result = result
        self.target = target
        self.index = index

    def __str__(self):
        s = '%s = %s[%s]' % (self.result, self.target, self.index)

        return s    

class irIndexStore(IR):
    def __init__(self, target, value, **kwargs):
        super(irIndexStore, self).__init__(**kwargs)        
        self.target = target
        self.value = value

    def __str__(self):
        s = '%s = %s' % (self.target, self.value)

        return s    


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
            'i32': irVari32,
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
        return irRecord.create(name, fields, lineno=lineno)

    def get_type(self, name, lineno=None):
        if name not in self.data_types:
            raise SyntaxError("Type '%s' not defined" % (name), lineno=lineno)

        return self.data_types[name]

    def build_var(self, name, data_type, length, lineno=None):
        data_type = self.get_type(data_type, lineno=lineno)
        ir = data_type(name, data_type, length, lineno=lineno)

        print type(ir)

        return ir

    def add_global(self, name, data_type='i32', length=1, lineno=None):
        if name in self.globals:
            return self.globals[name]

        ir = self.build_var(name, data_type, length, lineno=lineno)
        self.globals[name] = ir

        return ir

    def add_local(self, name, data_type='i32', length=1, lineno=None):
        # check if this is already in the globals
        if name in self.globals:
            return self.globals[name]

        if name in self.locals[self.current_func]:
            return self.locals[self.current_func][name]

        ir = self.build_var(name, data_type, length, lineno=lineno)
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
        if obj_name in self.globals:
            obj = self.globals[obj_name]

        else:
            try:
                obj = self.locals[self.current_func][obj_name]

            except KeyError:
                raise SyntaxError("Object '%s' not declared" % (obj_name), lineno=lineno)

        # get field
        try:
            field = obj.get_field(attr)

        except KeyError:
            raise SyntaxError("Attribute '%s' not found in object '%s'" % (attr, obj_name), lineno=lineno)


        var = self.get_type(field['type'])(attr, length=field['length'], lineno=lineno)

        return var

    def add_const(self, name, type='i32', length=1, lineno=None):
        if name in self.locals[self.current_func]:
            return self.locals[self.current_func][name]

        ir = irConst(name, type, length, lineno=lineno)
        self.locals[self.current_func][name] = ir

        return ir

    def add_temp(self, data_type='i32', lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = self.build_var(name, data_type, 1, lineno=lineno)
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
        # check if assigning to an indexed location
        if isinstance(target, irIndex): 
            ir = irIndexStore(target, value, lineno=lineno)

        else:
            ir = irAssign(target, value, lineno=lineno)

        self.append_node(ir)

        return ir

    def augassign(self, op, target, value, lineno=None):
        # check if storing to indexed location
        if isinstance(target, irIndex): 
            result = self.add_temp(lineno=lineno)
            ir = irIndexLoad(result, target.target, target.index, lineno=lineno)
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

    def index(self, target, index, load=True, lineno=None):
        if load:
            result = self.add_temp(lineno=lineno)
            ir = irIndexLoad(result, target, index, lineno=lineno)
            self.append_node(ir)

            return result

        else:
            ir = irIndex(target, index, lineno=lineno)
            
            return ir            


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
