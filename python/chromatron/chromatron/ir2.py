
import logging
from copy import copy, deepcopy


# debug imports:
import sys
from pprint import pprint
# /debug

source_code = []

COMMUTATIVE_OPS = ['add', 'mul']


def set_source_code(source):
    global source_code
    source_code = source

def params_to_string(params):
    s = ''

    if len(params) == 0:
        return s

    for p in params:
        try:
            s += '%s %s,' % (p.type, p.name)

        except AttributeError:
            s += '%s' % (p.name)            

    # strip last comma
    if s[-1] == ',':
        s = s[:-1]

    return s


def add_const_temp(const, datatype=None, lineno=None):
    name = '$' + str(const)
    
    ir = irVar(name, datatype=datatype, lineno=lineno)
    ir.is_temp = True
    ir.holds_const = True

    return ir


class SyntaxError(Exception):
    def __init__(self, message='', lineno=None):
        self.lineno = lineno

        message += ' (line %d)' % (self.lineno)

        super().__init__(message)

class CompilerFatal(Exception):
    def __init__(self, message=''):
        super(CompilerFatal, self).__init__(message)


class IR(object):
    def __init__(self, lineno=None):
        self.lineno = lineno
        self.block = None
        self.scope_depth = None
        self.live_in = []
        self.live_out = []
        self.is_nop = False

        self.loops = []

        assert self.lineno != None

    def generate(self):
        return BaseInstruction()

    def get_input_vars(self):
        return []

    def get_output_vars(self):
        return []

    def get_jump_target(self):
        return None

    @property
    def global_input_vars(self):
        return [a for a in self.get_input_vars() if a.is_global]

    @property
    def global_output_vars(self):
        return [a for a in self.get_output_vars() if a.is_global]

    @property
    def local_input_vars(self):
        return [a for a in self.get_input_vars() if not a.is_temp and not a.is_const and not a.is_global]

    @property
    def local_output_vars(self):
        return [a for a in self.get_output_vars() if not a.is_temp and not a.is_const and not a.is_global]


class irProgram(IR):
    def __init__(self, funcs={}, global_vars={},**kwargs):
        super().__init__(**kwargs)

        self.funcs = funcs
        self.globals = global_vars        

    def __str__(self):
        s = "FX IR:\n"

        s += 'Globals:\n'
        for i in list(self.globals.values()):
            s += '%d\t%s\n' % (i.lineno, i)

        # s += 'Consts:\n'
        # for i in list(self.consts.values()):
        #     s += '%d\t%s\n' % (i.lineno, i)
        
        # s += 'PixelArrays:\n'
        # for i in list(self.pixel_arrays.values()):
        #     s += '%d\t%s\n' % (i.lineno, i)

        s += 'Functions:\n'
        for func in list(self.funcs.values()):
            s += '%s\n' % (func)

        return s

    ###################################
    # Analysis
    ###################################
    def analyze_blocks(self):
        for func in self.funcs.values():
            func.analyze_blocks()


class Edge(object):
    def __init__(self, from_node, to_node):
        self.from_node = from_node
        self.to_node = to_node

    def __hash__(self):
        t = (self.from_node, self.to_node)

        return hash(t)

    def __eq__(self, other):
        return hash(self) == hash(other)

    def __str__(self):
        return f'{self.from_node.name} -> {self.to_node.name}'


