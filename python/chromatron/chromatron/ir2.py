
from copy import copy


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

        assert self.lineno != None

    def generate(self):
        return BaseInstruction()

    def get_input_vars(self):
        return []

    def get_output_vars(self):
        return []

    def get_jump_target(self):
        return None

class irBlock(IR):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.predecessors = []
        self.successors = []
        self.code = []
        self.locals = {}
        self.defines = {}
        self.uses = {}
        self.params = {}

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

        s += f'{depth}| In:\n'
        for i in self.params.values():
            s += f'{depth}|\t{i.name}: {i.type}\n'
        s += f'{depth}| Out:\n'
        for i in self.defines.values():
            s += f'{depth}|\t{i.name}: {i.type}\n'

        lines_printed = []
        s += f'{depth}| Code:\n'
        for ir in self.code:
            if ir.lineno >= 0 and ir.lineno not in lines_printed and not isinstance(ir, irLabel):
                s += f'{depth}|________________________________________________________\n'
                s += f'{depth}| {ir.lineno}: {depth}{source_code[ir.lineno - 1].strip()}\n'
                lines_printed.append(ir.lineno)

            s += f'{depth}|\t{ir}\n'

        s += f'{depth}|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n'

        return s

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

    def rename_vars(self, ssa_vars=None, visited=None):
        if ssa_vars is None:
            ssa_vars = {}

        if visited is None:
            visited = []

        if self in visited:
            return

        visited.append(self)
        
        self.defines = {}
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
                outputs = [o for o in ir.get_output_vars() if not o.is_temp and not o.is_const]

                for o in outputs:
                    o.block = self
                    self.defines[o._name] = o

                    if o._name in ssa_vars:
                        o.ssa_version = ssa_vars[o._name].ssa_version + 1
                        ssa_vars[o._name] = o

                    else:
                        raise SyntaxError(f'Variable {o._name} is not defined.', lineno=ir.lineno)

                inputs = [i for i in ir.get_input_vars() if not i.is_temp and not i.is_const]

                for i in inputs:
                    if i._name not in self.uses:
                        self.uses[i._name] = []

                    ds = self.get_defined(i._name)

                    if len(ds) == 0:
                        raise SyntaxError(f'Variable {i._name} is not defined.', lineno=ir.lineno)

                    elif len(ds) == 1:
                        # take previous definition for the input
                        i.__dict__ = copy(ds[0].__dict__)

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

            if not isinstance(self.code[index], irLabel):
                insertion_point = index
                break

        assert insertion_point is not None

        for k, v in self.params.items():
            sources = []
            for pre in self.predecessors:
                ds = pre.get_defined(k)
                sources.extend(ds)

            if len(sources) == 1:
                # if a single source, just use an assign
                ir = irAssign(v, sources[0], lineno=-1)

            else:
                ir = irPhi(v, sources, lineno=-1)
            
            self.code.insert(insertion_point, ir)

        for suc in self.successors:
            suc.insert_phi(visited)

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
    def input_vars(self):
        v = {}
        for node in self.code:
            inputs = node.get_input_vars()

            for i in inputs:
                if i.name not in v:
                    v[i.name] = i

        return [a for a in v.values() if not a.is_temp and not a.is_const]

    @property
    def output_vars(self):
        v = {}
        for node in self.code:
            outputs = node.get_output_vars()

            for o in outputs:
                if o.name not in v:
                    v[o.name] = o

        return [a for a in v.values() if not a.is_temp and not a.is_const]



class irFunc(IR):
    def __init__(self, name, ret_type='i32', params=None, body=None, builder=None, **kwargs):
        super().__init__(**kwargs)
        self.name = name
        self.ret_type = ret_type
        self.params = params
        self.body = []
        
        if self.params == None:
            self.params = []

        self.blocks = {}
        self.leader_block = None


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
        s += "Blocks:\n"
        s += "********************************\n"

        blocks = [self.blocks[k] for k in sorted(self.blocks.keys())]
        for block in blocks:
            s += f'{block}\n'
    
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

        block = irBlock(lineno=self.body[index].lineno)
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
                # we are branching to two locations:
                # 1. Fallthrough
                # This is the next instruction in the list.
                # 2. Jump target
                # This code be anywhere, even behind.
                # fallthrough_block = self.create_block_from_code_at_index(index, prev_block=block)
                true_block = self.create_block_from_code_at_label(ir.true_label, prev_block=block)
                block.successors.append(true_block)

                false_block = self.create_block_from_code_at_label(ir.false_label, prev_block=block)
                block.successors.append(false_block)

                # block.successors.append(fallthrough_block)
                

                # get successor parameters and add move instructions
                # to load our values to those parameters
                # what if we don't use those values at all?
                # need a method to ask for them from predecessors.
                # fallthrough_block_params = fallthrough_block.get_params()
                # target_block_params = target_block.params()

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


    def analyze_blocks(self):
        self.blocks = {}
        self.leader_block = self.create_block_from_code_at_index(0)

        # return
        
        # verify all instructions are assigned to a block:
        # for ir in self.body:
            # assert ir.block is not None

        # verify all blocks start with a label and end
        # with an unconditional jump or return
        for block in self.blocks.values():
            assert isinstance(block.code[0], irLabel)
            assert isinstance(block.code[-1], irBranch) or isinstance(block.code[-1], irJump) or isinstance(block.code[-1], irReturn)


        ssa_vars = self.leader_block.rename_vars()
        self.leader_block.insert_phi()



    # def remove_dead_labels(self):
    #     return
    #     labels = self.labels()

    #     keep = []
    #     code = self.code()

    #     # get list of labels that are used
    #     for label in labels:
    #         for node in code:
    #             target = node.get_jump_target()

    #             if target != None:
    #                 if target.name == label:
    #                     keep.append(label)
    #                     break

    #     dead_labels = [l for l in labels if l not in keep]
        
    #     self.root_block.remove_dead_labels(dead_labels)

    def labels(self):
        labels = {}

        for i in range(len(self.body)):
            ins = self.body[i]

            if isinstance(ins, irLabel):
                labels[ins.name] = i

        return labels

    def code(self):
        return self.body

    def append_node(self, ir):
        self.body.append(ir)


