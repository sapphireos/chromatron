
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
        self.named_consts = {}
        self.structs = {}
        self.strings = {}

        self.next_temp = 0

        self.next_loop = 0

        self.loop = []
        self.loop_preheader = []
        self.loop_header = []
        self.loop_top = []
        self.loop_body = []
        self.loop_end = []

        self.locals = {}
        self.refs = {}
        self.current_lookup = []
        self.current_func = None

    def __str__(self):
        s = "FX IR Builder:\n"

        s += 'Globals:\n'
        for i in list(self.globals.values()):
            s += '%d\t%s\n' % (i.lineno, i)

        # s += 'PixelArrays:\n'
        # for i in list(self.pixel_arrays.values()):
        #     s += '%d\t%s\n' % (i.lineno, i)

        s += 'Functions:\n'
        for func in list(self.funcs.values()):
            s += '%s\n' % (func)

        return s

    ###################################
    # Variables
    ###################################
    def add_temp(self, data_type=None, lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = irVar(name, datatype=data_type, lineno=lineno)
        ir.is_temp = True

        return ir
    
    def add_ref(self, target, lookups=[], lineno=None):
        ir = irVar(target.name, lineno=lineno)
        ir.lookups = lookups
        ir.is_ref = True
        ir.ref = target
        
        return ir    
    
    def add_const(self, value, data_type=None, lineno=None):
        if value is True:
            value = 1
        elif value is False:
            value = 0

        name = str(value)
    
        if name in self.current_func.consts:
            return copy(self.current_func.consts[name])

        if isinstance(value, int):
            data_type = 'i32'

        elif isinstance(value, float):
            data_type = 'f16'
            
        else:
            assert False

        ir = irVar(name, data_type, lineno=lineno)
        ir.is_const = True

        self.current_func.consts[name] = ir

        return ir

    def add_named_const(self, name, value, data_type=None, lineno=None):
        if value is True:
            value = 1
        elif value is False:
            value = 0

        if name in self.named_consts:
            return copy(self.named_consts[name])

        if isinstance(value, int):
            data_type = 'i32'

        elif isinstance(value, float):
            data_type = 'f16'
            
        else:
            assert False

        ir = irVar(str(value), data_type, lineno=lineno)
        ir.is_const = True

        self.named_consts[name] = ir

        return ir
    
    def build_var(self, name, data_type=None, dimensions=[], keywords={}, lineno=None):
        if data_type in PRIMITIVE_TYPES:
            return irVar(name, data_type, dimensions, lineno=lineno)

        elif data_type == 'str':
            # var = irString(name, lineno=lineno, **keywords) 

            var = irVar(name, data_type, dimensions, lineno=lineno)
            var.is_ref = True
            var.ref = self.strings[keywords['init_val']]

            return var

        elif data_type in self.structs:
            return self.structs[data_type](name, dimensions, lineno=lineno)

        raise CompilerFatal(f'Unknown type {data_type}')

    def declare_var(self, name, data_type='i32', dimensions=[], keywords={}, is_global=False, lineno=None):
        if data_type not in PRIMITIVE_TYPES and data_type not in self.structs and data_type != 'str':
            raise SyntaxError(f'Type {data_type} is unknown', lineno=lineno)

        var = self.build_var(name, data_type, dimensions, keywords=keywords, lineno=lineno)

        if is_global:
            if name in self.globals:
                # return self.globals[name]
                raise SyntaxError("Global variable '%s' already declared" % (name), lineno=lineno)

            var.is_global = True
            self.globals[name] = var

        else:
            # if len(keywords) > 0:
            #     raise SyntaxError("Cannot specify keywords for local variables", lineno=lineno)
            
            self.locals[name] = var

            ir = irDefine(var, lineno=lineno)

            self.append_node(ir)

        if var.is_ref:
            self.refs[var.basename] = var

        return var

    def get_var(self, name, lineno=None):
        if name in self.named_consts:
            return self.named_consts[name]

        if name in self.globals:
            return self.add_ref(self.globals[name], lineno=lineno)

        # elif name in self.refs:
        #     return self.add_ref(self.refs[name], lineno=lineno)

        if name in self.locals:
            return copy(self.locals[name])

        raise CompilerFatal(f'Var not found: {name}')

    def create_struct(self, name, fields, lineno=None):
        new_fields = {}
        
        for field_name, field in list(fields.items()):
            field_type = field['type']
            field_dims = field['dimensions']

            new_fields[field_name] = irVar(field_name, field_type, lineno=lineno)
            new_fields[field_name].dimensions = field_dims


        ir = irStruct(name, name, new_fields, lineno=lineno)

        self.structs[name] = ir

    def add_string(self, string, lineno=None):
        try:
            ir = self.strings[string]

        except KeyError:
            ir = irStrLiteral(string, lineno=lineno)

            self.strings[string] = ir

        return ir

    ###################################
    # IR instructions
    ###################################

    def append_node(self, node):
        node.scope_depth = self.scope_depth
        self.current_func.append_node(node)

    @property
    def prev_node(self):
        return self.current_func.prev_node

    def nop(self, lineno=None):
        pass

    def func(self, *args, **kwargs):
        func = irFunc(*args, global_vars=self.globals, **kwargs)
        self.funcs[func.name] = func
        self.current_func = func
        self.next_temp = 0
        self.refs = {}
        self.locals = {}
        self.scope_depth = 0

        func_label = self.label(f'func:{func.name}', lineno=kwargs['lineno'])
        self.position_label(func_label)

        return func

    def add_func_arg(self, func, name, data_type='i32', dimensions=[], lineno=None):
        ir = self.build_var(name, data_type=data_type, dimensions=dimensions, lineno=lineno)
        self.locals[name] = ir
        func.params.append(ir)

        return ir

    def finish_func(self, func):
        func.next_temp = self.next_temp

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

    def call(self, func_name, params, lineno=None):
        result = self.add_temp(lineno=lineno)

        if func_name in ARRAY_FUNCS:
            if len(params) != 1:
                raise SyntaxError("Array functions take one argument", lineno=lineno)

            if func_name == 'len':
                # since arrays are fixed length, we don't need a libcall, we 
                # can just do an assignment.
                array_len = params[0].ref.count
                const = self.add_const(array_len, lineno=lineno)

                self.assign(result, const, lineno=lineno)

        return result

    def ret(self, value, lineno=None):
        ir = irReturn(value, lineno=lineno)

        self.append_node(ir)

        return ir

    def jump(self, target, lineno=None):
        ir = irJump(target, lineno=lineno)
        self.append_node(ir)

    def assign(self, target, value, lineno=None):
        if target.is_obj:
            ir = irObjectAssign(target, value, lineno=lineno)

        elif value.is_obj:
            ir = irObjectLoad(target, value, lineno=lineno)

        elif target.is_ref and target.ref.is_array and len(target.lookups) == 0:
            ir = irVectorAssign(target.ref, value, lineno=lineno)
        
        elif value.is_ref and value.ref.is_array and len(value.lookups) == 0:
            raise SyntaxError(f'Cannot vector assign from array: {value.basename} to scalar: {target.basename}', lineno=lineno)

        elif target.is_ref:
            ir = irAssign(target, value, lineno=lineno)

        elif value.is_const:
            ir = irLoadConst(target, value, lineno=lineno)

        # check if previous op is a binop and the binop result
        # is the value for this assign
        # and the result is a temp register:
        elif isinstance(self.prev_node, irBinop) and \
             self.prev_node.target == value and \
             self.prev_node.target.is_temp:

            # in this case, just change the binop result to the assign target:
            self.prev_node.target = target
            self.next_temp -= 1 # rewind temp counter

            # this is *technically* a peephole optimization, but really it is
            # just correcting for how binops unfold in the code generator.

            # skip appending a node
            return

        else:
            ir = irAssign(target, value, lineno=lineno)
            
        self.append_node(ir)

    def binop(self, op, left, right, lineno=None):
        target = self.add_temp(data_type='i32', lineno=lineno)
        
        ir = irBinop(target, op, left, right, lineno=lineno)

        self.append_node(ir)

        return target

    def augassign(self, op, target, value, lineno=None):
        if target.is_obj:
            ir = irObjectOp(op, target, value, lineno=lineno)
            self.append_node(ir)

            return

        elif target.is_ref and target.ref.is_array and len(target.lookups) == 0:
            ir = irVectorOp(op, target.ref, value, lineno=lineno)
            self.append_node(ir)

            return

        elif value.is_ref and value.ref.is_array and len(value.lookups) == 0:
            raise SyntaxError(f'Cannot vector op from array: {value.basename} to scalar: {target.basename}', lineno=lineno)

        result = self.binop(op, target, value, lineno=lineno)

        # must copy target, so SSA conversion will work
        self.assign(copy(target), result, lineno=lineno)

    def finish_module(self):
        ir = irProgram(self.funcs, self.globals, lineno=0)

        return ir

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
        if isinstance(self.prev_node, irReturn):
            # no need to jump, we have already returned
            return

        self.jump(end_label, lineno=lineno)

    def do_else(self, lineno=None):
        pass

    def end_ifelse(self, end_label, lineno=None):
        self.jump(end_label, lineno=lineno)
        self.scope_depth -= 1
        self.position_label(end_label)

    def begin_while(self, lineno=None):
        loop_name = f'while.{self.next_loop}'
        self.loop.append(loop_name)

        top_label = self.label(f'{self.loop[-1]}.top', lineno=lineno)
        end_label = self.label(f'{self.loop[-1]}.end', lineno=lineno)
        preheader_label = self.label(f'{self.loop[-1]}.preheader', lineno=lineno)
        header_label = self.label(f'{self.loop[-1]}.header', lineno=lineno)
        body_label = self.label(f'{self.loop[-1]}.body', lineno=lineno)

        self.loop_preheader.append(preheader_label)
        self.loop_header.append(header_label)
        self.loop_top.append(top_label)
        self.loop_body.append(body_label)
        self.loop_end.append(end_label)

        self.position_label(preheader_label)

        self.next_loop += 1

    def test_while_preheader(self, test, lineno=None):
        ir = irBranch(test, self.loop_header[-1], self.loop_end[-1], lineno=lineno)
        self.append_node(ir)

    def while_header(self, test, lineno=None):
        self.position_label(self.loop_header[-1])

        loop_name = self.loop[-1]
        ir = irLoopHeader(loop_name, lineno=lineno)
        self.append_node(ir)

        self.jump(self.loop_body[-1], lineno=lineno)
            

        self.scope_depth += 1
        
        self.position_label(self.loop_top[-1])
        ir = irLoopTop(loop_name, lineno=lineno)
        self.append_node(ir)

        
    def test_while(self, test, lineno=None):
        ir = irBranch(test, self.loop_body[-1], self.loop_end[-1], lineno=lineno)
        self.append_node(ir)
        
        self.position_label(self.loop_body[-1])

    def end_while(self, lineno=None):
        loop_name = self.loop[-1]

        self.jump(self.loop_top[-1], lineno=lineno)
        
        self.scope_depth -= 1
        self.position_label(self.loop_end[-1])

        self.loop.pop(-1)
        self.loop_preheader.pop(-1)
        self.loop_header.pop(-1)
        self.loop_top.pop(-1)
        self.loop_body.pop(-1)
        self.loop_end.pop(-1)


    def loop_break(self, lineno=None):
        assert self.loop_end[-1] != None

        self.jump(self.loop_end[-1], lineno=lineno)

    def loop_continue(self, lineno=None):
        assert self.loop_top[-1] != None    
        self.jump(self.loop_top[-1], lineno=lineno)

    def assertion(self, test, lineno=None):
        ir = irAssert(test, lineno=lineno)

        self.append_node(ir)

    def start_lookup(self, lineno=None):
        self.current_lookup.insert(0, [])

    def add_lookup(self, index, is_attr=False, lineno=None):
        if is_attr:
            index = irAttribute(index, lineno=lineno)

        self.current_lookup[0].append(index)

    def finish_lookup(self, target, is_attr=False, lineno=None):
        target.is_obj = is_attr
        target.lookups = self.current_lookup[0]
        self.current_lookup.pop(0)

        return target
