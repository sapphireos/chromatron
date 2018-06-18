

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

class irAssign(IR):
    def __init__(self, target, value, **kwargs):
        super(irAssign, self).__init__(**kwargs)
        self.target = target
        self.value = value
        
    def __str__(self):
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

class Builder(object):
    def __init__(self):
        self.funcs = {}
        self.locals = {}
        self.temps = {}
        self.globals = {}
        self.objects = {}
        self.labels = {}

        self.next_temp = 0

        self.current_func = None

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

    def add_global(self, name, type='i32', length=1, lineno=None):
        if name in self.globals:
            return self.globals[name]

        ir = irVar(name, type, length, lineno=lineno)
        self.globals[name] = ir

        return ir

    def add_local(self, name, type='i32', length=1, lineno=None):
        if name in self.locals[self.current_func]:
            return self.locals[self.current_func][name]

        ir = irVar(name, type, length, lineno=lineno)
        self.locals[self.current_func][name] = ir

        return ir

    def add_temp(self, type='i32', lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        var = irVar(name, type, lineno=lineno)
        self.temps[self.current_func][name] = var

        return var

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
        result = self.add_temp(lineno=lineno)

        ir = irBinop(result, op, left, right, lineno=lineno)

        self.append_node(ir)

        return result

    def assign(self, target, value, lineno=None):
        ir = irAssign(target, value, lineno=lineno)

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
        top_label = self.label('for.top', lineno=lineno)
        continue_label = self.label('for.cont', lineno=lineno)
        end_label = self.label('for.end', lineno=lineno)

        # set up iterator code (init to -1, as first pass will increment before the body) 
        init_value = self.add_local('-1', lineno=lineno)
        ir = irAssign(iterator, init_value, lineno=lineno)
        self.append_node(ir)

        ir = irJump(continue_label, lineno=lineno)
        self.append_node(ir)

        return top_label, continue_label, end_label

    def end_for(self, iterator, stop, top, lineno=None):
        ir = irJumpLessPreInc(top, iterator, stop, lineno=lineno)
        self.append_node(ir)