class irPhi(IR):
    def __init__(self, target, defines=[], **kwargs):
        super().__init__(**kwargs)

        self.target = target
        self.defines = defines

    def __str__(self):
        s = ''
        for d in self.defines:
            s += f'{d.name}, '

        s = s[:-2]

        s = f'{self.target} = PHI({s})'

        return s


class irDefine(IR):
    def __init__(self, var, **kwargs):
        super().__init__(**kwargs)
        self.var = var
    
    def __str__(self):
        return f'DEF: {self.var} depth: {self.scope_depth}'

    def get_output_vars(self):
        return [self.var]


class irLabel(IR):
    def __init__(self, name, **kwargs):
        super().__init__(**kwargs)        
        self.name = name
        # list of jumps that arrive at this label
        self.sources = []

    def __str__(self):
        s = 'LABEL %s' % (self.name)

        return s

    def generate(self):
        return insLabel(self.name, lineno=self.lineno)


class irBranch(IR):
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
        return self.target

class irJump(IR):
    def __init__(self, target, **kwargs):
        super().__init__(**kwargs)        
        self.target = target

    def __str__(self):
        s = 'JMP -> %s' % (self.target.name)

        return s    

    # def generate(self):
        # return insJmp(self.target.generate(), lineno=self.lineno)

    def get_jump_target(self):
        return self.target


class irReturn(IR):
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
        super(irAssign, self).__init__(**kwargs)
        self.target = target
        self.value = value
        
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

class irIndex(IR):
    def __init__(self, result, target, index, **kwargs):
        super().__init__(**kwargs)        
        self.result = result
        self.target = target
        self.index = index

    def __str__(self):
        s = '%s = INDEX %s[%s]' % (self.result, self.target, self.index)

        return s

    def get_input_vars(self):
        return [self.target, self.index]

    def get_output_vars(self):
        return [self.result]


class irAttr(IR):
    def __init__(self, result, target, attr, **kwargs):
        super().__init__(**kwargs)        
        self.result = result
        self.target = target
        self.attr = attr

    def __str__(self):
        s = '%s = ATTR %s.%s' % (self.result, self.target, self.attr)

        return s

    def get_input_vars(self):
        # return [self.target, self.attr]
        return [self.target]

    def get_output_vars(self):
        return [self.result]


class irBinop(IR):
    def __init__(self, result, op, left, right, **kwargs):
        super(irBinop, self).__init__(**kwargs)
        self.result = result
        self.op = op
        self.left = left
        self.right = right

        self.data_type = self.result.type

    def __str__(self):
        s = '%s = %s %s %s' % (self.result, self.left, self.op, self.right)

        return s

    def get_input_vars(self):
        return [self.left, self.right]

    def get_output_vars(self):
        return [self.result]

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
        self._name = name
        self.type = datatype

        self.is_temp = False
        self.is_const = False
        self.ssa_version = None

    @property
    def name(self):
        if self.is_temp or self.ssa_version is None:
            return self._name
        
        return f'{self._name}_v{self.ssa_version}'

    @name.setter
    def name(self, value):
        self._name = value        

    def __str__(self):
        if self.type:
            return "Var(%s:%s)" % (self.name, self.type)

        else:
            return "Var(%s)" % (self.name)

        
class irConst(irVar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # if self.name == 'True':
        #     self.value = 1
        # elif self.name == 'False':
        #     self.value = 0
        # elif self.type == 'f16':
        #     val = float(self.name)
        #     if val > 32767.0 or val < -32767.0:
        #         raise SyntaxError("Fixed16 out of range, must be between -32767.0 and 32767.0", lineno=kwargs['lineno'])

        #     self.value = int(val * 65536)
        # else:
        #     self.value = int(self.name)

        # self.default_value = self.value

        self.is_const = True

    def __str__(self):
        if self.type:
            return "Const(%s:%s)" % (self.name, self.type)

        else:
            return "Const(%s)" % (self.name)
   
class irTemp(irVar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.is_temp = True

    def __str__(self):
        if self.type:
            return "Temp(%s:%s)" % (self.name, self.type)

        else:
            return "Temp(%s)" % (self.name)

class irRef(irTemp):
    def __init__(self, target, ref_count, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if isinstance(target, irRef):
            self.target = target.target

        else:
            self.target = target

        self.ref_count = ref_count

    @property
    def name(self):
        return f'&{self.target._name}_{self.ref_count}'

    def __str__(self):
        if self.type:
            return "Ref(%s:%s)" % (self.name, self.type)

        else:
            return "Ref(%s)" % (self.name)


