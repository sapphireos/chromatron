
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

        self.next_temp = 0
        # self.refs = {}

        self.next_loop = 0

        self.loop = []
        self.loop_preheader = []
        self.loop_header = []
        self.loop_top = []
        self.loop_body = []
        self.loop_end = []

        self.current_lookup = None
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

    def add_global(self, name, data_type='i32', dimensions=[], keywords=None, lineno=None):
        if name in self.globals:
            # return self.globals[name]
            raise SyntaxError("Global variable '%s' already declared" % (name), lineno=lineno)

        ir = irVar(name, data_type, lineno=lineno)
        ir.is_global = True
        self.globals[name] = ir

        return ir

    def add_temp(self, data_type=None, lineno=None):
        name = '%' + str(self.next_temp)
        self.next_temp += 1

        ir = irVar(name, datatype=data_type, lineno=lineno)
        ir.is_temp = True

        return ir
    
    def add_ref(self, target, lookups, lineno=None):
        # if isinstance(target, irRef):
        if target.is_ref:
            name = target.target.name

        else:
            name = target.name

        # if name not in self.refs:
        #     self.refs[name] = 0

        # target.name = f'{name}_{self.refs[name]}'
        target.name = f'{name}'

        # need to look up proper datatype from types database
        ir = irVar(target, datatype='i32', lineno=lineno)
        ir.is_ref = True
        ir.is_temp = True
        ir.lookups = lookups

        # self.refs[name] += 1

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
            return copy(self.consts[name])

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
    
    def declare_var(self, name, data_type='i32', dimensions=[], keywords={}, is_global=False, lineno=None):
        if is_global:
            return self.add_global(name, data_type, dimensions, keywords=keywords, lineno=lineno)

        else:
            # if len(keywords) > 0:
            #     raise SyntaxError("Cannot specify keywords for local variables", lineno=lineno)

            var = irVar(name, data_type, lineno=lineno)
            
            ir = irDefine(var, lineno=lineno)

            self.append_node(ir)

            return var

    def get_var(self, name, lineno=None):
        if name in self.named_consts:
            return self.named_consts[name]

        return irVar(name, lineno=lineno)

    def create_struct(self, name, fields, lineno=None):
        # new_fields = {}
        # offsets = {}
        # offset = 0
        # for field_name, field in list(fields.items()):
        #     field_type = field['type']
        #     field_dims = field['dimensions']
            
        #     # new_fields[field_name] = self.build_var(field_name, field_type, field_dims, lineno=lineno)

        #     offsets[field_name] = offset
        #     # offsets[field_name] = self.add_const(offset, lineno=lineno)
        #     offset += new_fields[field_name].length

        # ir = irRecord(name, name, new_fields, offsets, lineno=lineno)

        # self.record_types[name] = ir

        pass

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
        self.scope_depth = 0

        func_label = self.label(f'func:{func.name}', lineno=kwargs['lineno'])
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
        # if isinstance(target, irRef) and isinstance(value, irRef):
        #     # need to load from reference to a temp reg,
        #     # then store to the other reference from the temp reg.

        #     # LOAD
        #     # STORE

        #     ir = irAssign(target, value, lineno=lineno)

        # elif isinstance(target, irRef):
        #     # assigning to a reference target
        #     # we need to load the value to a temp register,
        #     # then we need to do a STORE to memory on
        #     # the reference address.  Can be the same
        #     # store as for globals.

        #     # note that we might not have to do an 
        #     # assign to temp, depending on the situation,
        #     # we can just point the STORE directly to
        #     # the source register.

        #     # ASSIGN/BINOP/LOAD_CONST
        #     # STORE

        #     ir = irAssign(target, value, lineno=lineno)

        # elif isinstance(value, irRef):
        #     # loading from a reference target.
        #     # should be able to just do a LOAD from memory
        #     # to the target register.

        #     # LOAD

        #     ir = irAssign(target, value, lineno=lineno)

        if value.is_const:
            ir = irLoadConst(target, value, lineno=lineno)

        # check if previous op is a binop and the binop result
        # is the value for this assign
        # and the result is a temp register:
        elif isinstance(self.prev_node, irBinop) and self.prev_node.target == value and self.prev_node.target.is_temp:
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



    # def begin_while(self, lineno=None):
    #     top_label = self.label('while.top', lineno=lineno)
    #     end_label = self.label('while.end', lineno=lineno)
    #     self.position_label(top_label)

    #     self.loop_top.append(top_label)
    #     self.loop_end.append(end_label)

    #     self.scope_depth += 1

    # def test_while(self, test, lineno=None):
    #     body_label = self.label('while.body', lineno=lineno)

    #     ir = irBranch(test, body_label, self.loop_end[-1], lineno=lineno)
    #     self.append_node(ir)
    #     self.position_label(body_label)

    # def end_while(self, lineno=None):
    #     ir = irJump(self.loop_top[-1], lineno=lineno)
    #     self.append_node(ir)

    #     self.scope_depth -= 1

    #     self.position_label(self.loop_end[-1])

    #     self.loop_top.pop(-1)
    #     self.loop_end.pop(-1)


    # def while_header(self, test, lineno=None):
    #     pass

    # def test_while_preheader(self, test, lineno=None):
    #     pass



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

        # place a landing pad at the end of the loop body
        # this makes some loop analysis easier
        # jump_label = self.label(f'{loop_name}.landing', lineno=lineno)
        # self.position_label(jump_label)

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
        assert self.current_lookup is None

        self.current_lookup = []

    def add_lookup(self, index, lineno=None):
        if isinstance(index, str):
            index = irAttribute(index, lineno=lineno)

        self.current_lookup.append(index)

    def finish_lookup(self, target, lineno=None):
        result = self.add_ref(target, self.current_lookup, lineno=lineno)
        # ir = irLookup(result, target, self.current_lookup, lineno=lineno)
        # self.append_node(ir)

        self.current_lookup = None

        return result
