
import logging
from copy import copy, deepcopy


# debug imports:
import sys
from pprint import pprint
# /debug

source_code = []

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


class IR(object):
    def __init__(self, lineno=None):
        self.lineno = lineno
        self.block = None
        self.scope_depth = None
        self.live_in = []
        self.live_out = []
        self.is_nop = False

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

        self.live_in = {}
        self.live_out = {}

        self.entry_label = None
        self.jump_target = None

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

            if self.func.live_vars:
                s += f'{ir_s}\n'
                s += f'{depth}|\t  def:  {sorted(list(set([a.name for a in self.func.defined_vars[ir]])))}\n'
                s += f'{depth}|\t  use:  {sorted(list(set([a.name for a in self.func.used_vars[ir]])))}\n'
                s += f'{depth}|\t  live: {sorted(list(set([a.name for a in self.func.live_vars[ir]])))}\n'

            else:
                s += f'{ir_s}\n'

        s += f'{depth}|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n'

        return s

    def __repr__(self):
        return self.name

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

    @property
    def _used(self):
        used = {}
        prev = []
    
        for ir in reversed(self.code):
            if ir not in used:
                used[ir] = []

            used[ir].extend(prev)

            for i in ir.get_input_vars():
                # if i.is_const:
                    # continue

                if i not in used[ir]:
                    used[ir].append(i)

            prev = copy(used[ir])

            for o in ir.get_output_vars():
                if o in prev:
                    prev.remove(o)

        return used
        
    ##############################################
    # Analysis Passes
    ##############################################
    
    def init_global_vars(self, visited=None):    
        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)

        self.stores = {}
        self.defines = {}
        self.declarations = copy(self.globals)
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
                assign = irLoadConst(ir.var, self.func.get_zero(ir.lineno), lineno=ir.lineno)
                
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
                    new_code.append(ir)

            new_code.append(self.code[index])

        self.code = new_code

        # continue with successors:
        for suc in self.successors:
            suc.init_global_vars(visited=visited)
                
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
        # look for defines and record them.
        # also insert inits to 0
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

                    consts[c.name] = target

                    new_code.append(load)

                # replace var with const target register
                c.__dict__ = copy(consts[c.name].__dict__)

            new_code.append(ir)


        self.code = new_code

        # continue with successors:
        for suc in self.successors:
            suc.init_consts(visited=visited)


    def rename_vars(self, ssa_vars=None, visited=None):
        if ssa_vars is None:
            ssa_vars = {}

        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)
        
        self.params = {} # variables required at the beginning of the block
        self.uses = {}

        for index in range(len(self.code)):
            ir = self.code[index]

            # look for defines and set their version to 0
            if isinstance(ir, irDefine):
                if ir.var._name in ssa_vars:
                    raise SyntaxError(f'Variable {ir.var._name} is already defined (variable shadowing is not allowed).', lineno=ir.lineno)

                assert ir.var.ssa_version is None

                ir.var.ssa_version = 0

                # set current SSA version of this variable
                ssa_vars[ir.var._name] = ir.var

                ir.var.block = self
                self.defines[ir.var._name] = ir.var

            else:
                # look for writes to current set of vars and increment versions
                outputs = ir.local_output_vars

                for o in outputs:
                    o.block = self
                    self.defines[o._name] = o

                    if o._name in ssa_vars:
                        o.ssa_version = ssa_vars[o._name].ssa_version + 1
                        ssa_vars[o._name] = o

                    else:
                        raise SyntaxError(f'Variable {o._name} is not defined.', lineno=ir.lineno)

                inputs = ir.local_input_vars

                for i in inputs:
                    if i._name not in self.uses:
                        self.uses[i._name] = []

                    ds = self.get_defined(i._name)

                    if len(ds) == 0:
                        raise SyntaxError(f'Variable {i._name} is not defined.', lineno=ir.lineno)

                    elif len(ds) == 1:
                        # take previous definition for the input
                        i.__dict__ = copy(ds[0].__dict__)
                        i.is_global = False

                    else:
                        # this indicates we'll need a phi to reconcile the 
                        # values, but we'll get that on the phi conversion step
                        pass

                # set block parameters
                # unless we are a leader block
                if len(self.predecessors) > 0:
                    inputs = [i for i in ir.get_input_vars() if not i.is_temp and not i.is_const]

                    for i in inputs:
                        if i._name in self.params:
                            continue

                        # set SSA version for param
                        if i._name in ssa_vars:
                            i.ssa_version = ssa_vars[i._name].ssa_version + 1
                            ssa_vars[i._name] = i

                        else:
                            assert False

                        self.params[i._name] = i
                        self.defines[i._name] = i

        # continue with successors:
        for suc in self.successors:
            suc.rename_vars(ssa_vars, visited)

        return ssa_vars

    def insert_phi(self, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return
        
        visited.append(self)

        insertion_point = None
        for i in range(len(self.code)):
            index = i

            if not isinstance(self.code[index], irLabel) and \
               not isinstance(self.code[index], irLoopHeader): # and \
               # not isinstance(self.code[index], irLoopEntry):
                insertion_point = index
                break

        assert insertion_point is not None

        for k, v in self.params.items():
            sources = []
            for pre in self.predecessors:
                ds = pre.get_defined(k, visited=[self])
                sources.extend(ds)
    
            ir = irPhi(v, sources, lineno=-1)
            
            self.code.insert(insertion_point, ir)

        for suc in self.successors:
            suc.insert_phi(visited)


    def apply_types(self, visited=None, declarations=None):
        if visited is None:
            visited = []

        if self in visited:
            return
        
        visited.append(self)

        if declarations is None:
            declarations = {}

        declarations.update(self.declarations)

        # scan forward and assign types from declared variables to
        # their SSA instances
        for ir in self.code:
            variables = ir.local_input_vars
            variables.extend(ir.local_output_vars)

            for v in variables:
                if v._name not in declarations:
                    raise Exception

                v.type = declarations[v._name].type


        # scan in reverse for assignments,
        # if they are sourced from a temp register, apply type
        # to that register.
        for i in range(len(self.code)):
            index = (len(self.code) - 1) - i

            ir = self.code[index]

            if isinstance(ir, irAssign):
                if ir.target.type != None and ir.value.is_temp and ir.value.type is None:
                    ir.value.type = ir.target.type

        for suc in self.successors:
            suc.apply_types(visited, declarations)

    def used(self, used=None, visited=None, edge=None, include_consts=False):
        def merge_used(used1, used2):
            used = copy(used1)
        
            for k2, v2 in used2.items():
                if k2 in used:
                    for v in v2:
                        if v not in used[k2]:
                            used[k2].append(v)

                else:
                    used[k2] = copy(v2)

            return used

        if visited is None:
            visited = []

        if edge is not None and edge in visited:
            return {}

        visited.append(edge)
            
        if used is None:
            used = {}

        
        if not include_consts:
            _used = {}
            for k, v in self._used.items():
                _used[k] = [a for a in v if not a.is_const]

        else:
            _used = self._used

        used = merge_used(used, _used)

        if isinstance(edge, Edge):
            from_used = used[edge.from_node.code[0]]

            for ir in self.code:
                for v in from_used:
                    if v not in used[ir]:
                        used[ir].append(v)

        for pre in self.predecessors:
            edge = Edge(self, pre)
            pre_used = pre.used(used=used, visited=visited, edge=edge)

            used = merge_used(used, pre_used)

        return used

    def defined(self, defined=None, prev=None, visited=None, edge=None, include_consts=False):
        if visited is None:
            visited = []

        if edge is not None and edge in visited:
            return {}

        visited.append(edge)

        if defined is None:
            defined = {}

        if prev is None:
            prev = []

        consts = []

        for ir in self.code:
            if ir not in defined:
                defined[ir] = []

            # add define for any input consts, if not already defined
            if include_consts:
                for i in ir.get_input_vars():
                    if i.is_const and i not in consts:
                        consts.append(i)
                        defined[ir].append(i)

            for d in prev:
                if d not in defined[ir]:
                    defined[ir].append(d)

            for o in ir.get_output_vars():
                if o.is_const and not include_consts:
                    continue

                if o not in defined[ir]:
                    defined[ir].append(o)

            # phi kills definitions on its inputs (as they are replaced by the phi's output)
            if isinstance(ir, irPhi):
                for d in ir.defines:
                    if d in defined[ir]:
                        defined[ir].remove(d)

            prev = defined[ir]


        # look for phi nodes.
        # phi defines are live starting at top of block:
        phi_defines = {}
        for ir in self.code:
            if isinstance(ir, irPhi):
                phi_defines[ir] = []
                for d in ir.defines:
                    if d not in phi_defines:
                        phi_defines[ir].append(d)

        # apply phi defines from top down to phi node
        for ir in self.code:
            # check if this is a phi, if so,
            # apply it's defines and then remove it from consideration
            if ir in phi_defines:
                for d in phi_defines[ir]:
                    if d not in defined[ir]:
                        defined[ir].append(d)

                del phi_defines[ir]

            else:                
                for phi, defines in phi_defines.items():
                    for d in defines:
                        if d not in defined[ir]:
                            defined[ir].append(d)


        # continue with successors:
        for suc in self.successors:
            edge = Edge(self, suc)
            suc.defined(defined, prev, visited, edge)

        return defined

    def liveness(self, used, defined, live=None, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)

        if live is None:
            live = {}

        # intersection of defined and used for liveness:
        for ir in self.code:
            if ir not in live:
                live[ir] = []

            for d in defined[ir]:
                if d in used[ir]:
                    if d not in live[ir]:
                        live[ir].append(d)

        # continue with successors:
        for suc in self.successors:
            suc.liveness(used, defined, live, visited)

        return live

    def _loops_pass_1(self, loops=None, visited=None):
        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)

        if loops is None:
            loops = {}

        for ir in self.code:
            if isinstance(ir, irLoopHeader):
                loops[ir.name] = {
                    'header': self,
                    'entry': None,
                    'end': None,
                    'body': [],
                    'body_vars': [],
                }

                # we are a loop header

                # we should have 1 successors:
                assert len(self.successors) == 1

                # the header's successor is the loop body
                # the loop body should have 2 predecessors:
                loop_top = self.successors[0]

                # the loop body should have 2 predecessors, one is the header (us)
                # the other is the loop entry node

                # if it only has 1 predecessor:
                if len(loop_top.predecessors) == 1:
                    # so we're not actually a loop.

                    # possibly a break statement that jumps out of the loop
                    # without a conditional block
                    del loops[ir.name]

                    # while we're here, delete the loop header
                    self.code = [ir for ir in self.code if not isinstance(ir, irLoopHeader)]

                    break


                assert len(loop_top.predecessors) == 2
                entry_node = [a for a in loop_top.predecessors if a is not self][0]

                # and the entry node of the loop is that successor
                loops[ir.name]['entry'] = entry_node

        for suc in self.successors:
            suc._loops_pass_1(loops, visited)                

        return loops

    def _loops_pass_2(self, loop, visited=None):
        if visited is None:
            # make sure start node is loop entry
            assert loop['entry'] is self
            visited = []

        if self in visited:
            return

        visited.append(self)

        # iterate over all predecessors, skipping the loop header.
        # this will record every node in the loop
        for pre in self.predecessors:
            if pre is loop['header']:
                continue

            pre._loops_pass_2(loop, visited=visited)

        if self not in loop['body']:
            if self is loop['entry']:
                # place entry at start of list
                # this just makes the list easier to read
                loop['body'].insert(0, self)
            
            else:
                loop['body'].append(self)

            # record vars used in the loop body:
            input_vars = self.get_input_vars()
            for i in input_vars:
                if i not in loop['body_vars']:
                    loop['body_vars'].append(i)

    def _loops_pass_3(self, loop):
        entry_node = loop['entry']
        loop['end'] = None

        for suc in entry_node.successors:
            if suc is not loop['header'] and suc not in loop['body']:
                assert loop['end'] is None                
                loop['end'] = suc

    def _loop_test_vars(self, loop):
        test_vars = [loop['test_var']]

        # test vars will be in the entry block
        for ir in reversed(loop['entry'].code):
            # skip phi nodes
            if isinstance(ir, irPhi):
                continue

            for o in ir.get_output_vars():
                if o in test_vars:
                    # output is a test var,
                    # so the inputs are dependent as well
                    test_vars.extend([i for i in ir.get_input_vars() if not i.is_const])

        return test_vars

    def _loop_induction_vars(self, loop):
        # init induction vars with any vars
        # in the loop test vars that are modified
        # within the loop body
        for v in loop['test_vars']:
            pass


    def analyze_loops(self):
        # get loop entries/headers
        loops = self._loops_pass_1()

        # fill out loop bodies
        for loop, info in loops.items():
            info['entry']._loops_pass_2(info)
            self._loops_pass_3(info)

            # info['test_vars'] = self._loop_test_vars(info)
            # info['induction_vars'] = self._loop_induction_vars(info)

        return loops

    ##############################################
    # Optimizer Passes
    ##############################################
    def fold_and_propagate_constants(self):
        replaced = False
        new_code = []
        aliases = {}

        for ir in self.code:
            if isinstance(ir, irBinop):
                if ir.left.name in aliases:
                    replaced = True
                    ir.left = aliases[ir.left.name]

                if ir.right.name in aliases:
                    replaced = True
                    ir.right = aliases[ir.right.name]

                # attempt to fold
                val = ir.fold()

                if val is not None:
                    # replace with assign
                    ir = irLoadConst(ir.result, val, lineno=ir.lineno)

                    replaced = True
            
            if isinstance(ir, irAssign):
                # if assigning a constant, record
                # that association with the target var
                # if ir.value.is_const:
                aliases[ir.target.name] = ir.value

                # if this value is aliased, then we
                # can just assign the constant value directly
                # instead of the temp var
                if ir.value.name in aliases:
                    replaced = True
                    ir.value = aliases[ir.value.name]
                    aliases[ir.target.name] = ir.value

            elif isinstance(ir, irReturn):
                if ir.ret_var.name in aliases:
                    replaced = True
                    ir.ret_var = aliases[ir.ret_var.name]

            new_code.append(ir)

        self.code = new_code       

        return replaced

    def reduce_strength(self):
        new_code = []

        for ir in self.code:
            if isinstance(ir, irBinop):
                new_ir = ir.reduce_strength()

                if new_ir is not None:
                    # replace instruction
                    ir = new_ir

            new_code.append(ir)

        self.code = new_code       


    def remove_redundant_binop_assigns(self):
        new_code = []

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
                    self.code[index + 1] = irNop(lineno=ir.lineno)

            else:
                new_code.append(ir)

        # append last instruction (since loop will miss it)
        new_code.append(self.code[-1])

        self.code = new_code

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
                        if ir2.target._name == ir.target._name:
                            # remove ir as it is redundant
                            remove.append(ir)
                            break

                        # if the assignment is used, then we are done here
                        elif ir.target in ir2.get_input_vars():
                            break

                    j += 1

        # remove instructions:
        self.code = [ir for ir in self.code if ir not in remove]


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

        self.blocks = {}
        self.leader_block = None
        self.live_vars = None
        self.loops = {}

    def get_zero(self, lineno=None):
        return self.consts['0']

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
        s += "Loops:\n"
        s += "********************************\n"
        for loop, info in self.loops.items():
            s += f'{loop}\n'
            s += f'\tHeader: {info["header"].name}\n'
            s += f'\tEntry:  {info["entry"].name}\n'
            s += '\tBody:\n'
            for block in info['body']:
                s += f'\t\t\t{block.name}\n'

            s += '\tBody Vars:\n'
            for v in sorted(list(set([v.name for v in info['body_vars']]))):
                s += f'\t\t\t{v}\n'

            s += f'\tEnd:    {info["end"].name}\n'

        s += "********************************\n"
        s += "Blocks:\n"
        s += "********************************\n"

        blocks = [self.blocks[k] for k in sorted(self.blocks.keys())]
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

    def create_block_from_code_at_label(self, label, prev_block=None):
        labels = self.labels()
        index = labels[label.name]

        # verify instruction at index is actually our label:
        assert self.body[index].name == label.name
        assert isinstance(self.body[index], irLabel)

        return self.create_block_from_code_at_index(index, prev_block=prev_block)

    def create_block_from_code_at_index(self, index, prev_block=None):
        # check if we already have a block starting at this index
        if index in self.blocks:
            block = self.blocks[index]
            # check if prev_block is not already as predecessor:
            if prev_block not in block.predecessors:
                block.predecessors.append(prev_block)

            return block

        block = irBlock(func=self, lineno=self.body[index].lineno)
        block.scope_depth = self.body[index].scope_depth
        self.blocks[index] = block

        if prev_block:
            block.predecessors.append(prev_block)

        while True:
            ir = self.body[index]

            # check if label,
            # if so, this is an entry point for a new block,
            # possibly a backwards jump
            if isinstance(ir, irLabel) and (len(block.code) > 0):
                new_block = self.create_block_from_code_at_label(ir, prev_block=block)
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
                true_block = self.create_block_from_code_at_label(ir.true_label, prev_block=block)
                block.successors.append(true_block)

                false_block = self.create_block_from_code_at_label(ir.false_label, prev_block=block)
                block.successors.append(false_block)

                break

            elif isinstance(ir, irJump):
                # jump to a single location
                target_block = self.create_block_from_code_at_label(ir.target, prev_block=block)
                block.successors.append(target_block)
                break

            elif isinstance(ir, irReturn):
                # return from function
                break

        return block

    def defined(self):
        return self.leader_block.defined()

    def used(self):
        # run used on each terminator node
        used = {}
        for block in self.blocks.values():
            if block.is_terminator:
                used = block.used(used=used, edge=block) 

        return used

    def liveness(self, used, defined):
        # verify used and defined:
        
        # an instruction that uses a var should have it defined
        # by that point:

        e = None

        for ir in used:
            for v in [a for a in ir.get_input_vars() if not a.is_const]:
                try:
                    assert v in defined[ir]

                except AssertionError as exc:   
                    if e is None: 
                        e = exc

                    logging.critical(f'FATAL: {v} not defined, used at {ir}. Func: {self.name} Line: {ir.lineno}')
             
        self.live_vars = self.leader_block.liveness(used, defined)

        if e:
            raise e

        return self.live_vars

    def analyze_blocks(self):
        self.blocks = {}
        self.leader_block = self.create_block_from_code_at_index(0)

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

        global_vars = self.leader_block.init_global_vars()
        ssa_vars = self.leader_block.rename_vars(ssa_vars=global_vars)
        self.leader_block.apply_types()
        self.leader_block.insert_phi()


        optimize = True

        if optimize:
            for block in self.blocks.values():
                block.remove_redundant_binop_assigns()

            for block in self.blocks.values():
                block.fold_and_propagate_constants()

            for block in self.blocks.values():
                block.reduce_strength()


        self.leader_block.init_consts()


        if optimize:
            for block in self.blocks.values():
                reads = [a.name for a in self.get_input_vars()]
                block.remove_dead_code(reads=reads)

        self.loops = self.leader_block.analyze_loops()

        if optimize:
            # basic loop invariant code motion:
            self.loop_invariant_code_motion(self.loops)

            # common subexpr elimination?



            # for block in self.blocks.values():
            #     # remove redundant assignments.
            #     # loop invariant code motion can create these
            #     block.remove_redundant_assigns()

            # remove dead code:
            for block in self.blocks.values():
                reads = [a.name for a in self.get_input_vars()]
                block.remove_dead_code(reads=reads)


        # run usedef analysis
        defined = self.defined()
        used = self.used()
        
        self.live_vars = None

        self.used_vars = used
        self.defined_vars = defined

        # liveness analysis
        live = self.liveness(used, defined)

        self.live_vars = live

        # DO NOT MODIFY BLOCK CODE BEYOND THIS POINT!

        # reassemble code
        self.code = self.get_code_from_blocks()

        self.verify_ssa()
        
        if optimize:
            self.prune_jumps()

            # jump threading...


        self.prune_no_ops()
        self.remove_dead_labels()

        self.deconstruct_ssa();

        # register allocator



    def get_code_from_blocks(self):
        code = []

        for block in self.blocks.values():
            code.extend(block.code)

        return code

    def verify_ssa(self):
        writes = {}
        for ir in self.code:
            for o in ir.get_output_vars():
                try:
                    assert o.name not in writes

                except AssertionError:
                    logging.critical(f'FATAL: {o.name} defined by {writes[o.name]} at line {writes[o.name].lineno}, overwritten by {ir} at line {ir.lineno}')

                    raise

                writes[o.name] = ir

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
            
            for block in info['body']:
                block_code = []

                for index in range(len(block.code)):
                    ir = block.code[index]
                    block_code.append(ir)

                    if isinstance(ir, irBinop):
                        # check if inputs are loop invariant

                        # for now, just check for consts, until we have a reaching def
                        if ir.left.is_const and ir.right.is_const:
                            # move instruction to header
                            header_code.append(ir)

                            # remove from block code
                            block_code.remove(ir)

                    elif isinstance(ir, irAssign) or isinstance(ir, irLoadConst):
                        if ir.value.is_const:
                            # move instruction to header
                            header_code.append(ir)

                            # remove from block code
                            block_code.remove(ir)

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
                    header.code.insert(insert_index, ir)
                    insert_index += 1

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

        assert self.target not in self.defines

    def __str__(self):
        s = ''
        for d in self.defines:
            s += f'{d.name}, '

        s = s[:-2]

        s = f'{self.target} = PHI({s})'

        return s

    def get_input_vars(self):
        return copy(self.defines)

    def get_output_vars(self):
        return [self.target]