class irBlock(IR):
    def __init__(self, func=None, **kwargs):
        super().__init__(**kwargs)
        self.predecessors = []
        self.successors = []
        self.code = []
        self.locals = {}
        self.params = {}
        self.func = func
        self.globals = self.func.globals

        self.entry_label = None
        self.jump_target = None

        self.local_values = {}
        self.local_defines = {}

        self.is_ssa = False
        self.filled = False
        self.sealed = False
        self.temp_phis = []

    def __str__(self):
        tab = '\t'
        depth = f'{self.scope_depth * tab}'
        s =  f'{depth}||||||||||||||||||||||||||||||||||||||||||||||||||||||||\n'
        s += f'{depth}| BLOCK: {self.name} @ {self.lineno}'
        if self.is_leader:
            s += ': LEADER'

        if self.is_terminator:
            s += ': TERMINATOR'

        s += '\n'

        s += f'{depth}| Predecessors:\n'
        for p in self.predecessors:
            s += f'{depth}|\t{p.name}\n'

        s += f'{depth}| Successors:\n'
        for p in self.successors:
            s += f'{depth}|\t{p.name}\n'

        s += f'{depth}| Params:\n'
        for p in self.params.values():
            s += f'{depth}|\t{p}\n'

        lines_printed = []
        s += f'{depth}| Code:\n'
        for ir in self.code:
            if ir.lineno >= 0 and ir.lineno not in lines_printed and not isinstance(ir, irLabel):
                s += f'{depth}|________________________________________________________\n'
                s += f'{depth}| {ir.lineno}: {depth}{source_code[ir.lineno - 1].strip()}\n'
                lines_printed.append(ir.lineno)

            ir_s = f'{depth}|\t{str(ir):48}'

            if self.func.live_in:
                s += f'{ir_s}\n'
                s += f'{depth}|\t  in:  {sorted(list(set([a.name for a in self.func.live_in[ir]])))}\n'
                s += f'{depth}|\t  out: {sorted(list(set([a.name for a in self.func.live_out[ir]])))}\n'

            else:
                s += f'{ir_s}\n'

        s += f'{depth}|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n'

        return s

    def __repr__(self):
        return self.name

    def get_blocks(self, blocks=None, visited=None):
        if blocks is None:
            blocks = []

        if visited is None:
            visited = []

        if self in visited:
            return visited

        visited.append(self)

        blocks.append(self)
        blocks.extend(self.successors)

        for s in self.successors:
            s.get_blocks(blocks, visited)

        return blocks

    @property
    def name(self):
        try:
            if isinstance(self.code[0], irLabel):
                return f'{self.code[0].name}'
            else:
                assert False

        except IndexError:
            return 'UNKNOWN BLOCK'

    @name.setter
    def name(self, value):
        self._name = value

    @property
    def is_leader(self):
        return len(self.predecessors) == 0

    @property
    def is_terminator(self):
        return len(self.successors) == 0    

    # @property
    # def params(self):
        # return [v for v in self.input_vars if not v.is_temp and not v.is_const]

    @property
    def global_input_vars(self):
        v = {}
        for node in self.code:
            inputs = node.global_input_vars

            for i in inputs:
                if i.name not in v:
                    v[i.name] = i

        return v.values()

    @property
    def global_output_vars(self):
        v = {}
        for node in self.code:
            outputs = node.global_output_vars

            for o in outputs:
                if o.name not in v:
                    v[o.name] = o

        return v.values()

    @property
    def local_input_vars(self):
        v = {}
        for node in self.code:
            inputs = node.local_input_vars

            for i in inputs:
                if i.name not in v:
                    v[i.name] = i

        return v.values()

    @property
    def local_output_vars(self):
        v = {}
        for node in self.code:
            outputs = node.local_output_vars

            for o in outputs:
                if o.name not in v:
                    v[o.name] = o

        return v.values()

    def get_input_vars(self):
        v = []
        for node in self.code:
            v.extend(node.get_input_vars())

        return v

    def get_output_vars(self):
        v = []
        for node in self.code:
            v.extend(node.get_output_vars())

        return v

    def append(self, node):
        # ensure that each node only belongs to one block:

        assert node.block is None
 
        node.block = self
        self.code.append(node)

    def get_defined(self, name, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return []

        try:
            return [self.defines[name]]

        except KeyError:
            pass

        visited.append(self)

        ds = []
        for pre in self.predecessors:
            for d in pre.get_defined(name, visited=visited):
                if d not in ds:
                    ds.append(d)

        return ds

    ##############################################
    # Analysis Passes
    ##############################################

    def init_vars(self, visited=None, declarations=None):    
        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)

        self.stores = {}
        self.defines = {}
        self.declarations = copy(self.globals)
        if declarations is not None:
            self.declarations.update(declarations
                )
        new_code = []

        # iterate over code
        # look for defines and record them.
        # also insert inits to 0
        for index in range(len(self.code)):
            ir = self.code[index]
            new_code.append(ir)

            if isinstance(ir, irDefine):
                if ir.var._name in self.globals:
                    raise SyntaxError(f'Variable {ir.var._name} is already defined (variable shadowing is not allowed).', lineno=ir.lineno)

                self.declarations[ir.var._name] = ir.var

                # init variable to 0
                assign = irLoadConst(copy(ir.var), self.func.get_zero(ir.lineno), lineno=ir.lineno)
                assign.block = self
                
                new_code.append(assign)

        self.code = new_code
        new_code = []
        
        # iterate over code
        for index in range(len(self.code)):
            ir = self.code[index]

            if not isinstance(ir, irDefine):
                # analyze inputs and LOAD from global if needed
                inputs = ir.local_input_vars

                for i in inputs:
                    if i._name in self.globals and i._name not in self.defines:
                        # copy global var to register and add to defines:
                        i.__dict__ = copy(self.globals[i._name].__dict__)
                        i.is_global = False
                        self.defines[i._name] = i

                        # insert a LOAD instruction here
                        ir = irLoad(i, self.globals[i._name], lineno=ir.lineno)
                        ir.block = self
                        new_code.append(ir)

                        # set up SSA
                        i.ssa_version = 0

                # analyze outputs and initialize a global as needed                
                outputs = ir.local_output_vars

                for o in outputs:
                    if o._name in self.globals:
                        # record a store - unless this is a load
                        if not isinstance(ir, irLoad):
                            self.stores[o._name] = o

                        if  o._name not in self.defines:
                            # copy global var to register and add to defines:
                            o.__dict__ = copy(self.globals[o._name].__dict__)
                            o.is_global = False
                            self.defines[o._name] = o

                            # set up SSA
                            o.ssa_version = 0

            new_code.append(self.code[index])

        self.code = new_code
        new_code = []

        # iterate over code
        for index in range(len(self.code)):
            ir = self.code[index]

            if isinstance(ir, irReturn): # or irCall
                for k, v in self.stores.items():
                    ir = irStore(v, self.globals[k], lineno=ir.lineno)
                    ir.block = self
                    new_code.append(ir)

            # assign types
            variables = []
            variables.extend(ir.local_input_vars)
            variables.extend(ir.local_output_vars)
            for v in variables:
                if v.type is None:
                    v.type = self.declarations[v.name].type


            new_code.append(self.code[index])

        self.code = new_code

        # continue with successors:
        for suc in self.successors:
            suc.init_vars(visited=visited, declarations=copy(self.declarations))
                
        return copy(self.defines)

    def init_consts(self, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)

        new_code = []
        consts = {}

        # iterate over code
        # look for consts and load them to temp registers
        for index in range(len(self.code)):
            ir = self.code[index]

            if isinstance(ir, irLoadConst):
                new_code.append(ir)
                continue

            ir_consts = [i for i in ir.get_input_vars() if i.is_const]
            for c in ir_consts:
                # check if const is loaded:
                if c.name not in consts:
                    # load const
                    target = add_const_temp(c.name, datatype=c.type, lineno=ir.lineno)
                    load = irLoadConst(target, copy(c), lineno=-1)
                    load.block = self

                    consts[c.name] = target

                    new_code.append(load)

                # replace var with const target register
                c.__dict__ = copy(consts[c.name].__dict__)

            new_code.append(ir)


        self.code = new_code

        # continue with successors:
        for suc in self.successors:
            suc.init_consts(visited=visited)

    def _loop_test_vars(self, loop):
        test_vars = [loop['test_var']]

        # test vars will be in the top block
        for ir in reversed(loop['top'].code):
            # skip phi nodes
            if isinstance(ir, irPhi):
                continue

            for o in ir.get_output_vars():
                if o in test_vars:
                    # output is a test var,
                    # so the inputs are dependent as well
                    test_vars.extend([i for i in ir.get_input_vars() if not i.is_const])

        return test_vars

    # def _loop_induction_vars(self, loop):
    #     # init induction vars with any vars
    #     # in the loop test vars that are modified
    #     # within the loop body
    #     for v in loop['test_vars']:
    #         pass


    # def _analyze_loops(self):
    #     # get loop entries/headers
    #     loops = self._loops_pass_1a()
    #     self._loops_pass_1b(loops)

    #     # fill out loop bodies
    #     for loop, info in loops.items():
    #         info['top']._loops_pass_2(info)
    #         self._loops_pass_3(info)

    #         # info['test_vars'] = self._loop_test_vars(info)
    #         # info['induction_vars'] = self._loop_induction_vars(info)

    #     return loops


    def _loops_find_header(self, loop, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)
        for ir in self.code:
            if isinstance(ir, irLoopHeader) and ir.name == loop:
                return self
                # we are a loop header

        for suc in self.successors:
            header = suc._loops_find_header(loop, visited)

            if header is not None:
                return header

        return None

    def analyze_copies_local(self):
        # local block only

        copy_in = {}
        copy_out = {}

        # init to empty sets
        for ir in self.code:
            copy_in[ir] = set()
            copy_out[ir] = set()

        prev_out = set()
        for ir in self.code:
            if ir not in copy_in:
                continue            
            
            gen = set()
            if isinstance(ir, irAssign) or isinstance(ir, irLoadConst):
                gen = set([ir])
        
            defines = ir.get_output_vars()

            kill = set()
            for copy_ir in copy_in[ir]:
                copy_vars = copy_ir.get_output_vars()
                copy_vars.extend(copy_ir.get_input_vars())

                for c in copy_vars:
                    if c in defines:
                        # this instruction kills these copies
                        kill = set([ir])
                        break

            copy_in[ir] = prev_out

            copy_out[ir] = (copy_in[ir] - kill) | gen
            
            prev_out = copy(copy_out[ir])

        self.copy_in = copy_in
        self.copy_out = copy_out
        
    ##############################################
    # Optimizer Passes
    ##############################################
    def fold_constants(self):
        new_code = []
       
        for ir in self.code:
            if isinstance(ir, irBinop):
                
                # attempt to fold
                val = ir.fold()

                if val is not None:
                    # replace with assign
                    ir = irLoadConst(ir.result, val, lineno=ir.lineno)
                    ir.block = self

            new_code.append(ir)

        self.code = new_code     

    def reduce_strength(self):
        new_code = []

        for ir in self.code:
            if isinstance(ir, irBinop):
                new_ir = ir.reduce_strength()

                if new_ir is not None:
                    new_ir.block = self
                    # replace instruction
                    ir = new_ir

            new_code.append(ir)

        self.code = new_code       


    def remove_redundant_binop_assigns(self):
        new_code = []
        nops = []

        # this is a peephole optimization
        for index in range(len(self.code) - 1):
            ir = self.code[index]
            next_ir = self.code[index + 1]

            # if a binop is followed by an assign:
            if isinstance(ir, irBinop) and isinstance(next_ir, irAssign):
                # and the binop's result is the input value for the assign:
                if ir.result.name == next_ir.value.name:
                    # just replace the binop result
                    ir.result = next_ir.target
                    new_code.append(ir)
                    nop = irNop(lineno=ir.lineno)
                    nop.block = self
                    self.code[index + 1] = nop
                    nops.append(nop)

                else:
                    new_code.append(ir)

            else:
                new_code.append(ir)

        # append last instruction (since loop will miss it)
        new_code.append(self.code[-1])

        # remove the nops we added:
        self.code = [n for n in new_code if n not in nops]

    def remove_dead_code(self, reads=None):
        new_code = []

        assert reads is not None

        for ir in self.code:
            is_read = False

            # check if this instruction writes to any vars which are read
            for o in ir.get_output_vars():
                if o.name in reads:
                    is_read = True
                    break

            # any of this instruction's outputs are eventually read:
            # if this instruction has no outputs, we will not remove it.
            if is_read or \
               len(ir.get_output_vars()) == 0 and \
               not isinstance(ir, irNop):

                # keep this instruction
                new_code.append(ir)

        self.code = new_code

    def remove_redundant_assigns(self):
        # remove redundant assigns in SSA form.
        remove = []

        for i in range(len(self.code)):
            ir = self.code[i]

            if isinstance(ir, irAssign) or isinstance(ir, irLoadConst):
                j = i + 1

                while j < len(self.code):
                    ir2 = self.code[j]

                    # check for assign target
                    if isinstance(ir2, irAssign) or isinstance(ir2, irLoadConst):
                        if ir2.target.name == ir.target.name:
                            # remove ir as it is redundant
                            remove.append(ir)
                            break

                        # if the assignment is used, then we are done here
                        elif ir.target in ir2.get_input_vars():
                            break

                    j += 1

        # remove instructions:
        self.code = [ir for ir in self.code if ir not in remove]

    # def local_value_numbering(self):
    #     next_val = self.func.current_value_num
        
    #     values = {}
    #     defines = {}

    #     for ir in self.code:
    #         if isinstance(ir, irLoadConst):                
    #             pass

    #         elif isinstance(ir, irAssign):
    #             target = ir.target
    #             values = [ir.value]
    #             op = '='

    #         else:
    #             continue


    #         target.ssa_version = next_val
    #         values[target] = next_val


    #         next_val += 1


    #     self.func.current_value_num = next_val
    #     self.local_values = values

    ##############################################
    # Transformation Passes
    ##############################################

    def resolve_phi(self, merge_number=0):
        # extract phis and remove from block code
        phis = []
        new_code = []
        for ir in self.code:
            if isinstance(ir, irPhi):
                phis.append(ir)
                
                # phi node will be removed from block
            else:
                new_code.append(ir)

        self.code = new_code

        # collect incoming blocks from all phis
        incoming_blocks = []
        defines = {}
        for phi in phis:
            defines[phi] = {}
            for d in phi.defines:

                """
                d.block is the source block where d is defined.
                However, that isn't the block we want - we want
                the direct predecessor on the path d was defined
                that leads into this block.

                To do the lookup, we can iterator over predecessors
                and look up d.  Since we might get multiple sources
                for d (since the lookup is on the non-SSA name),
                we will collect all sources with their predecessors
                and select the predecessor matching the SSA version.
                 

                """
                
                for p in self.predecessors:
                    v = p.lookup_var(d)
                    if v.name == d.name:
                        assert v not in defines[phi]
                            
                        defines[phi][v] = p

                        if p not in incoming_blocks:
                            incoming_blocks.append(p)

                # .... possibly we don't need merge blocks,
                # just place assigns on the predecessor blocks?
                # want to make sure the assign happens after the jump
                # though?
        
        merge_blocks = {}

        # insert a merge block for each incoming block
        for in_block in incoming_blocks:
            merge_block = irBlock(self.func, lineno=in_block.lineno - 1)
            merge_block.scope_depth = self.scope_depth

            label = irLabel(f'merge.{merge_number}_{self.name}', lineno=-1)
            merge_number += 1
            merge_block.append(label)


            # set exit jump to self block
            jump = irJump(self.code[0], lineno=-1)
            merge_block.append(jump)

            # replace jump on the incoming block to point to the merge block
            incoming_jump = in_block.code[-1]

            if isinstance(incoming_jump, irJump):
                incoming_jump.target = label

            elif isinstance(incoming_jump, irBranch):
                if incoming_jump.true_label.name == self.name:
                    incoming_jump.true_label = label

                if incoming_jump.false_label.name == self.name:
                    incoming_jump.false_label = label

            else:
                # last instruction in block is supposed to be a jump or branch
                assert False

            # rearrange block structure    
            in_block.successors.remove(self)
            in_block.successors.append(merge_block)
            merge_block.predecessors.append(in_block)
            merge_block.successors.append(self)
            self.predecessors.remove(in_block)
            self.predecessors.append(merge_block)

            merge_blocks[in_block] = merge_block

        for phi, phi_defines in defines.items():
            for var, pred in phi_defines.items():
                
                merge_block = merge_blocks[pred]

                ir = irAssign(phi.target, var, lineno=-1)
                ir.block = merge_block
                merge_block.code.insert(len(merge_block.code) - 1, ir)

        return merge_number        

    def add_phi(self, var, values):
        ir = irPhi(var, values, lineno=-1)
        ir.block = self
        self.temp_phis.append(ir)

    def add_incomplete_phi(self, var):
        ir = irIncompletePhi(var, self, lineno=-1)
        ir.block = self
        self.temp_phis.append(ir)

    def lookup_var(self, var, skip_local=False, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return None

        visited.append(self)

        if not isinstance(var, str):
            var_name = var._name

        else:
            var_name = var

        # check local block
        if var_name in self.defines and not skip_local:
            return self.defines[var_name]

        # if no predecessors
        elif len(self.predecessors) == 0:
            # look up fails
            raise KeyError(var_name)

        # if only one predecessor
        elif len(self.predecessors) == 1:
            # we enforce that at least one predecessor is filled before filling a block
            # we check that here.  since there is only one in this case, it has to be filled.
            assert self.predecessors[0].filled

            v = self.predecessors[0].lookup_var(var, visited=visited)
            return v

        # if block is sealed (all preds are filled)
        elif len([p for p in self.predecessors if not p.filled]) == 0:

            assert var._name not in self.defines

            values = []

            for p in self.predecessors:
                pv = p.lookup_var(var, visited=visited)

                if isinstance(pv, list):
                    for item in pv:
                        if item is not None:
                            values.append(item)

                elif pv is not None:
                    values.append(pv)

            values = list(set(values))

            # remove self reference
            if var in values:
                values.remove(var)

            if len(values) == 0:
                # no values left
                return var

            if len(values) == 1:
                return values[0]

            # multiple values
            # create new var and add a phi node
            new_var = irVar(name=var_name, lineno=-1)
            new_var.clone(var)
            new_var.block = self
            new_var.ssa_version = None
            new_var.convert_to_ssa()
            self.defines[var_name] = new_var

            self.add_phi(new_var, values)

            return new_var

        # if block is not sealed:
        elif len([p for p in self.predecessors if p.filled]) < len(self.predecessors):
            assert not self.sealed

            new_var = irVar(name=var_name, lineno=-1)
            new_var.clone(var)
            new_var.block = self
            new_var.ssa_version = None
            new_var.convert_to_ssa()
            self.defines[var_name] = new_var

            # this requires an incomplete phi which defines a new value
            self.add_incomplete_phi(new_var)

            return new_var

        else:
            assert False


    def seal(self):        
        if len(self.predecessors) == 0:
            # if no preds, we must be the start block
            self.sealed = True

        if self.sealed:
            return

        assert len(self.predecessors) > 0

        # check if all predecessors are filled
        if len([p for p in self.predecessors if not p.filled]) > 0:
            # at least one predecessor is unfilled
            return

        # get any incomplete phis and add them to the code
        for ir in [p for p in self.temp_phis if isinstance(p, irIncompletePhi)]:
            self.code.insert(1, ir)

        self.temp_phis = [p for p in self.temp_phis if not isinstance(p, irIncompletePhi)]

        # we can seal this block
        new_code = []
        for ir in self.code:
            if isinstance(ir, irIncompletePhi):
                values = []
                for p in self.predecessors:
                    v = p.lookup_var(ir.var)

                    if v is not None:
                        values.append(v)

                values = list(set(values))

                if ir.var in values: # remove self references
                    values.remove(ir.var)

                assert len(values) > 0

                phi = irPhi(ir.var, values, lineno=ir.lineno)
                phi.block = self

                new_code.append(phi)

            else:
                new_code.append(ir)

        self.code = new_code

        self.sealed = True

    def fill(self):
        if self.filled:
            return

        # are any predecessors filled?
        if len(self.predecessors) > 0 and \
           len([p for p in self.predecessors if p.filled]) == 0:
            return

        self.defines = {}

        new_code = []
        # start to fill block
        for ir in self.code:
            # look for defines and set their version to 0
            if isinstance(ir, irDefine):
                if ir.var._name in self.defines:
                    raise SyntaxError(f'Variable {ir.var._name} is already defined (variable shadowing is not allowed).', lineno=ir.lineno)

                assert ir.var.ssa_version is None

                ir.var.block = self
                ir.var.convert_to_ssa()
                
                self.defines[ir.var._name] = ir.var

            else:
                inputs = ir.local_input_vars

                for i in inputs:
                    i.block = self

                    # check if we have a definition:
                    try:
                        v = self.lookup_var(i)

                    except KeyError:
                        raise SyntaxError(f'Variable {i._name} is not defined.', lineno=ir.lineno)

                    i.clone(v)

                # look for writes to current set of vars and increment versions
                outputs = ir.local_output_vars

                for o in outputs:
                    o.block = self

                    # check if we have a definition:
                    try:
                        v = self.lookup_var(o)

                    except KeyError:
                        raise SyntaxError(f'Variable {o._name} is not defined.', lineno=ir.lineno)

                    o.clone(v)
                    o.ssa_version = None
                    o.convert_to_ssa()
                    
                    self.defines[o._name] = o
        
            new_code.append(ir)

        self.code = new_code

        self.filled = True                    

    def clean_up_phis(self):
        assert len([ir for ir in self.code if isinstance(ir, irIncompletePhi)]) == 0

        # insert any phis that are leftover
        for ir in self.temp_phis:
            # incomplete phis should already have been processed at this point
            assert isinstance(ir, irPhi)

            self.code.insert(1, ir)

        new_code = []
        for ir in self.code:
            if isinstance(ir, irPhi):

                # make sure list of defines is unique
                ir.defines = list(set(ir.defines))

                # assign type
                for d in ir.defines:
                    if d.type is not None:
                        ir.target.type = d.type

                        break

                # check if only one define
                # these convert trivially to assigns
                if len(ir.defines) == 1:
                    ir = irAssign(ir.target, ir.defines[0], lineno=ir.lineno)
                    ir.block = self

                # no defines?
                elif len(ir.defines) == 0:
                    continue # skip phi

            new_code.append(ir)

        self.code = new_code


class irFunc(IR):
    def __init__(self, name, ret_type='i32', params=None, body=None, builder=None, **kwargs):
        super().__init__(**kwargs)
        self.name = name
        self.ret_type = ret_type
        self.params = params
        self.body = [] # input IR
        self.code = [] # output IR
        self.builder = builder
        self.globals = builder.globals

        ZERO = irVar(0, 'i32', lineno=-1)
        ZERO.is_const = True

        self.consts = {'0': ZERO}
        
        if self.params == None:
            self.params = []

        # self.blocks = {}
        self.leader_block = None
        self.live_vars = None
        self.loops = {}

        self.live_in = None
        self.live_out = None

        self.ssa_next_val = {}

        # self.current_value_num = 0

    def get_zero(self, lineno=None):
        return self.consts['0']

    @property
    def blocks(self):
        return {b.name: b for b in self.leader_block.get_blocks()}

    @property
    def global_input_vars(self):
        v = []
        for block in self.blocks.values():
            v.extend(block.global_input_vars)

        return v

    @property
    def global_output_vars(self):
        v = []
        for block in self.blocks.values():
            v.extend(block.global_output_vars)

        return v

    @property
    def local_input_vars(self):
        v = []
        for block in self.blocks.values():
            v.extend(block.local_input_vars)

        return v

    @property
    def local_output_vars(self):
        v = []
        for block in self.blocks.values():
            v.extend(block.local_output_vars)

        return v

    def get_input_vars(self):
        v = []
        for block in self.blocks.values():
            v.extend(block.get_input_vars())

        return v
    
    def get_output_vars(self):
        v = []
        for block in self.blocks.values():
            v.extend(block.get_output_vars())

        return v

    @property
    def max_registers(self):
        if self.live_vars is None:
            return None

        max_live =0 
        for ir, live in self.live_vars.items():
            live = {l.name: None for l in live}

            if len(live) > max_live:
                max_live = len(live)

        return max_live

    @property
    def var_defines(self):
        d = {}

        for ir in self.get_code_from_blocks():
            for o in ir.get_output_vars():
                if o not in d:
                    d[o] = []

                d[o].append(ir)

        return d

    def __str__(self):
        params = params_to_string(self.params)

        s = "\n######## Line %4d       ########\n" % (self.lineno)
        s += "Func %s(%s) -> %s\n" % (self.name, params, self.ret_type)

        # s += "********************************\n"
        # s += "Locals:\n"
        # s += "********************************\n"

        # for v in self.locals.values():
        #     s += f'{v.lineno:3}\t{v.name:16}:{v.type}\n'

        s += "********************************\n"
        s += "Input Code:\n"
        s += "********************************\n"
        lines_printed = []
        for ir in self.body:
            if ir.lineno >= 0 and ir.lineno not in lines_printed and not isinstance(ir, irLabel):
                s += f'________________________________________________________\n'
                s += f' {ir.lineno}: {source_code[ir.lineno - 1].strip()}\n'
                lines_printed.append(ir.lineno)
    
            s += f'\t{ir}\n'

        s += "********************************\n"
        s += "Dominance:\n"
        s += "********************************\n"
        for n, dom in self.dominators.items():
            s += f'\t{n.name}\n'
            for d in dom:
                s += f'\t\t{d.name}\n'

        s += "********************************\n"
        s += "Dominator Tree:\n"
        s += "********************************\n"
        for n, dom in self.dominator_tree.items():
            s += f'\t{n.name}\n'
            if len(dom) == 0:
                s += '\t\tNone\n'
            else:
                for d in dom:
                    s += f'\t\t{d.name}\n'

        s += "********************************\n"
        s += "Loops:\n"
        s += "********************************\n"
        for loop, info in self.loops.items():
            s += f'{loop}\n'
            s += f'\tHeader: {info["header"].name}\n'
            s += f'\tTop:    {info["top"].name}\n'
            s += '\tBody:\n'
            for block in info['body']:
                s += f'\t\t\t{block.name}\n'

            s += '\tBody Vars:\n'
            for v in sorted(list(set([v.name for v in info['body_vars']]))):
                s += f'\t\t\t{v}\n'

        s += "********************************\n"
        s += "Blocks:\n"
        s += "********************************\n"

        # blocks = [self.blocks[k] for k in sorted(self.blocks.keys())]
        blocks = self.blocks.values()
        for block in blocks:
            s += f'{block}\n'

        s += "********************************\n"
        s += "Code:\n"
        s += "********************************\n"
        lines_printed = []
        for ir in self.code:
            if ir.lineno >= 0 and ir.lineno not in lines_printed and not isinstance(ir, irLabel):
                s += f'________________________________________________________\n'
                s += f' {ir.lineno}: {source_code[ir.lineno - 1].strip()}\n'
                lines_printed.append(ir.lineno)

            
            if self.live_vars:
                live = sorted(list(set([a.name for a in self.live_vars[ir]])))
                s += f'\t{str(ir):48}\tlive: {live}\n'

            else:
                s += f'\t{ir}\n'

        s += f'Max registers: {self.max_registers}\n'
        s += f'Instructions: {len([i for i in self.code if not isinstance(i, irLabel)])}\n'

        return s

    def create_block_from_code_at_label(self, label, prev_block=None, blocks=None):
        labels = self.labels()
        index = labels[label.name]

        # verify instruction at index is actually our label:
        assert self.body[index].name == label.name
        assert isinstance(self.body[index], irLabel)

        return self.create_block_from_code_at_index(index, prev_block=prev_block, blocks=blocks)

    def create_block_from_code_at_index(self, index, prev_block=None, blocks=None):
        if blocks is None:
            blocks = {}
            # blocks = self.blocks

        # check if we already have a block starting at this index
        if index in blocks:
            block = blocks[index]
            # check if prev_block is not already as predecessor:
            if prev_block not in block.predecessors:
                block.predecessors.append(prev_block)

            return block

        block = irBlock(func=self, lineno=self.body[index].lineno)
        block.scope_depth = self.body[index].scope_depth
        blocks[index] = block

        if prev_block:
            block.predecessors.append(prev_block)

        while True:
            ir = self.body[index]

            # check if label,
            # if so, this is an entry point for a new block,
            # possibly a backwards jump
            if isinstance(ir, irLabel) and (len(block.code) > 0):
                new_block = self.create_block_from_code_at_label(ir, prev_block=block, blocks=blocks)
                block.successors.append(new_block)

                # add a jump to this label in this block, this creates a clean
                # end point for this block
                jump = irJump(ir, lineno=ir.lineno)
                block.append(jump)

                break

            index += 1

            block.append(ir)

            if isinstance(ir, irBranch):
                # conditional branch choosing between 2 locations
                true_block = self.create_block_from_code_at_label(ir.true_label, prev_block=block, blocks=blocks)
                block.successors.append(true_block)

                false_block = self.create_block_from_code_at_label(ir.false_label, prev_block=block, blocks=blocks)
                block.successors.append(false_block)

                break

            elif isinstance(ir, irJump):
                # jump to a single location
                target_block = self.create_block_from_code_at_label(ir.target, prev_block=block, blocks=blocks)
                block.successors.append(target_block)
                break

            elif isinstance(ir, irReturn):
                # return from function
                break

        return block

    def calc_dominance(self):
        dominators = {self.leader_block: set([self.leader_block])}

        # blocks = [b for b in self.blocks.values() if b is not self.leader_block]
        blocks = list(self.blocks.values())
        for block in blocks:
            if block is self.leader_block:
                continue

            dominators[block] = set(copy(blocks))
        
        changed = True
        while changed:
            changed = False

            for block in blocks:
                predecessor_sets = []

                for pre in block.predecessors:
                    predecessor_sets.append(set(dominators[pre]))

                if len(predecessor_sets) == 0:
                    # leader block will not have predecessors
                    intersection = set()

                else:
                    # every other block:
                    intersection = predecessor_sets[0].intersection(*predecessor_sets[1:])

                new = set([block]).union(intersection)

                if dominators[block] != new:
                    changed = True

                dominators[block] = new

        # lookup: key is dominated by values

        return dominators

    def calc_strict_dominance(self, dominators):
        sdom = {}

        for node, dom in dominators.items():
            sdom[node] = copy(dom)
            sdom[node].remove(node)

        return sdom

    def calc_immediate_dominance(self, sdom):
        idom = {}

        # convert from "is-dominated_by" to "dominates"
        sdom2 = {}
        for n in self.blocks.values():
            sdom2[n] = []

        for node, dom in sdom.items():
            for d in dom:
                if node is not d:
                    sdom2[d].append(node)

        for n in self.blocks.values():
            idom[n] = None

            """
            The immediate dominator or idom of a node n is 
            the unique node that strictly dominates n but 
            does not strictly dominate any other node that 
            strictly dominates n. Every node, except the 
            entry node, has an immediate dominator.

            """

            sdoms = sdom[n] # all nodes that strictly dominate n
            # one of these will be the idom

            for d in sdoms:
                # does d strictly dominate any other node that
                # also strictly dominates n?

                d_doms = sdom2[d] # set of nodes that d strictly dominates

                skip = False
                for d2 in d_doms:
                    # does d2 strictly dominate n?
                    if n in sdom2[d2]:
                        skip = True
                        break

                if skip:
                    continue

                assert idom[n] is None
                idom[n] = d                

        assert idom[self.leader_block] is None

        return idom

    def calc_dominator_tree(self, dominators):
        sdom = self.calc_strict_dominance(dominators)
        idom = self.calc_immediate_dominance(sdom)

        # convert from "is-dominated_by" to "dominates"
        tree = {}
        for n in self.blocks.values():
            tree[n] = []

        for n in self.blocks.values():
            if idom[n] is None:
                continue

            tree[idom[n]].append(n)

        return tree

    # not sure if this works:
    # def calc_dominance_frontiers(self, dominators):
    #     df = {}

    #     sdom = self.calc_strict_dominance(dominators)

    #     for n in self.blocks.values():        
    #         df[n] = []
    #         for m in self.blocks.values():
    #             # is m in the DF of n?

    #             # m is in DF(n) if and only if:
    #             # 1. n does not strictly dominate m
    #             if m in sdom[n]: # is m in the set of nodes strictly dominated by n?
    #                 continue

    #             # AND:
    #             # 2. n dominates q where q is a predecessor of m
    #             for q in m.predecessors:
    #                 # does n dominate q?
    #                 # is q in the set of nodes dominated by n?
    #                 if q in dominators[n]:
    #                     df[n].append(m)              
    #                     break
    
    #     return df
    def get_successors(self, code=None):
        if code is None:
            code = self.get_code_from_blocks()

        succ = {}

        # init to empty
        for ir in code:
            succ[ir] = []

        # init successors
        for i in range(len(code)):
            ir = code[i]

            targets = ir.get_jump_target()

            if isinstance(ir, irReturn):
                # returns have no successor
                continue

            if targets is None:
                # no jump targets, successor is next instruction
                succ[ir] = [code[i + 1]]

            else:
                succ[ir].extend(targets)
        
        return succ

    def liveness_analysis(self):
        live_in = {}
        live_out = {}
        
        code = self.get_code_from_blocks()

        # init to empty sets
        for ir in code:
            live_in[ir] = set()
            live_out[ir] = set()

        succ = self.get_successors(code=code)

        iterations = 0
        iteration_limit = 512
        changed = True
        while changed and iterations < iteration_limit:
            iterations += 1
            changed = False
            prev_live_in = {}
            prev_live_out = {}

            for ir in reversed(code):
                prev_live_in[ir] = copy(live_in[ir])
                prev_live_out[ir] = copy(live_out[ir])

                in_vars = [a for a in ir.get_input_vars() if not a.is_const]
                out_vars = [a for a in ir.get_output_vars() if not a.is_const]

                use = set(in_vars)
                define = set(out_vars)

                live_in[ir] = use | (live_out[ir] - define)
                live_out[ir] = set()

                for s in succ[ir]:
                    live_out[ir] |= live_in[s]

            if prev_live_in != live_in:
                changed = True

            elif prev_live_out != live_out:
                changed = True

        self.live_in = live_in
        self.live_out = live_out

        logging.debug(f'Liveness analysis in {iterations} iterations')

        # top of function should not have anything live:
        assert len(self.live_in[code[0]]) == 0
        assert len(self.live_out[code[0]]) == 0


    def verify_block_assignments(self):
        # verify all instructions are recording their blocks:
        for block in self.blocks.values():
            for ir in block.code:
                try:
                    assert ir.block is block

                except AssertionError:

                    logging.critical(f'FATAL: {ir} from {block.name} does not have a block assignment.')
                    raise

    def verify_variables(self):
        # check that define instructions define the first version of a variable
        # this will catch if a variable reference is incorrectly placed somewhere else and modified.
        for block in self.blocks.values():
            for ir in block.code:
                if isinstance(ir, irDefine):
                    if ir.var.ssa_version != 0:
                        raise CompilerFatal(f'Variable {ir.var} at line {ir.lineno} modified out of place')


        # check that used vars have a define
        # this is just a basic check, it does not
        # perform a dataflow analysis
        outputs = self.local_output_vars
        for i in self.local_input_vars:
            if i not in outputs:
                raise CompilerFatal(f'Variable {i} used without define')


    def resolve_phi(self):
        merge_number = 0
        for block in self.blocks.values():
            merge_number = block.resolve_phi(merge_number)

    def convert_to_ssa(self):
        blocks = self.blocks.values()
        
        self.ssa_next_val = {}
        iterations = 0
        iteration_limit = 1024
        while (len([b for b in blocks if not b.filled]) > 0) or \
              (len([b for b in blocks if not b.sealed]) > 0):
              
            iterations += 1

            if iterations > iteration_limit:
                raise CompilerFatal(f'SSA conversion failed after {iterations} iterations')
                break

            for block in blocks:
                block.fill()
                block.seal()


        for block in blocks:
            assert block.filled
            assert block.sealed
        
        for block in blocks:
            block.clean_up_phis()

        logging.debug(f'SSA conversion in {iterations} iterations')

    def check_critical_edges(self):
        for block in self.blocks.values():
            # check if block has multiple successors
            if len(block.successors) <= 1:
                continue

            # check if any successors have multiple predecessors
            for s in block.successors:
                if len(s.predecessors) > 1:
                    raise CompilerFatal(f'Critical edge from {block.name} to {s.name}')

    def analyze_loops(self):
        self.loops = {}

        # reset loop membership
        for block in self.blocks.values():
            block.loops = []

        # dominator based algorithm
        # get back edges:
        back_edges = []
        for h in self.dominator_tree:
            for n in self.blocks.values():
                if h is not n and h in self.dominators[n] and h in n.successors:
                    # n dominated by h
                    back_edges.append((n, h))

                    # loop body is all nodes starting at h that reach n


        loop_bodies = []

        # https://www.csd.uwo.ca/~mmorenom/CS447/Lectures/CodeOptimization.html/node6.html        
        for back_edge in back_edges:
            n = back_edge[0]
            d = back_edge[1]
            loop_nodes = list(back_edge)
            stack = [n]

            while len(stack) > 0:
                m = stack.pop()

                for p in m.predecessors:
                    if p not in loop_nodes:
                        loop_nodes.append(p)
                        stack.append(p)

            loop_bodies.append(loop_nodes)

        loops = {}
        for body in loop_bodies:
            loop_name = None
            info = {
                'header': None,
                'top': None,
                'body': [],
                'body_vars': [],
                }


            for block in body:
                # look for loop top
                try:
                    loop_top = [a for a in block.code if isinstance(a, irLoopTop)][0]

                except IndexError:
                    continue

                loop_name = loop_top.name
                info['top'] = block
                info['body'] = body

                loops[loop_name] = info
                break

        for loop, info in loops.items():
            # search for headers
            info['header'] = self.leader_block._loops_find_header(loop)
            assert info['header'] is not None

            # add loop to blocks in body
            for block in info['body']:
                block.loops.append(loop)

        self.loops = loops

    def analyze_blocks(self):
        self.leader_block = self.create_block_from_code_at_index(0)
        
        self.leader_block.init_vars()
        self.leader_block.init_consts()

        self.dominators = self.calc_dominance()
        self.dominator_tree = self.calc_dominator_tree(self.dominators)


        # return

        self.convert_to_ssa()    

        # checks
        self.verify_block_assignments()
        self.verify_ssa()
        self.verify_variables();
        
        # loop analysis
        self.analyze_loops()
        
        # value numbering
        


        # optimizers
        optimize = False
        # optimize = True
        if optimize:
            # basic loop invariant code motion:
            self.loop_invariant_code_motion(self.loops)



        # return

        # convert out of SSA form
        self.resolve_phi()

        # blocks may be rearranged or added at this point

        self.dominators = self.calc_dominance()
        self.dominator_tree = self.calc_dominator_tree(self.dominators)

        # redo loop analysis
        self.analyze_loops()


        # checks
        # self.check_critical_edges()
        self.verify_block_assignments()



        # basic block merging (helps with jump elimination)
        # guide with simple branch prediction:
        # prioritize branches that occur within a loop





        # liveness

        self.liveness_analysis()

        # allocate registers

        self.code = self.get_code_from_blocks()

        # prune jumps



        return


        self.dominators = self.calc_dominance()
        self.dominator_tree = self.calc_dominator_tree(self.dominators)
        self.dominance_frontier = self.calc_dominance_frontiers(self.dominators)

        # verify all instructions are assigned to a block:
        # THIS WILL FAIL ON BREAK STATEMENTS AT THE END OF A LOOP!
        # for ir in self.body:
        #     if ir.block is None:
        #         raise SyntaxError(f'Unreachable code.', lineno=ir.lineno)

        # self.body = None

        # verify all blocks start with a label and end
        # with an unconditional jump or return
        for block in self.blocks.values():
            assert isinstance(block.code[0], irLabel)
            assert isinstance(block.code[-1], irControlFlow)

        global_vars = self.leader_block.init_vars()
        return
        ssa_vars = self.leader_block.rename_vars(ssa_vars=global_vars)
        self.leader_block.apply_types()
        self.leader_block.insert_phi()

        self.verify_block_assignments()

        self.verify_ssa()


        optimize = True

        if optimize:
            for block in self.blocks.values():
                block.remove_redundant_binop_assigns()

            # for block in self.blocks.values():
            #     block.fold_constants()

            # for block in self.blocks.values():
            #     block.reduce_strength()

            
            # self.leader_block.propagate_copies()

        self.verify_block_assignments()

        self.leader_block.init_consts()
        
        self.verify_block_assignments()

        # if optimize:
        #     for block in self.blocks.values():
        #         reads = [a.name for a in self.get_input_vars()]
        #         block.remove_dead_code(reads=reads)

        self.loops = self.leader_block.analyze_loops()

        optimize = False
        if optimize:
            # basic loop invariant code motion:
            self.loop_invariant_code_motion(self.loops)


            # common subexpr elimination?



            # for block in self.blocks.values():
            #     # remove redundant assignments.
            #     # loop invariant code motion can create these
                # block.remove_redundant_assigns()

        # self.verify_ssa()


        # convert out of SSA form
        # self.resolve_phi()



        # self.analyze_copies()

        # optimize = True
        # if optimize:
        #     for block in self.blocks.values():
        #         block.remove_redundant_binop_assigns()

        #     for block in self.blocks.values():
        #         block.remove_redundant_assigns()

            # self.leader_block.propagate_copies()

            # for block in self.blocks.values():
            #     block.local_value_numbering()

            # self.propagate_copies()



        #     for block in self.blocks.values():
        #         block.fold_constants()

        #     for block in self.blocks.values():
        #         block.reduce_strength()


            # remove dead code:
            # for block in self.blocks.values():
            #     reads = [a.name for a in self.get_input_vars()]
            #     block.remove_dead_code(reads=reads)

        # self.liveness_analysis()

        # DO NOT MODIFY BLOCK CODE BEYOND THIS POINT!


        # reassemble code
        self.code = self.get_code_from_blocks()
        
        if optimize:
            self.prune_jumps()

            # jump threading...


        self.prune_no_ops()
        self.remove_dead_labels()

        # self.deconstruct_ssa();

        # register allocator



    def get_code_from_blocks(self):
        code = []

        for block in self.blocks.values():
            # attach loop data to instructions
            for ir in block.code:
                ir.loops = self.loops.keys()

            # attach code
            code.extend(block.code)

        return code

    def verify_ssa(self):
        writes = {}
        for ir in self.get_code_from_blocks():
            for o in [o for o in ir.get_output_vars() if not o.is_const and not o.is_temp]:
                try:
                    assert o.name not in writes

                except AssertionError:
                    logging.critical(f'FATAL: {o.name} defined by {writes[o.name]} at line {writes[o.name].lineno}, overwritten by {ir} at line {ir.lineno}')

                    raise

                try:
                    assert o.ssa_version is not None

                except AssertionError:
                    logging.critical(f'FATAL: {o.name} not assigned to SSA. Line {ir.lineno}')

                    raise

                writes[o.name] = ir

            for i in [i for i in ir.get_input_vars() if not i.is_const and not i.is_temp]:
                try:
                    assert i.ssa_version is not None

                except AssertionError:
                    logging.critical(f'FATAL: {i.name} not assigned to SSA. Line {ir.lineno}')

                    raise

    def prune_jumps(self):
        new_code = []

        # this is a peephole optimization
        # note we skip the last instruction in the loop, since
        # we check current + 1 in the peephole
        for index in range(len(self.code) - 1):
            ir = self.code[index]
            next_ir = self.code[index + 1]

            # if a jump followed by a label:
            if isinstance(ir, irJump) and isinstance(next_ir, irLabel):
                # check if label is jump target:
                if ir.target.name == next_ir.name:
                    # skip this jump
                    pass          

                else:
                    new_code.append(ir)

            else:
                new_code.append(ir)

        # append last instruction (since loop will miss it)
        new_code.append(self.code[-1])

        self.code = new_code

    # remove all no-op instructions
    def prune_no_ops(self):
        new_code = []

        for index in range(len(self.code)):
            ir = self.code[index]
            
            if not ir.is_nop:
                new_code.append(ir)

        self.code = new_code

    def deconstruct_ssa(self):
        # convert all SSA variables back to single
        # representation
        for ir in self.code:
            for i in ir.get_input_vars():
                i.ssa_version = None

            for o in ir.get_output_vars():
                o.ssa_version = None

                new_code = []

        # remove phi nodes
        for index in range(len(self.code)):
            ir = self.code[index]
            
            if not isinstance(ir, irPhi):
                new_code.append(ir)

        self.code = new_code

    def loop_invariant_code_motion(self, loops):
        for loop, info in loops.items():
            header_code = []

            entry_dominators = self.dominators[info['top']]
            
            for block in info['body']:
                block_code = []

                for index in range(len(block.code)):
                    ir = block.code[index]
                    block_code.append(ir)

                    # check if the instruction's block dominates the loop entry node
                    if ir.block not in entry_dominators:
                        continue

                    if isinstance(ir, irBinop):
                        # check if inputs are loop invariant

                        # for now, just check for consts, until we have a reaching def
                        if ir.left.is_const and ir.right.is_const:
                            # move instruction to header
                            header_code.append(ir)

                            # remove from block code
                            block_code.remove(ir)

                            logging.debug(f'LICM: loop {loop} moving [{ir}] to loop header')

                    elif isinstance(ir, irAssign) or isinstance(ir, irLoadConst):
                        if ir.value.is_const:
                            # move instruction to header
                            header_code.append(ir)

                            # remove from block code
                            block_code.remove(ir)

                            logging.debug(f'LICM: loop {loop} moving [{ir}] to loop header')

                block.code = block_code

            if len(header_code) > 0:
                header = info['header']
                insert_index = None
                
                # search for header:
                for index in range(len(header.code)):
                    ir = header.code[index]

                    if isinstance(ir, irLoopHeader):
                        insert_index = index + 1
                        break

                # add code to loop header
                for ir in header_code:
                    ir.block = header
                    header.code.insert(insert_index, ir)
                    insert_index += 1

    def propagate_copies(self):
        iterations = 0
        iteration_limit = 128
        changed = True
        while changed and iterations < iteration_limit:
            changed = self.leader_block.propagate_copies()

            iterations += 1

        logging.debug(f'Propagate copies in {iterations} iterations')        
    
    def analyze_copies(self):
        for block in self.blocks.values():
            block.analyze_copies_local()

        # from:
        # https://www.csd.uwo.ca/~mmorenom/CS447/Lectures/CodeOptimization.html/node8.html

        code = self.get_code_from_blocks()

        all_copies = []
        for ir in code:
            if isinstance(ir, irAssign) or isinstance(ir, irLoadConst):
                all_copies.append(ir)

        all_copies = set(all_copies)

        block_copies_in = {}
        block_copies_out = {}

        for block in self.blocks.values():
            block_copies_in[block] = copy(all_copies)
            block_copies_out[block] = set()

        block_copies_in[self.leader_block] = set()
        block_copies_out[self.leader_block] = set(list(self.leader_block.copy_out.values())[-1])

        changed = True
        while changed:
            changed = False

            for block in self.blocks.values():
                copies_in = None
                # set copies in from predecessors outs
                for pre in block.predecessors:
                    pre_copies_in = set(list(pre.copy_in.values())[-1])

                    if copies_in is None:
                        copies_in = pre_copies_in

                    else:
                        # intersection
                        copies_in &= pre_copies_in

                if copies_in is None:
                    copies_in = set()

                # gen is the copies out from this block
                gen = list(block.copy_out.values())[-1]

                # kills are all copies (from all points in the function)
                kills = []
                defines = block.get_output_vars()

                for ir in code:
                    if ir in block.code:
                        continue

                    copy_vars = ir.get_output_vars()
                    copy_vars.extend(ir.get_input_vars())

                    for c in copy_vars:
                        if c in defines:
                            kills.append(ir)
                            break

                kills = set(kills)

                # union
                copies_out = gen | (copies_in - kills) 

                if copies_in != block_copies_in[block]:
                    changed = True

                if copies_out != block_copies_out[block]:
                    changed = True

                block_copies_in[block] = copies_in
                block_copies_out[block] = copies_out



    def remove_dead_labels(self):
        labels = self.labels()

        keep = []
        
        # get list of labels that are used
        for label in labels:
            for node in self.code:
                targets = node.get_jump_target()
                if targets is None:
                    continue
                    
                for target in targets:
                    if target != None and target.name == label:
                        keep.append(label)
                            

        dead_labels = [l for l in labels if l not in keep]

        new_code = []
        for ir in self.code:
            if isinstance(ir, irLabel):
                if ir.name in dead_labels:
                    continue

            new_code.append(ir)
        
        self.code = new_code

    def labels(self):
        labels = {}

        for i in range(len(self.body)):
            ins = self.body[i]

            if isinstance(ins, irLabel):
                labels[ins.name] = i

        return labels

    def append_node(self, ir):
        self.body.append(ir)

    @property
    def prev_node(self):
        return self.body[-1]

class irPhi(IR):
    def __init__(self, target, defines=[], **kwargs):
        super().__init__(**kwargs)

        self.target = target
        self.defines = defines

        assert self.target not in defines

    def __str__(self):
        s = ''
        for d in sorted(self.defines, key=lambda a: a.name):
            s += f'{d.name}[{d.block.name}], '

        s = s[:-2]

        s = f'{self.target} = PHI({s})'

        return s

    def get_input_vars(self):
        return copy(self.defines)

    def get_output_vars(self):
        return [self.target]

class irIncompletePhi(IR):
    def __init__(self, var, block, **kwargs):
        super().__init__(**kwargs)
        self.var = var
        self.block = block

    def __str__(self):
        return f'{self.var} = Incomplete PHI() @ {self.block.name}'

class irNop(IR):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.is_nop = True

    def __str__(self):
        return "NOP" 

class irDefine(IR):
    def __init__(self, var, **kwargs):
        super().__init__(**kwargs)
        self.var = var
        self.is_nop = True
    
    def __str__(self):
        return f'DEF: {self.var} depth: {self.scope_depth}'

class irLoopHeader(IR):
    def __init__(self, name, **kwargs):
        super().__init__(**kwargs)
        self.name = name
        self.is_nop = True

    def __str__(self):
        return f'LoopHeader: {self.name}'

class irLoopTop(IR):
    def __init__(self, name, **kwargs):
        super().__init__(**kwargs)
        self.name = name
        self.is_nop = True

    def __str__(self):
        return f'LoopTop: {self.name}'

class irLabel(IR):
    def __init__(self, name, **kwargs):
        super().__init__(**kwargs)        
        self.name = name

        self.loop_top = None
        self.loop_end = None
    
    @property
    def is_loop(self):
        return self.loop_top is not None and self.loop_end is not None

    @property
    def is_loop_top(self):
        return self.loop_top == self

    @property
    def is_loop_end(self):
        return self.loop_end == self

    def __str__(self):
        s = 'LABEL %s' % (self.name)

        return s

    def generate(self):
        return insLabel(self.name, lineno=self.lineno)

class irControlFlow(IR):
    pass

class irBranch(irControlFlow):
    def __init__(self, value, true_label, false_label, **kwargs):
        super().__init__(**kwargs)        
        self.value = value
        self.true_label = true_label
        self.false_label = false_label

    def __str__(self):
        s = 'BR %s -> T: %s | F: %s' % (self.value, self.true_label.name, self.false_label.name)

        return s    

    def get_input_vars(self):
        return [self.value]

    def get_jump_target(self):
        return [self.true_label, self.false_label]

class irJump(irControlFlow):
    def __init__(self, target, **kwargs):
        super().__init__(**kwargs)        
        self.target = target

    def __str__(self):
        s = 'JMP -> %s' % (self.target.name)

        return s    

    # def generate(self):
        # return insJmp(self.target.generate(), lineno=self.lineno)

    def get_jump_target(self):
        return [self.target]


class irReturn(irControlFlow):
    def __init__(self, ret_var, **kwargs):
        super().__init__(**kwargs)
        self.ret_var = ret_var

    def __str__(self):
        return "RET %s" % (self.ret_var)

    # def generate(self):
        # return insReturn(self.ret_var.generate(), lineno=self.lineno)

    def get_input_vars(self):
        return [self.ret_var]


class irAssign(IR):
    def __init__(self, target, value, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.value = value

        assert not self.value.is_const
        
    def __str__(self):
        if isinstance(self.target, irRef):
            target = f'*{self.target}'
        else:
            target = f'{self.target}'

        if isinstance(self.value, irRef):
            value = f'*{self.value}'
        else:
            value = f'{self.value}'

        return f'{target} = {value}'

    def get_input_vars(self):
        return [self.value]

    def get_output_vars(self):
        if isinstance(self.target, irRef):
            return []

        else:
            return [self.target]

    # def generate(self):
    #     return insMov(self.target.generate(), self.value.generate(), lineno=self.lineno)

class irLookup(IR):
    def __init__(self, result, target, indexes, **kwargs):
        super().__init__(**kwargs)        
        self.result = result
        self.target = target
        self.indexes = indexes

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            if isinstance(index, irAttribute):
                indexes += '.%s' % (index.name)
            else:
                indexes += '[%s]' % (index.name)

        s = '%s = LOOKUP %s%s' % (self.result, self.target, indexes)

        return s

    def get_input_vars(self):
        i = [self.target]
        i.extend(self.indexes)

        return i

    def get_output_vars(self):
        return [self.result]


# Load constant to register
class irLoadConst(IR):
    def __init__(self, target, value, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.value = copy(value)
        
        assert value.is_const

    def __str__(self):
        return f'LOAD CONST {self.target} <-- {self.value}'

    def get_input_vars(self):
        return [self.value]

    def get_output_vars(self):
        return [self.target]


# Load register from memory
class irLoad(IR):
    def __init__(self, target, ref, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.ref = ref
        
    def __str__(self):
        return f'LOAD {self.target} <-- {self.ref}'

    def get_input_vars(self):
        # return [self.ref]
        return []

    def get_output_vars(self):
        return [self.target]


# Store register to memory
class irStore(IR):
    def __init__(self, register, ref, **kwargs):
        super().__init__(**kwargs)
        self.register = register
        self.ref = ref
        
    def __str__(self):
        return f'STORE {self.register} --> {self.ref}'

    def get_input_vars(self):
        return [self.register]

    def get_output_vars(self):
        # return [self.ref]
        return []


# Spill register to stack
class irSpill(IR):
    def __init__(self, target, ref, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.ref = ref
        
    def __str__(self):
        return f'SPILL {self.target} >>> {self.ref}'

    def get_input_vars(self):
        return [self.ref]

    def get_output_vars(self):
        return [self.target]

# Fill register from stack
class irFill(IR):
    def __init__(self, target, ref, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.ref = ref
        
    def __str__(self):
        return f'FILL {self.target} <<< {self.ref}'

    def get_input_vars(self):
        return [self.ref]

    def get_output_vars(self):
        return [self.target]


class irBinop(IR):
    def __init__(self, target, op, left, right, **kwargs):
        super().__init__(**kwargs)
        self.target = target
        self.op = op

        # canonicalize the binop expression, if possible:
        if op in COMMUTATIVE_OPS:
            # check if one side is a const, if so,
            # make sure it is on the right to make
            # common subexpressions easier to find.
            # this only applies to operations which are commutative
            # if op in COMMUTATIVE_OPS and isinstance(left, irConst) and not isinstance(right, irConst):
            if left.is_const and not right.is_const:
                temp = left
                left = right
                right = temp

            else:
                # place them in sorted order by name
                temp = list(sorted([left, right], key=lambda a: a.name))
                left = temp[0]
                right = temp[1]

                # NOTE:
                # it *looks* like Python's parser already does something like this.
                # however, we are not going to rely on that behavior, and will ensure 
                # it is done to our specs here.

        self.left = left
        self.right = right

        self.data_type = self.target.type

        if self.op == 'div' and self.right.is_const and self.right.value == 0:
            raise SyntaxError("Division by 0", lineno=self.lineno)

    def __str__(self):
        s = '%s = %s %s %s' % (self.target, self.left, self.op, self.right)

        return s

    def get_input_vars(self):
        return [self.left, self.right]

    def get_output_vars(self):
        return [self.target]

    def reduce_strength(self):
        if self.op == 'add':
            # add to 0 is just an assign
            if self.left.is_const and self.left.value == 0:
                return irAssign(self.target, self.right, lineno=self.lineno)

            elif self.right.is_const and self.right.value == 0:
                return irAssign(self.target, self.left, lineno=self.lineno)

        elif self.op == 'sub':
            # sub 0 from var is just an assign
            if self.right.is_const and self.right.value == 0:
                return irAssign(self.target, self.left, lineno=self.lineno)
        
        elif self.op == 'mul':
            # mul times 0 is assign to 0
            if self.left.is_const and self.left.value == 0:
                return irAssign(self.target, self.get_zero(self.lineno), lineno=self.lineno)

            elif self.right.is_const and self.right.value == 0:
                return irAssign(self.target, self.get_zero(self.lineno), lineno=self.lineno)

            # mul times 1 is assign
            elif self.left.is_const and self.left.value == 1:
                return irAssign(self.target, self.right, lineno=self.lineno)

            elif self.right.is_const and self.right.value == 1:
                return irAssign(self.target, self.left, lineno=self.lineno)

        elif self.op == 'div':
            # div by 1 is assign
            if self.right.is_const and self.right.value == 1:
                return irAssign(self.target, self.left, lineno=self.lineno)

        return None

    def fold(self):
        val = None
        op = self.op
        left = self.left
        right = self.right

        if not left.is_const or not right.is_const:
            return None

        if op == 'eq':
            val = left.value == right.value

        elif op == 'neq':
            val = left.value != right.value

        elif op == 'gt':
            val = left.value > right.value

        elif op == 'gte':
            val = left.value >= right.value

        elif op == 'lt':
            val = left.value < right.value

        elif op == 'lte':
            val = left.value <= right.value

        elif op == 'logical_and':
            val = left.value and right.value

        elif op == 'logical_or':
            val = left.value or right.value

        elif op == 'add':
            val = left.value + right.value

        elif op == 'sub':
            val = left.value - right.value

        elif op == 'mul':
            val = left.value * right.value

            if left.type == 'f16':
                val /= 65536

        elif op == 'div':
            if left.type == 'f16':
                val = (left.value * 65536) / right.value

            else:
                val = left.value / right.value

        elif op == 'mod':
            val = left.value % right.value

        else:
            assert False

        v = irVar(val, lineno=self.lineno)
        v.is_const = True

        return v

    # def generate(self):
    #     ops = {
    #         'i32':
    #             {'eq': insCompareEq,
    #             'neq': insCompareNeq,
    #             'gt': insCompareGt,
    #             'gte': insCompareGtE,
    #             'lt': insCompareLt,
    #             'lte': insCompareLtE,
    #             'logical_and': insAnd,
    #             'logical_or': insOr,
    #             'add': insAdd,
    #             'sub': insSub,
    #             'mul': insMul,
    #             'div': insDiv,
    #             'mod': insMod},
    #         'f16':
    #             {'eq': insF16CompareEq,
    #             'neq': insF16CompareNeq,
    #             'gt': insF16CompareGt,
    #             'gte': insF16CompareGtE,
    #             'lt': insF16CompareLt,
    #             'lte': insF16CompareLtE,
    #             'logical_and': insF16And,
    #             'logical_or': insF16Or,
    #             'add': insF16Add,
    #             'sub': insF16Sub,
    #             'mul': insF16Mul,
    #             'div': insF16Div,
    #             'mod': insF16Mod},
    #         'str':
    #             # placeholders for now
    #             # need actual string instructions
    #             {'eq': insBinop, # compare
    #             'neq': insBinop, # compare not equal
    #             'in': insBinop,  # search
    #             'add': insBinop, # concatenate
    #             'mod': insBinop} # formatted output
    #     }

    #     return ops[self.data_type][self.op](self.target.generate(), self.left.generate(), self.right.generate(), lineno=self.lineno)




class irVar(IR):
    def __init__(self, name=None, datatype=None, **kwargs):
        super().__init__(**kwargs)
        self._name = str(name)
        self.type = datatype

        self.is_global = False
        self.is_temp = False
        self.holds_const = False
        self.is_const = False
        self.ssa_version = None

    def __hash__(self):
        return hash(self.name)

    def __eq__(self, other):
        return self.name == other.name  

    def convert_to_ssa(self):
        assert self.ssa_version is None
        assert self.block is not None

        if self._name not in self.block.func.ssa_next_val:
            self.block.func.ssa_next_val[self._name] = 0

        self.ssa_version = self.block.func.ssa_next_val[self._name]
        self.block.func.ssa_next_val[self._name] += 1

    def clone(self, source):
        assert self._name == source._name

        # clone source attributes to self
        self.ssa_version = source.ssa_version
        self.type = source.type

    @property
    def value(self):
        assert self.is_const

        if self.name == 'True':
            return 1
        elif self.name == 'False':
            return 0
        elif self.type == 'f16':
            val = float(self.name)
            if val > 32767.0 or val < -32767.0:
                raise SyntaxError("Fixed16 out of range, must be between -32767.0 and 32767.0", lineno=kwargs['lineno'])

            return int(val * 65536)

        elif self.is_const:
            if self.name.startswith('$'):
                return int(self.name[1:])

            return int(self.name)

        else:
            return None
            
    @property
    def name(self):
        if self.is_temp or self.ssa_version is None:
            return self._name

            # if self.is_const:
            #     return self._name

            # return f'{self._name}/{str(id(self))[-3:]}'
        
        return f'{self._name}.{self.ssa_version}'

    @name.setter
    def name(self, value):
        self._name = value        

    def __str__(self):
        if self.type:
            s = f"{self.name}:{self.type}"

        else:
            s = f"{self.name}"

        if self.is_global:
            return f'Global({s})'

        elif self.is_temp:
            # return f'Temp{s}'
            return f'{s}'

        elif self.is_const:
            return f'Const({s})'

        else:
            # return f'Var{s}'            
            return f'{s}'

    def __repr__(self):
        return f'{self.name}/{id(self)}'

class irRef(irVar):
    def __init__(self, target, ref_count, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if isinstance(target, irRef):
            self.target = target.target

        else:
            self.target = target

        self.ref_count = ref_count
        self.is_temp = True

    @property
    def name(self):
        return f'&{self.target._name}_{self.ref_count}'

    def __str__(self):
        if self.type:
            return "Ref(%s:%s)" % (self.name, self.type)

        else:
            return "Ref(%s)" % (self.name)

class irAttribute(irVar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)    

        self.is_temp = True

    def __str__(self):    
        return "Attr(%s)" % (self.name)


class irAssert(IR):
    def __init__(self, value, **kwargs):
        super(irAssert, self).__init__(**kwargs)        
        self.value = value

    def get_input_vars(self):
        return [self.value]

    def __str__(self):
        s = 'ASSERT %s' % (self.value)

        return s   

