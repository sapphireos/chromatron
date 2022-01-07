
from .symbols import *
from .types import *
from .ir2 import *
import logging

INIT_TEMP_VAR = '__init_temp__'

class Builder(object):
    def __init__(self, script_name='fx_script', source=[]):
        self.script_name = script_name

        if isinstance(source, str):
            source = source.splitlines()

        self.source = source

        self.funcs = {}
        self.scope_depth = 0
        self.labels = {}
        # self.globals = {}
        # self.named_consts = {}
        self.structs = {}
        self.strings = {}
        self.links = []

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
        self.current_attr = []
        self.current_func = None

        self.global_symbols = SymbolTable()
        self.current_symbol_table = self.global_symbols
        self.symbol_tables = [self.global_symbols]

        self.type_manager = TypeManager()

        self.declare_var('pixels', data_type='PixelArray', is_global=True, lineno=-1)
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

            if value > 32767.0 or value < -32767.0:
                raise SyntaxError("Fixed16 value out of range, must be between -32767.0 and 32767.0", lineno=lineno)
            
        else:
            assert False

        var = self._build_var(name, data_type, lineno=lineno)
        # var.const = True
        var.value = value


        # is this the global symbol table?
        # if so, we don't add a const to it (otherwise it will not be loaded)
        if self.current_symbol_table != self.global_symbols:
            self.add_var_to_symbol_table(var)

        if self.current_func is None:
            return var

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

        for dim in dimensions:
            if not isinstance(dim, int):
                raise SyntaxError(f'Dimension {dim} is invalid for {name}, requires constant integer.', lineno=lineno)

        var = self._build_var(name, data_type, dimensions, keywords=keywords, lineno=lineno)

        # force objects to global symbols:
        if isinstance(var.var, varObject):  
            is_global = True

        if is_global:
            if name in self.global_symbols:
                # return self.globals[name]
                raise SyntaxError("Global variable '%s' already declared" % (name), lineno=lineno)

            var.is_global = True

            self.global_symbols.add(var.var) # deref the var container since the global is not technically loaded into a register

        else:
            if isinstance(var.var, varComposite):
                self.add_var_to_symbol_table(var.var)

            else:
                const = 0

                # set up init value:
                if var.init_val is not None:
                    const = var.init_val

                ir = irLoadConst(var, const, lineno=lineno)
                self.append_node(ir)

                self.add_var_to_symbol_table(var)

        return var

    def get_var(self, name, lineno=None):
        var = self.current_symbol_table.lookup(name)

        if var.is_container:
            return var.copy()

        # if composite or object, return, we will assume there
        # is a lookup
        if isinstance(var, varComposite) or isinstance(var, varObject):
            return var        

        # var does not have a container:
        # we need a load for the current symbol scope
        register = VarContainer(var.copy()) # wrap var
        self.add_var_to_symbol_table(register)

        ir = irLoad(register.copy(), var, lineno=lineno)
        self.append_node(ir)

        return register

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
            return self.strings[string]

        except KeyError:
            var = self.declare_var(f'__lit__{string}', 'strlit', keywords={'init_val': string}, is_global=True, lineno=lineno)
        
            self.strings[string] = var.var
        
            return var.var

    def generic_object(self, name, data_type, kw={}, lineno=None):
        return self.declare_var(name, data_type, keywords=kw, lineno=lineno)

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

        func = irFunc(*args, symbol_table=sym, type_manager=self.type_manager, source_code=self.source, **kwargs)
        self.funcs[func.name] = func
        self.current_func = func
        self.next_temp = 0
        self.scope_depth = 0

        func_label = self.label(f'func:{func.name}', lineno=kwargs['lineno'])
        self.position_label(func_label)

        self.declare_var(func.name, data_type='func', is_global=True, lineno=kwargs['lineno'])

        # this is used to load constants to variables that have init values.
        # if unused the optimizer will remove it.
        func._init_var = self.declare_var(INIT_TEMP_VAR, data_type='var', lineno=kwargs['lineno'])

        logging.info(f'Building function: {func.name}')

        return func

    def add_func_arg(self, func, name, data_type='i32', dimensions=[], lineno=None):
        var = self._build_var(name, data_type=data_type, dimensions=dimensions, lineno=lineno)
        
        if isinstance(var.var, varComposite):
            self.add_var_to_symbol_table(var.var)

        else:
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
        params = [self.load_value(p, lineno=lineno) for p in params]

        indirect = False
        lib_call = False

        if func_name == 'print':
            ir = irPrint(params[0], lineno=lineno)
            self.append_node(ir)

            return

        elif func_name in self.funcs:
            func = self.funcs[func_name]

        # check for indirect call or libcall:
        else:
            if isinstance(func_name, str):
                try:
                    func = self.get_var(func_name, lineno=lineno)

                    indirect = True

                except KeyError:
                    func = func_name
                    lib_call = True

            else:
                func = self.load_value(func_name, lineno=lineno)

                indirect = True

        if lib_call:
            ret_type = 'gfx16'
        
        else:
            ret_type = func.ret_type

        result = self.add_temp(data_type=ret_type, lineno=lineno)

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
            ir = irIndirectCall(func, params, lineno=lineno)
            self.append_node(ir)

        elif lib_call:
            hashed_func = string_hash_func(func)

            const = self.add_const(hashed_func, lineno=lineno)

            ir = irLibCall(const, params, func, lineno=lineno)
            self.append_node(ir)

        else:
            if len(params) != len(func.params):
                raise SyntaxError(f'Incorrect number of arguments to function: {func.name}. Expected: {len(func.params)} Received: {len(params)}', lineno=lineno)

            ir = irCall(func, params, lineno=lineno)
            self.append_node(ir)

        ir = irLoadRetVal(result, lineno=lineno)
        self.append_node(ir)

        return result

    def ret(self, value, lineno=None):
        value = self.load_value(value, lineno=lineno)

        ir = irReturn(value, lineno=lineno)

        self.append_node(ir)

        return ir

    def jump(self, target, lineno=None):
        ir = irJump(target, lineno=lineno)
        self.append_node(ir)

    def jump_loop(self, true, false, iterator, stop, lineno=None):
        ir = irLoop(true, false, self.get_var(iterator.name, lineno=lineno), self.get_var(iterator.name, lineno=lineno), stop, lineno=lineno)
        self.append_node(ir)

    def load_value(self, value, lineno=None):
        if value.data_type == 'offset':
            var = self.add_temp(data_type=value.target.scalar_type, lineno=lineno)
            ir = irLoad(var, value, lineno=lineno)
            self.append_node(ir)
            
            return var

        elif isinstance(value, VarContainer) and isinstance(value.var, varObjectRef):
            if value.target is not None and value.target.data_type == 'PixelArray':
                try:
                    data_type = PIXEL_ARRAY_FIELDS[value.attr.name]

                except KeyError:
                    raise SyntaxError(f'Unknown attribute for PixelArray: {value.target.name} -> {value.attr.name}', lineno=lineno)

                var = self.add_temp(data_type=data_type, lineno=lineno)

            else:
                var = self.add_temp(data_type='var', lineno=lineno)

            if len(value.lookups) > 0:
                result = self.add_temp(data_type='objref', lineno=lineno)
                    
                if value.target:
                    result.target = value.target

                else:
                    result.target = value
                
                ir = irObjectLookup(result, value, lookups=value.lookups, lineno=lineno)
                self.append_node(ir)

                value.lookups = []
                ir = irObjectLoad(var, result, value.attr, lineno=lineno)

            else:
                ir = irObjectLoad(var, value, value.attr, lineno=lineno)

            value.attr = None

            self.append_node(ir)

            return var

        elif isinstance(value, varObject):
            var = self.add_temp(data_type='objref', lineno=lineno)
            ir = irLoadRef(var, value, lineno=lineno)
            self.append_node(ir)
            
            return var

        # elif isinstance(value, varString):
        #     var = self.add_temp(data_type='strref', lineno=lineno)
        #     ir = irLoadRef(var, value, lineno=lineno)
        #     self.append_node(ir)
            
        #     return var

        elif isinstance(value, varComposite):
            # var = self.add_temp(data_type='objref', lineno=lineno)
            # ir = irLoadRef(var, value, lineno=lineno)
            # self.append_node(ir)
            
            # return var

            var = self.add_temp(data_type='offset', lineno=lineno)
            var.ref = value.lookup()

            ir = irLookup(var, value, lineno=lineno)
            self.append_node(ir)
            
            return var

        return value

    def convert_type(self, target, value, lineno=None):
        # in normal expressions, f16 will take precedence over i32.
        # however, for the assign, the assignment target will 
        # have priority.

        # print target, value
        # print target.get_base_type(), value.get_base_type()

        # check if value is const 0
        # if so, we don't need to convert, 0 has the same binary representation
        # in all data types
        if value.value == 0:
            pass

        elif isinstance(value, varStringLiteral):
            pass

        # check for special case of a database target.
        # we don't know the type of the database target, the 
        # database itself will do the conversion.
        # elif target.get_base_type() == 'db':
            # pass

        # check if target or value is a reference
        # we don't convert these
        elif (isinstance(target, VarContainer) and isinstance(target.var, varRef)) or \
             (isinstance(value, VarContainer) and isinstance(value.var, varRef)):
            pass

        # check if target or value is a offset
        # we don't convert these
        elif (isinstance(target, VarContainer) and isinstance(target.var, varOffset)) or \
             (isinstance(value, VarContainer) and isinstance(value.var, varOffset)):
            pass

        # check if base types don't match, if not, then do a conversion.
        elif target.scalar_type != value.scalar_type:
            # check if one of the types is gfx16.  if it is,
            # then we don't do a conversion
            # if (target.scalar_type == 'gfx16') or \
            #    (value.scalar_type == 'gfx16'):
            #    pass
               
            # else:


            temp = self.add_temp(lineno=lineno, data_type=target.scalar_type)
            ir = irConvertType(temp, value, lineno=lineno)
            self.append_node(ir)
            value = temp

            # convert value to target type and replace value with result
            # first, check if we created a temp reg.  if we did, just
            # do the conversion in place to avoid creating another, unnecessary
            # temp reg.

            # if value.temp:
            #     # check if one of the types is gfx16.  if it is,
            #     # then we don't do a conversion
            #     if (target.get_base_type() == 'gfx16') or \
            #        (value.get_base_type() == 'gfx16'):
            #        pass

            #     else:
            #         ir = irConvertTypeInPlace(value, target.get_base_type(), lineno=lineno)
            #         self.append_node(ir)

            # else:
            #     # check if one of the types is gfx16.  if it is,
            #     # then we don't do a conversion
            #     if (target.get_base_type() == 'gfx16') or \
            #        (value.get_base_type() == 'gfx16'):
            #        pass
                   
            #     else:
            #         temp = self.add_temp(lineno=lineno, data_type=target.get_base_type())
            #         ir = irConvertType(temp, value, lineno=lineno)
            #         self.append_node(ir)
            #         value = temp

        return value

    def assign(self, target, value, lineno=None):
        value = self.load_value(value, lineno=lineno)
        value = self.convert_type(target, value, lineno=lineno)

        if target.data_type == 'offset':
            if isinstance(target.target, varScalar):
                ir = irStore(value, target, lineno=lineno)

            elif isinstance(target.target, varArray):
                ir = irVectorAssign(target, value, lineno=lineno)    

            elif isinstance(target.target, varRef):
                ir = irStore(value, target, lineno=lineno)

            else:
                raise CompilerFatal(target)

        elif isinstance(target, VarContainer) and \
             isinstance(target.var, varObjectRef):
            
            if target.attr is not None:
                if len(target.lookups) > 0:
                    result = self.add_temp(data_type='objref', lineno=lineno)
                    
                    if target.target:
                        result.target = target.target

                    else:
                        result.target = target
                    
                    ir = irObjectLookup(result, target, lookups=target.lookups, lineno=lineno)
                    self.append_node(ir)

                    result.lookups = target.lookups # object store may need to know if there are any lookups for instruction selection
                    target.lookups = []

                    ir = irObjectStore(result, value, target.attr, lineno=lineno)

                else:
                    ir = irObjectStore(target, value, target.attr, lineno=lineno)

                target.attr = None

            else:
                if isinstance(value.var, varScalar):
                    raise SyntaxError(f'Cannot assign scalar to reference: {target} = {value}', lineno=lineno)

                ir = irAssign(target, value, lineno=lineno)

        elif isinstance(target, varArray):
            # load address to register:
            ref = self.add_temp(data_type='ref', lineno=lineno)
            ref.target = target

            ir = irLoadRef(ref, target, lineno=lineno)
            self.append_node(ir)            

            ir = irVectorAssign(ref, value, lineno=lineno)

        elif isinstance(target, VarContainer) and \
             isinstance(target.var, varRegister):

            ir = irAssign(target, value, lineno=lineno)
            
            if target.is_global:
                self.append_node(ir)
                ir = irStore(target, target.var, lineno=lineno)

        elif isinstance(target, varString):
            target = self.load_value(target, lineno=lineno)

            ir = irLoadString(target, value, lineno=lineno)

            # # load address to register:
            # var = self.add_temp(data_type='offset', lineno=lineno)
            # var.ref = target.lookup()

            # ir = irLookup(var, target, lineno=lineno)
            # self.append_node(ir)

            # if isinstance(value.ref, varString):
            #     ir = irLoadString(var, value, lineno=lineno)

            # else:
            #     raise SyntaxError(f'Invalid string assignment', lineno=lineno)

        else:
            raise SyntaxError(f'Invalid assign: {target} = {value}', lineno=lineno)
    
        self.append_node(ir)

    def binop(self, op, left, right, lineno=None):
        left = self.load_value(left, lineno=lineno)
        right = self.load_value(right, lineno=lineno)

        # if both types are gfx16, use i32
        if left.data_type == 'gfx16' and right.data_type == 'gfx16':
            target_data_type = 'i32'

        else:
            # if left is gfx16, use right type
            if left.data_type == 'gfx16':
                target_data_type = right.data_type
            else:
                target_data_type = left.data_type

            if right.data_type == 'f16':
                target_data_type = right.data_type

        left_result = left
        right_result = right

        if target_data_type == 'f16':
            if left.data_type != 'f16':
                left_result = self.add_temp(data_type=target_data_type, lineno=lineno)

                ir = irConvertType(left_result, left, lineno=lineno)
                self.append_node(ir)

            if right.data_type != 'f16':
                right_result = self.add_temp(data_type=target_data_type, lineno=lineno)

                ir = irConvertType(right_result, right, lineno=lineno)
                self.append_node(ir)

        if op in COMPARE_BINOPS:
            # need i32 for comparisons
            target_data_type = 'i32'

        target = self.add_temp(data_type=target_data_type, lineno=lineno)
        
        ir = irBinop(target, op, left_result, right_result, lineno=lineno)

        self.append_node(ir)

        return target

    def augassign(self, op, target, value, lineno=None):
        value = self.load_value(value, lineno=lineno)

        # do a type conversion here, if needed.
        # while binop will automatically convert types, 
        # it gives precedence to f16.  however, in the case
        # of augassign, we want to convert to the target type.
        value = self.convert_type(target, value, lineno=lineno)
        
        if isinstance(target, varArray):
            ref = self.add_temp(data_type='ref', lineno=lineno)
            ref.target = target

            ir = irLoadRef(ref, target, lineno=lineno)
            self.append_node(ir)

            ir = irVectorOp(op, ref, value, lineno=lineno)
            self.append_node(ir)

        elif target.data_type == 'offset':
            ir = irVectorOp(op, target, value, lineno=lineno)
            self.append_node(ir)

        elif target.data_type == 'objref':
            if len(target.lookups) > 0:
                result = self.add_temp(data_type='objref', lineno=lineno)
                
                if target.ref:
                    result.ref = target.ref

                else:
                    result.ref = target
                
                ir = irObjectLookup(result, target, lookups=target.lookups, lineno=lineno)
                self.append_node(ir)

                result.lookups = target.lookups # object op may need to know if there are any lookups for instruction selection
                target.lookups = []

                ir = irObjectOp(op, result, value, target.attr, lineno=lineno)
                self.append_node(ir)

            else:

                ir = irObjectOp(op, target, value, target.attr, lineno=lineno)
                self.append_node(ir)

        else:
            result = self.binop(op, target, value, lineno=lineno)

            # must copy target, so SSA conversion will work
            self.assign(copy(target), result, lineno=lineno)

    def finish_module(self):
        # check if there is no init function
        if 'init' not in self.funcs:
            func = self.func('init', lineno=-1)
            zero = self.declare_var(0, lineno=-1)
            self.ret(zero, lineno=-1)
            self.finish_func(func)

        # check if there is no loop function
        if 'loop' not in self.funcs:
            func = self.func('loop', lineno=-1)
            zero = self.declare_var(0, lineno=-1)
            self.ret(zero, lineno=-1)
            self.finish_func(func)

        # set up init code for global vars that have init values
        init_func = self.funcs['init']
        init_var = init_func._init_var

        for var in self.global_symbols.globals.values():
            if var.init_val is None:
                continue

            if not isinstance(var, varScalar) and not isinstance(var, varString):
                raise SyntaxError(f'Init values only implemented for scalar types and strings', var.lineno)

            if isinstance(var, varString):
                # init_var = init_var.copy()
                # ir = irLoadConst(var.copy(), var.init_val, lineno=var.lineno)
                # init_func.body.insert(1, ir)
                pass
                
            else:
                init_var = init_var.copy()
                ir = irLoadConst(init_var, var.init_val, lineno=var.lineno)
                init_func.body.insert(1, ir)
                ir = irStore(init_var, var, lineno=var.lineno)
                init_func.body.insert(2, ir)

        return irProgram(self.script_name, self.funcs, self.global_symbols, self.strings, lineno=0)

    def link(self, mode, source, dest, query, aggregation, rate, lineno=None):
        aggregations = {
            'any': LINK_AGG_ANY,
            'min': LINK_AGG_MIN,
            'max': LINK_AGG_MAX,
            'sum': LINK_AGG_SUM,
            'avg': LINK_AGG_AVG,
        }

        modes = {
            'send': LINK_MODE_SEND,
            'receive': LINK_MODE_RECV,
            'sync': LINK_MODE_SYNC,
        }

        new_link = {'mode': modes[mode],
                    'aggregation': aggregations[aggregation],
                    'rate': int(rate),
                    'source': source,
                    'dest': dest,
                    'query': query}

        pprint(new_link)

        self.links.append(new_link)

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

    """
    While loop:

    preheader: (decides if we run the loop at all)
        if test:
            jump header

        else:
            jump end

    header:
        this is where loop-invariants will go

        jump body
    
    body:
        body code

    top: (referring to the top of the loop as written in source code)
        if test:
            jump body

        else:
            jump end


    end:    
    
        loop done
    
    """

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

    """
    For loop:
    
    preheader: (decides if we run the loop at all)
        i <- 0
        if i < stop:
            jump header
        else:
            jump end

    header:
        this is where loop-invariants will go

        jump body

    body:
        body code

    top: (referring to the top of the loop as written in source code)
        i++
        if i < stop:
            jump body

        else:
            jump end
    
    end:
        loop done

    """

    def begin_for(self, iterator, stop, lineno=None):
        loop_name = f'for.{self.next_loop}'
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

    def test_for_preheader(self, iterator, stop, lineno=None):
        # init iterator to 0
        zero = self.declare_var(0, lineno=-1)
        self.assign(iterator, zero, lineno=lineno)

        compare = self.binop('lt', iterator, stop, lineno=lineno)

        ir = irBranch(compare, self.loop_header[-1], self.loop_end[-1], lineno=lineno)
        self.append_node(ir)

    def for_header(self, iterator, stop, lineno=None):
        self.position_label(self.loop_header[-1])

        loop_name = self.loop[-1]
        ir = irLoopHeader(loop_name, lineno=lineno)
        self.append_node(ir)

        self.jump(self.loop_body[-1], lineno=lineno)
            

        self.push_scope()
        
        self.position_label(self.loop_top[-1])
        ir = irLoopTop(loop_name, lineno=lineno)
        self.append_node(ir)

        self.position_label(self.loop_body[-1])

    def end_for(self, iterator, stop, lineno=None):
        loop_name = self.loop[-1]

        self.jump_loop(self.loop_top[-1], self.loop_end[-1], iterator, stop, lineno=lineno)
        
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


    def start_attr(self, lineno=None):
        self.current_attr.insert(0, [])

    def add_attr(self, index, lineno=None):
        index = irAttribute(index, lineno=lineno)

        self.current_attr[0].append(index)

    def finish_attr(self, target, lineno=None):
        if isinstance(target, VarContainer) and isinstance(target.var, varObjectRef):
            # object access from an existing reference (ref was already loaded, such as to an array)
            # p(pixref)->None
            target.attr = self.current_attr.pop(0)[-1]

            return target

        elif isinstance(target, VarContainer) and isinstance(target.var, varOffset):
            var = self.add_temp(data_type='objref', lineno=lineno)
            var.target = target.target
            var.lookups = target.target.lookups
            var.attr = self.current_attr.pop(0)[-1]

            ir = irLoad(var, target, lineno=lineno)
            self.append_node(ir)

            return var

        elif isinstance(target, varObject):
            # object access direct from an object requires the reference be loaded
            # p1(PixelArray)

            var = self.add_temp(data_type='objref', lineno=lineno)
            var.target = target
            var.attr = self.current_attr.pop(0)[-1]

            ir = irLoadRef(var, target, lineno=lineno)
            self.append_node(ir)

            return var

        raise CompilerFatal(f'Invalid attr for: {target}')

    def start_lookup(self, lineno=None):
        self.current_lookup.insert(0, [])

    def add_lookup(self, index, lineno=None):
        self.current_lookup[0].append(index)

    def finish_lookup(self, target, lineno=None):
        if isinstance(target, varObject):
            var = self.add_temp(data_type='objref', lineno=lineno)
            var.target = target
            var.lookups.extend(self.current_lookup.pop(0))

            ir = irLoadRef(var, target, lineno=lineno)
            self.append_node(ir)

            return var

        elif isinstance(target, VarContainer) and isinstance(target.var, varObjectRef):
            target.var.lookups.extend(self.current_lookup[0])
            
            self.current_lookup.pop(0)
            
            return target
    
        elif isinstance(target, varArray):
            ref = self.add_temp(data_type='ref', lineno=lineno)
            ref.target = target

            ir = irLoadRef(ref, target, lineno=lineno)
            self.append_node(ir)

            offset = self.add_temp(data_type='offset', lineno=lineno)
            offset.target = target.lookup(self.current_lookup[0], lineno=lineno)

            # strip any lookups from an object ref (which will be resolved directly
            # in the object accessor instruction, instead of the array lookup)
            if isinstance(offset.target, varObjectRef):
                self.current_lookup[0] = self.current_lookup[0][:len(self.current_lookup[0]) - len(offset.target.lookups)]

            lookups = self.current_lookup.pop(0)
            counts = []
            strides = []

            for count in ref.target.get_counts():
                counts.append(self.add_const(count, lineno=lineno))

            for stride in ref.target.get_strides():
                strides.append(self.add_const(stride, lineno=lineno))

            # limit depth to number of indexes
            counts = counts[:len(lookups)]
            strides = strides[:len(lookups)]

            ir = irOffset(offset, ref, lookups, counts, strides, lineno=lineno)
            self.append_node(ir)

            return offset


            # var = self.add_temp(data_type='offset', lineno=lineno)
            # var.ref = target.lookup(self.current_lookup[0], lineno=lineno)

            # # strip any lookups from an object ref (which will be resolved directly
            # # in the object accessor instruction, instead of the array lookup)
            # if isinstance(var.ref, varObjectRef):
            #     self.current_lookup[0] = self.current_lookup[0][:len(self.current_lookup[0]) - len(var.ref.lookups)]


            # lookup = self.current_lookup.pop(0)

            # # load array details as constants
            # counts = []
            # strides = []

            # sub_target = target

            # for i in range(len(lookup)):
            #     try:
            #         count = self.add_const(sub_target.length, lineno=lineno)

            #     except (IndexError, AttributeError):
            #         raise SyntaxError(f'{target.name} has only {target.dimensions} dimensions, requested {len(lookup)}', lineno=self.lineno)

            #     counts.append(count)

            #     stride = self.add_const(sub_target.stride, lineno=lineno)
            #     strides.append(stride)

            #     sub_target = sub_target.element


            # ir = irLookup(var, target, lookup, counts, strides, lineno=lineno)
            # self.append_node(ir)

            # return var

        raise CompilerFatal(f'Invalid lookup for: {target}')

