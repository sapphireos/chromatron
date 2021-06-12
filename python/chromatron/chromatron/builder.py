
from .ir2 import *


class Builder(object):
    def __init__(self, script_name='fx_script', source=[]):
        
        self.script_name = script_name

        # load source code for debug
        source_code = source
        if isinstance(source_code, str):
            source_code = source_code.splitlines()

        set_source_code(source_code)

        self.funcs = {}
        self.scope_depth = 0
        self.labels = {}
        self.globals = {}
        self.consts = {}

        self.next_temp = 0


    def __str__(self):
        s = "FX IR:\n"

        s += 'Globals:\n'
        for i in list(self.globals.values()):
            s += '%d\t%s\n' % (i.lineno, i)

        s += 'Consts:\n'
        for i in list(self.consts.values()):
            s += '%d\t%s\n' % (i.lineno, i)
        
        # s += 'PixelArrays:\n'
        # for i in list(self.pixel_arrays.values()):
        #     s += '%d\t%s\n' % (i.lineno, i)

        s += 'Functions:\n'
        for func in list(self.funcs.values()):
            s += '%s\n' % (func)

        return s

    def append_node(self, node):
        node.scope_depth = self.scope_depth
        self.current_func.append_node(node)

    def func(self, *args, **kwargs):
        func = irFunc(*args, builder=self, **kwargs)
        self.funcs[func.name] = func
        self.current_func = func
        self.next_temp = 0
        self.scope_depth = 0

        func_label = self.label(f'function:{func.name}', lineno=kwargs['lineno'])
        self.position_label(func_label)

        return func

    def label(self, name, lineno=None):
        if name not in self.labels:
            self.labels[name] = 0

        else:
            self.labels[name] += 1

        name += '.%d' % (self.labels[name])

        ir = irLabel(name, lineno=lineno)
        return ir

    def position_label(self, label):
        self.append_node(label)


    def ret(self, value, lineno=None):
        ir = irReturn(value, lineno=lineno)

        self.append_node(ir)

        return ir

    def jump(self, target, lineno=None):
        ir = irJump(target, lineno=lineno)
        self.append_node(ir)

    def assign(self, target, value, lineno=None):     
        ir = irAssign(target, value, lineno=lineno)
            
        self.append_node(ir)

    def binop(self, op, left, right, lineno=None):
        result = self.add_temp(lineno=lineno)

        ir = irBinop(result, op, left, right, lineno=lineno)

        self.append_node(ir)

        return result

    def analyze_blocks(self):
        for func in self.funcs.values():
            func.analyze_blocks()


    def add_temp(self, data_type='i32', lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = irTemp(name, lineno=lineno)
    
        return ir

    def add_const(self, value, data_type=None, lineno=None):
        name = str(value)

        if name in self.consts:
            return self.consts[name]

        ir = irConst(name, data_type, lineno=lineno)

        self.consts[name] = ir

        return ir
    
    def declare_var(self, name, data_type='i32', dimensions=[], keywords={}, is_global=False, lineno=None):
        if is_global:
            return self.add_global(name, data_type, dimensions, keywords=keywords, lineno=lineno)

        else:
            if len(keywords) > 0:
                raise SyntaxError("Cannot specify keywords for local variables", lineno=lineno)

            var = irVar(name, data_type, lineno=lineno)
            
            ir = irDefine(var, lineno=lineno)

            self.append_node(ir)

            return var

    def get_var(self, name, lineno=None):
        return irVar(name, lineno=lineno)

    def finish_module(self):
        # clean up stuff after first pass is done

        return self

    def ifelse(self, test, lineno=None):
        body_label = self.label('if.then', lineno=lineno)
        else_label = self.label('if.else', lineno=lineno)
        end_label = self.label('if.end', lineno=lineno)

        assert not isinstance(test, irBinop)

        branch = irBranch(test, body_label, else_label, lineno=lineno)
        self.append_node(branch)

        self.scope_depth += 1

        return body_label, else_label, end_label

    def end_if(self, end_label, lineno=None):
        self.jump(end_label, lineno=lineno)

    def do_else(self, lineno=None):
        pass

    def end_ifelse(self, end_label, lineno=None):
        self.jump(end_label, lineno=lineno)
        self.scope_depth -= 1
        self.position_label(end_label)