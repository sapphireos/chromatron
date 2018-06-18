

def params_to_string(params):
    s = ''

    for p in params:
        s += '%s %s,' % (p.type, p.name)

    # strip last comma
    s = s[:len(s) - 1]

    return s

class IR(object):
    def __init__(self):
        pass

class irVar(IR):
    def __init__(self, name, type='i32', length=1):
        self.name = name
        self.type = type
        self.length = length

        assert length > 0

    def __str__(self):
        return "Var (%s, %s, %d)" % (self.name, self.type, self.length)

class irFunc(IR):
    def __init__(self, name, ret_type='i32', params=None, body=None):
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
            s += '\t\t%s\n' % (node)

        return s

class irReturn(IR):
    def __init__(self, ret_var):
        self.ret_var = ret_var

    def __str__(self):
        return "Return %s" % (self.ret_var)

class irNop(IR):
    def __str__(self):
        return "NOP" 

class irBinop(IR):
    def __init__(self, result, op, left, right):
        self.result = result
        self.op = op
        self.left = left
        self.right = right
    
    def __str__(self):
        s = '%s = %s %s %s' % (self.result, self.left, self.op, self.right)

        return s

class irAssign(IR):
    def __init__(self, target, value):
        self.target = target
        self.value = value
        
    def __str__(self):
        s = '%s = %s' % (self.target, self.value)

        return s

class irCall(IR):
    def __init__(self, target, params, result):
        self.target = target
        self.params = params
        self.result = result

    def __str__(self):
        params = params_to_string(self.params)
        s = 'CALL %s(%s) -> %s' % (self.target, params, self.result)

        return s


class Builder(object):
    def __init__(self):
        self.funcs = {}
        self.locals = {}
        self.temps = {}
        self.globals = {}
        self.objects = {}

        self.next_temp = 0

        self.current_func = None

    def __str__(self):
        s = "FX IR:\n"

        s += 'Globals:\n'
        for i in self.globals.values():
            s += '\t%s\n' % i

        s += 'Locals:\n'
        for fname in sorted(self.locals.keys()):
            if len(self.locals[fname].values()) > 0:
                s += '\t%s\n' % (fname)

                for l in sorted(self.locals[fname].values()):
                    s += '\t\t%s\n' % (l)

        s += 'Temps:\n'
        for fname in sorted(self.temps.keys()):
            if len(self.temps[fname].values()) > 0:
                s += '\t%s\n' % (fname)

                for l in sorted(self.temps[fname].values()):
                    s += '\t\t%s\n' % (l)

        s += 'Functions:\n'
        for func in self.funcs.values():
            s += '\t%s\n' % func

        return s

    def add_global(self, name, type, length):
        ir = irVar(name, type, length)
        self.globals[name] = ir

        return ir

    def add_local(self, name, type, length):
        ir = irVar(name, type, length)
        self.locals[self.current_func][name] = ir

        return ir

    def add_temp(self, type='i32'):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        var = irVar(name, type)
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

    def ret(self, value):
        ir = irReturn(value)

        self.append_node(ir)

        return ir

    def nop(self):
        ir = irNop()

        self.append_node(ir)

        return ir

    def binop(self, op, left, right):
        result = self.add_temp()

        ir = irBinop(result, op, left, right)

        self.append_node(ir)

        return result

    def assign(self, target, value):
        ir = irAssign(target, value)

        self.append_node(ir)

        return ir

    def call(self, target, params):
        result = self.add_temp()

        ir = irCall(target, params, result)
    
        self.append_node(ir)        

        return result