class irNop(IR):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.is_nop = True

    def __str__(self, **kwargs):
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
    def __init__(self, result, op, left, right, **kwargs):
        super().__init__(**kwargs)
        self.result = result
        self.op = op
        self.left = left
        self.right = right

        self.data_type = self.result.type

        if self.op == 'div' and self.right.is_const and self.right.value == 0:
            raise SyntaxError("Division by 0", lineno=self.lineno)

    def __str__(self):
        s = '%s = %s %s %s' % (self.result, self.left, self.op, self.right)

        return s

    def get_input_vars(self):
        return [self.left, self.right]

    def get_output_vars(self):
        return [self.result]

    def reduce_strength(self):
        if self.op == 'add':
            # add to 0 is just an assign
            if self.left.is_const and self.left.value == 0:
                return irAssign(self.result, self.right, lineno=self.lineno)

            elif self.right.is_const and self.right.value == 0:
                return irAssign(self.result, self.left, lineno=self.lineno)

        elif self.op == 'sub':
            # sub 0 from var is just an assign
            if self.right.is_const and self.right.value == 0:
                return irAssign(self.result, self.left, lineno=self.lineno)
        
        elif self.op == 'mul':
            # mul times 0 is assign to 0
            if self.left.is_const and self.left.value == 0:
                return irAssign(self.result, self.get_zero(self.lineno), lineno=self.lineno)

            elif self.right.is_const and self.right.value == 0:
                return irAssign(self.result, self.get_zero(self.lineno), lineno=self.lineno)

            # mul times 1 is assign
            elif self.left.is_const and self.left.value == 1:
                return irAssign(self.result, self.right, lineno=self.lineno)

            elif self.right.is_const and self.right.value == 1:
                return irAssign(self.result, self.left, lineno=self.lineno)

        elif self.op == 'div':
            # div by 1 is assign
            if self.right.is_const and self.right.value == 1:
                return irAssign(self.result, self.left, lineno=self.lineno)

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

    #     return ops[self.data_type][self.op](self.result.generate(), self.left.generate(), self.right.generate(), lineno=self.lineno)




class irVar(IR):
    def __init__(self, name=None, datatype=None, **kwargs):
        super().__init__(**kwargs)
        self._name = str(name)
        self.type = datatype

        self.is_global = False
        # self._is_global_modified = False
        self.is_temp = False
        self.holds_const = False
        self.is_const = False
        self.ssa_version = None

    # @property
    # def is_global_modified(self):
    #     return self._is_global_modified

    # @is_global_modified.setter
    # def is_global_modified(self, value):
    #     if value:
    #         assert self.is_global

    #     self._is_global_modified = value

    def __hash__(self):
        # if self.ssa_version is None:
        #     return hash(id(self))

        # else:
        #     return hash(self.name)

        return hash(self.name)

    def __eq__(self, other):
        # if self.ssa_version is None:
        #     return super().__eq__(other)

        # else:
        #     return self.name == other.name   
        return self.name == other.name   

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
        
        return f'{self._name}.v{self.ssa_version}'

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

