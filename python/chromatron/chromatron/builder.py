
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


    def analyze_blocks(self):
        for func in self.funcs.values():
            func.analyze_blocks()


    def add_const(self, value, data_type=None, lineno=None):
        name = str(value)

        if name in self.consts:
            return self.consts[name]

        ir = irConst(name, data_type, lineno=lineno)

        self.consts[name] = ir

        return ir
    


    def finish_module(self):
        # clean up stuff after first pass is done

        return self


