
from .symbols import *
from .types import *
from .ir2 import *
import logging

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
        # self.globals = {}
        # self.named_consts = {}
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

        # self.locals = {}
        # self.refs = {}
        self.current_lookup = []
        self.current_func = None

        self.global_symbols = SymbolTable()
        self.symbol_tables = [self.global_symbols]

        self.type_manager = TypeManager()

        self.declare_var('pixels', data_type='obj', is_global=True, lineno=-1)
        self.declare_var('db', data_type='obj', is_global=True, lineno=-1)

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

        var = self._build_var(name, data_type, lineno=lineno)
        self.add_var_to_symbol_table(var)

        return var

        # ir = irVar(name, datatype=data_type, lineno=lineno)
        # ir.is_temp = True

        # return ir
    
    def add_ref(self, target, lookups=[], lineno=None):
        # ir = irVar(target.name, lineno=lineno)
        # ir.lookups = lookups
        # ir.is_ref = True
        # ir.ref = target
        
        # return ir    
        pass
    
    def add_const(self, value, data_type=None, lineno=None):
        if value is True:
            value = 1
        elif value is False:
            value = 0

        name = f'{str(value)}'

        try:
            return self.get_var(f'${name}')

        except KeyError:
            pass
    
        # if name in self.current_func.consts:
        #     return copy(self.current_func.consts[name])

        if isinstance(value, int):
            data_type = 'i32'

        elif isinstance(value, float):
            data_type = 'f16'
            
        else:
            assert False

        var = self._build_var(name, data_type, lineno=lineno)
        # var.const = True
        var.value = value
        self.add_var_to_symbol_table(var)

        ir = irLoadConst(var, value, lineno=lineno)
        self.append_node(ir)

        # self.current_func.consts[name] = var

        return var

        # ir = irVar(name, data_type, lineno=lineno)
        # ir.is_const = True

        # self.current_func.consts[name] = ir

        # return ir

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

        var = self._build_var(name, data_type, lineno=lineno)
        # var.const = True
        var.value = value

        # self.current_func.consts[name] = var
        self.add_var_to_symbol_table(var)

        return var

        # ir = irVar(str(value), data_type, lineno=lineno)
        # ir.is_const = True

        # self.named_consts[name] = ir

        # return ir
    
    def _build_var(self, name, data_type=None, dimensions=[], keywords={}, lineno=None):
        return self.type_manager.create_var_from_type(name, data_type, dimensions=dimensions, keywords=keywords, lineno=lineno)


        # if data_type in PRIMITIVE_TYPES:
        #     return irVar(name, data_type, dimensions, lineno=lineno)

        # elif data_type == 'str':
        #     # var = irString(name, lineno=lineno, **keywords) 

        #     var = irVar(name, data_type, dimensions, lineno=lineno)
        #     var.is_ref = True
        #     var.ref = self.strings[keywords['init_val']]

        #     return var

        # elif data_type in self.structs:
        #     return self.structs[data_type](name, dimensions, lineno=lineno)

        # raise CompilerFatal(f'Unknown type {data_type}')

    def declare_var(self, name, data_type='i32', dimensions=[], keywords={}, is_global=False, lineno=None):
        # if data_type not in PRIMITIVE_TYPES and data_type not in self.structs and data_type != 'str':
        #     raise SyntaxError(f'Type {data_type} is unknown', lineno=lineno)

        var = self._build_var(name, data_type, dimensions, keywords=keywords, lineno=lineno)

        if is_global:
            if name in self.global_symbols:
                # return self.globals[name]
                raise SyntaxError("Global variable '%s' already declared" % (name), lineno=lineno)

            var.is_global = True
            # self.globals[name] = var

            self.global_symbols.add(var.var) # deref the var container since the global is not technically loaded into a register

        else:
            # if len(keywords) > 0:
            #     raise SyntaxError("Cannot specify keywords for local variables", lineno=lineno)
            
            # self.locals[name] = var

            # ir = irDefine(var, lineno=lineno)
            ir = irLoadConst(var, 0, lineno=lineno)

            self.append_node(ir)

            self.add_var_to_symbol_table(var)

        # if var.is_ref:
            # self.refs[var.basename] = var

        return var

    def get_var(self, name, lineno=None):
        var = self.current_symbol_table.lookup(name)

        if var.is_container:
            return var.copy()

        assert var.is_global

        # if composite, return, we will assume there
        # is a lookup
        if isinstance(var, varComposite):
            return var        

        # var does not have a container:
        # we need a load for the current symbol scope
        register = VarContainer(var.copy()) # wrap var
        self.add_var_to_symbol_table(register)

        ir = irLoad(register.copy(), var, lineno=lineno)
        self.append_node(ir)

        return register

        # if name in self.named_consts:
        #     return self.named_consts[name]

        # if name in self.globals:
        #     return self.add_ref(self.globals[name], lineno=lineno)

        # # elif name in self.refs:
        # #     return self.add_ref(self.refs[name], lineno=lineno)

        # if name in self.locals:
        #     return copy(self.locals[name])

        # raise CompilerFatal(f'Var not found: {name}')

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
            var = self.strings[string]

        except KeyError:
            var = varStringLiteral(string=string, lineno=lineno)

            self.strings[string] = var

        return var

    def push_symbol_table(self):
        sym = SymbolTable(self.symbol_tables[0])
        self.symbol_tables.insert(0, sym)

        self.current_symbol_table = sym

        return sym

    def pop_symbol_table(self):
        self.symbol_tables.pop(0)
        self.current_symbol_table = self.symbol_tables[0]

    def add_var_to_symbol_table(self, var):
        self.current_symbol_table.add(var)

    ###################################
    # IR instructions
    ###################################
    def push_scope(self):
        self.scope_depth += 1
        self.push_symbol_table()

    def pop_scope(self):
        self.scope_depth -= 1
        self.pop_symbol_table()

    def append_node(self, node):
        node.scope_depth = self.scope_depth
        self.current_func.append_node(node)

    @property
    def prev_node(self):
        return self.current_func.prev_node

    def nop(self, lineno=None):
        pass

    def func(self, *args, returns=None, **kwargs):
        sym = self.push_symbol_table()

        print(returns)

        func = irFunc(*args, symbol_table=sym, type_manager=self.type_manager, **kwargs)
        self.funcs[func.name] = func
        self.current_func = func
        self.next_temp = 0
        # self.refs = {}
        # self.locals = {}
        self.scope_depth = 0

        func_label = self.label(f'func:{func.name}', lineno=kwargs['lineno'])
        self.position_label(func_label)

        self.declare_var(func.name, data_type='func', is_global=True, lineno=kwargs['lineno'])

        logging.info(f'Building function: {func.name}')

        return func

    def add_func_arg(self, func, name, data_type='i32', dimensions=[], lineno=None):
        var = self._build_var(name, data_type=data_type, dimensions=dimensions, lineno=lineno)
        self.add_var_to_symbol_table(var)
        func.params.append(var)

        return var

    def finish_func(self, func):
        func.next_temp = self.next_temp
        self.pop_symbol_table()

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
        if func_name in self.funcs:
            func = self.funcs[func_name]
            indirect = False

        else:
            # check for indirect call:
            func = self.get_var(func_name, lineno=lineno)
            indirect = True

        result = self.add_temp(data_type=func.ret_type, lineno=lineno)

        # if func_name in ARRAY_FUNCS:
        #     if len(params) != 1:
        #         raise SyntaxError("Array functions take one argument", lineno=lineno)

        #     if func_name == 'len':
        #         # since arrays are fixed length, we don't need a libcall, we 
        #         # can just do an assignment.
        #         array_len = params[0].ref.count
        #         const = self.add_const(array_len, lineno=lineno)

        #         self.assign(result, const, lineno=lineno)
        if indirect:
            ir = irIndirectCall(func, params, result, lineno=lineno)

        else:
            ir = irCall(func, params, result, lineno=lineno)

        self.append_node(ir)

        return result

    def store_globals(self, lineno=None):
        # only store globals that were loaded to containers:
        for g in [g for g in self.current_symbol_table.loaded_globals.values() if isinstance(g, VarContainer)]:
            g = g.copy()
            ir = irStore(g, g.var, lineno=lineno)
            self.append_node(ir)

    def ret(self, value, lineno=None):
        value = self.load_value(value, lineno=lineno)

        self.store_globals(lineno=lineno)

        ir = irReturn(value, lineno=lineno)

        self.append_node(ir)

        return ir

    def jump(self, target, lineno=None):
        ir = irJump(target, lineno=lineno)
        self.append_node(ir)

    def load_value(self, value, lineno=None):
        if value.data_type == 'offset':
            var = self.add_temp(data_type=value.ref.scalar_type, lineno=lineno)
            ir = irLoad(var, value, lineno=lineno)
            self.append_node(ir)
            
            return var

        elif value.data_type == 'objref':
            var = self.add_temp(data_type='var', lineno=lineno)
            ir = irObjectLoad(var, value.ref, lookups=value.lookups, lineno=lineno)

            self.append_node(ir)
            
            return var

        elif value.data_type == 'func':
            var = self.add_temp(data_type='funcref', lineno=lineno)
            ir = irObjectLoad(var, value, lineno=lineno)

            self.append_node(ir)
            
            return var

        return value

    def assign(self, target, value, lineno=None):
        value = self.load_value(value, lineno=lineno)

        if target.data_type == 'offset':
            if isinstance(target.ref, varScalar):
                ir = irStore(value, target, lineno=lineno)

            elif isinstance(target.ref, varArray):
                ir = irVectorAssign(target, value, lineno=lineno)    

            else:
                raise CompilerFatal(target)

        elif target.data_type in ['objref', 'funcref']:
            ir = irObjectStore(target.ref, value, lookups=target.lookups, lineno=lineno)

        elif isinstance(target, varArray):
            # load address to register:
            var = self.add_temp(data_type='offset', lineno=lineno)
            var.ref = target.lookup()

            ir = irLookup(var, target, lineno=lineno)
            self.append_node(ir)

            ir = irVectorAssign(var, value, lineno=lineno)

        else:
            ir = irAssign(target, value, lineno=lineno)

    
        self.append_node(ir)


        # if target.is_obj:
        #     ir = irObjectAssign(target, value, lineno=lineno)

        # elif value.is_obj:
        #     ir = irObjectLoad(target, value, lineno=lineno)

        # elif target.is_ref and target.ref.is_array and len(target.lookups) == 0:
        #     ir = irVectorAssign(target.ref, value, lineno=lineno)
        
        # elif value.is_ref and value.ref.is_array and len(value.lookups) == 0:
        #     raise SyntaxError(f'Cannot vector assign from array: {value.basename} to scalar: {target.basename}', lineno=lineno)

        # elif target.is_ref:
        #     ir = irAssign(target, value, lineno=lineno)

        # elif value.is_const:
        #     ir = irLoadConst(target, value, lineno=lineno)

        # # check if previous op is a binop and the binop result
        # # is the value for this assign
        # # and the result is a temp register:
        # elif isinstance(self.prev_node, irBinop) and \
        #      self.prev_node.target == value and \
        #      self.prev_node.target.is_temp:

        #     # in this case, just change the binop result to the assign target:
        #     self.prev_node.target = target
        #     self.next_temp -= 1 # rewind temp counter

        #     # this is *technically* a peephole optimization, but really it is
        #     # just correcting for how binops unfold in the code generator.

        #     # skip appending a node
        #     return

        # else:
        #     ir = irAssign(target, value, lineno=lineno)
            
        # self.append_node(ir)

    def binop(self, op, left, right, lineno=None):
        left = self.load_value(left, lineno=lineno)
        right = self.load_value(right, lineno=lineno)

        target = self.add_temp(data_type='i32', lineno=lineno)
        
        ir = irBinop(target, op, left, right, lineno=lineno)

        self.append_node(ir)

        return target

    def augassign(self, op, target, value, lineno=None):
        value = self.load_value(value, lineno=lineno)
        # if target.is_obj:
        #     ir = irObjectOp(op, target, value, lineno=lineno)
        #     self.append_node(ir)

        #     return

        # elif target.is_ref and target.ref.is_array and len(target.lookups) == 0:
        #     ir = irVectorOp(op, target.ref, value, lineno=lineno)
        #     self.append_node(ir)

        #     return

        # elif value.is_ref and value.ref.is_array and len(value.lookups) == 0:
        #     raise SyntaxError(f'Cannot vector op from array: {value.basename} to scalar: {target.basename}', lineno=lineno)
        if isinstance(target, varArray) or target.data_type == 'offset':
            ir = irVectorOp(op, target, value, lineno=lineno)
            self.append_node(ir)

        elif target.data_type == 'objref':
            ir = irObjectOp(op, target.ref, value, target.lookups, lineno=lineno)
            self.append_node(ir)

        else:
            result = self.binop(op, target, value, lineno=lineno)

            # must copy target, so SSA conversion will work
            self.assign(copy(target), result, lineno=lineno)

    def finish_module(self):
        ir = irProgram(self.funcs, self.global_symbols, lineno=0)

        return ir

    def ifelse(self, test, lineno=None):
        body_label = self.label('if.then', lineno=lineno)
        else_label = self.label('if.else', lineno=lineno)
        end_label = self.label('if.end', lineno=lineno)

        assert not isinstance(test, irBinop)

        branch = irBranch(test, body_label, else_label, lineno=lineno)
        self.append_node(branch)

        self.push_scope()

        return body_label, else_label, end_label

    def end_if(self, end_label, lineno=None):
        if isinstance(self.prev_node, irReturn):
            self.pop_scope()

            # no need to jump, we have already returned
            return

        self.jump(end_label, lineno=lineno)

        self.pop_scope()

    def do_else(self, lineno=None):
        self.push_scope()

    def end_ifelse(self, end_label, lineno=None):
        self.jump(end_label, lineno=lineno)
        self.pop_scope()
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
            

        self.push_scope()
        
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
        
        self.pop_scope()
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

    def finish_lookup(self, target, load=False, is_attr=False, lineno=None):
        if isinstance(target, varObject):
            var = self.add_temp(data_type='objref', lineno=lineno)

            var.ref = target
            var.lookups = self.current_lookup[0]

        elif target.var.scalar_type == 'funcref':
            var = self.add_temp(data_type='funcref', lineno=lineno)

            var.ref = target
            var.lookups = self.current_lookup[0]

        else:
            var = self.add_temp(data_type='offset', lineno=lineno)
            var.ref = target.lookup(self.current_lookup[0])

            ir = irLookup(var, target, self.current_lookup[0], lineno=lineno)
            self.append_node(ir)

        self.current_lookup.pop(0)

        return var

