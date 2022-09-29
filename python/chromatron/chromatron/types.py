
import struct
from copy import deepcopy

from .exceptions import *
from .instructions2 import insReg, insAddr
from .opcode import OpcodeObject, OpcodeFunc, OpcodeString

class VarContainer(object):
    def __init__(self, var):
        self.var = var 
        self.reg = None # what register is signed to this container 
        self.is_container = True
        self.force_used = False
        
    @property
    def var(self):
        return self._var

    @var.setter
    def var(self, value):
        assert isinstance(value, Var)
        self._var = value

    def copy(self):
        return deepcopy(self)

    def convert_to_ssa(self, ssa_vals={}):
        self.var = self.var.copy()

        if self.var.name not in ssa_vals:
            ssa_vals[self.var.name] = 0

        self.var.ssa_version = ssa_vals[self.var.name]
        ssa_vals[self.var.name] += 1

    def __eq__(self, other):
        try:
            return self.ssa_name == other.ssa_name

        except AttributeError:
            return self.ssa_name == other

    def __hash__(self):
        return hash(self.ssa_name)

    def __str__(self):
        base = f'{self.var}'

        if self.is_global:
            base = f'~{base}'

        if self.reg is not None:
            return f'{base}@{self.reg}'

        else:
            return f'{base}'

    def __getattribute__(self, name):
        try:
            return super().__getattribute__(name)

        except AttributeError as e:
            pass

        # this will come up during a copy,
        # var won't have been assigned to the class yet,
        # so the getattr on var will recursively fail.
        if "_var" not in self.__dict__:
            raise AttributeError

        return getattr(self.var, name)

    def __setattr__(self, name, value):
        if name in ['var', '_var', 'reg', 'is_container']:
            super().__setattr__(name, value)

        else:
            setattr(self.var, name, value)

    def generate(self):
        if self.reg is None:
            raise CompilerFatal(f"{self} does not have a register. Line: {self.lineno}")

        return insReg(self.reg, self, lineno=self.lineno)

class Var(object):
    def __init__(self, name=None, data_type=None, lineno=None):
        self._name = name
        self.addr = None # assigned address of this variable
        self.data_type = data_type
        self.lineno = lineno

        self.value = None
        self.init_val = None
        self._is_global = False
        self.is_container = False
        self.is_allocatable = True
        self.is_phi_merge = False
        self.keywords = {}

        self.ssa_version = None

    def copy(self):
        return deepcopy(self)

    @property
    def id_debug(self):
        return id(self)

    def __setattr__(self, name, value):
        if name in ['var', 'block']:
            raise Exception

        super().__setattr__(name, value)

    def __eq__(self, other):
        try:
            return self.ssa_name == other.ssa_name

        except AttributeError:
            return self.ssa_name == other

    def __hash__(self):
        return hash(self.ssa_name)

    @property
    def name(self):
        # if self.const:
            # return f'${self._name}'

        return f'{self._name}'

    @name.setter
    def name(self, value):
        self._name = value

    @property
    def ssa_name(self):
        if self.ssa_version is not None:
            return f'{self.name}.v{self.ssa_version}'        

        else:
            return self.name

    @property
    def is_scalar(self):
        return False

    @property
    def is_global(self):
        return self._is_global

    @is_global.setter
    def is_global(self, value):
        self._is_global = value

    @property
    def const(self):  # flag to indicate if variable is a constant
        if self.value is not None:
            return True

        return False

    @property
    def scalar_type(self):
        return self.data_type

    @property
    def size(self): # size in machine words (32 bits)
        return None

    @property
    def dimensions(self):
        return 0

    def build(self, name, lineno=None, **kwargs):
        base = deepcopy(self)
        base.name = name
        base.lineno = lineno

        return base

    def __str__(self):
        if self.addr is None:
            s = f'{self.ssa_name}({self.data_type})'

        else:
            s = f'{self.ssa_name}({self.data_type})@0x{self.addr}'

        if self.const:
            s = f'${s}'

        if self.is_phi_merge:
            s = f'{s}[phi]'

        return s

    def lookup(self, indexes=[], lineno=None):
        return self

    def generate(self):
        if self.addr is None:
            raise CompilerFatal(f"{self} does not have an address. Line: {self.lineno}")

        return self.addr.generate()

class varRegister(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    @property
    def size(self):
        return 1 # register vars are always size 1

class varScalar(varRegister):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        if self.data_type is None:
            self.data_type = 'var'

    def lookup(self, indexes=[], lineno=None): # lookup returns address offset and type (self, for scalars)
        return self

    @property
    def is_scalar(self):
        return True

class varInt32(varScalar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='i32', **kwargs)

class varFixed16(varScalar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='f16', **kwargs)

class varGfx16(varScalar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='gfx16', **kwargs)

class varOffset(varRegister):
    def __init__(self, *args, offset=None, **kwargs):
        super().__init__(*args, data_type='offset', **kwargs)
        self.offset = offset
        self.target = None
        
    def __str__(self):
        return f'{super().__str__()}'

class varComposite(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varObject(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.is_allocatable = False

class varPixelArray(varObject):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varFunction(varObject):
    def __init__(self, *args, func=None, **kwargs):
        super().__init__(*args, data_type='func', **kwargs)
        self.func = func

class varRef(varRegister):
    def __init__(self, *args, target=None, data_type='ref', **kwargs):
        super().__init__(*args, data_type=data_type, **kwargs)

        self.target = target

    @property
    def scalar_type(self):
        return self.target.scalar_type

    @property
    def length(self):
        return 1

class varObjectRef(varRef):
    def __init__(self, *args, lookups=None, attr=None, **kwargs):
        super().__init__(*args, data_type=None, **kwargs)

        if lookups is None:
            lookups = []

        self.lookups = lookups
        self.attr = attr

    @property
    def scalar_type(self):
        return self.data_type

    def lookup(self, indexes=[], lineno=None):
        self.lookups.extend(indexes) # add remaining indexes, if any
        return self

    def __str__(self):
        lookups = ''
        for a in self.lookups:
            assert isinstance(a, int) or isinstance(a, VarContainer)
            lookups += f'[{a}]'
              
        if self.target is not None:
            base = f'{super().__str__()}->{self.target}{lookups}'

        else:
            base = f'{super().__str__()}{lookups}'

        if self.attr is not None:
            return f'{base}.{self.attr.name}'

        return base

class varFunctionRef(varRef):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='funcref', **kwargs)
        self.ret_type = 'var'
        self.params = None

    @property
    def scalar_type(self):
        return self.data_type

class varArray(varComposite):
    def __init__(self, *args, element=None, length=1, **kwargs):
        super().__init__(*args, data_type=f'{element.data_type}[{length}]', **kwargs)
        self.length = length
        self.element = element

        self.init_val = element.init_val

    @property
    def is_global(self):
        return self._is_global

    @is_global.setter
    def is_global(self, value):
        self._is_global = value

        self.element.is_global = value

    @property
    def scalar_type(self):
        if not isinstance(self.element, varArray):
            return self.element.data_type

        return self.element.scalar_type

    @property
    def size(self):
        return self.length * self.element.size

    @property
    def stride(self):
        return self.element.size

    @property
    def dimensions(self):
        return 1 + self.element.dimensions

    def get_counts(self):
        counts = [self.length]

        if isinstance(self.element, varArray):
            counts.extend(self.element.get_counts())

        return counts

    def get_strides(self):
        strides = [self.stride]

        if isinstance(self.element, varArray):
            strides.extend(self.element.get_strides())

        return strides

    def lookup(self, indexes=[], lineno=None):
        # verify lookups are resolvable:
        for a in indexes:
            if not isinstance(a, VarContainer):
                raise SyntaxError(f'Lookup for "{a.name}" is not resolvable on "{self}"', lineno=lineno)

        indexes = deepcopy(indexes)
        
        if len(indexes) > 0:
            indexes.pop(0)
            
            return self.element.lookup(indexes)

        return self

class varStruct(varComposite):
    def __init__(self, *args, fields={}, **kwargs):
        super().__init__(*args, **kwargs)
        self.fields = fields

        self.offsets = {}

        self._size = 0
        for field in list(self.fields.values()):
            self.offsets[field.name] = self._size

            self._size += field.size

    def __str__(self):
        return f'Struct({super().__str__()})'

    @property
    def size(self):
        return self._size

    def get_field_from_offset(self, offset): 
        for field_name, addr in list(self.offsets.items()):
            if addr.name == offset.name:
                return self.fields[field_name]

        assert False

    def lookup(self, indexes=[], lineno=None):
        indexes = deepcopy(indexes)

        if len(indexes) > 0:
            index = indexes.pop(0)

            try:
                return self.fields[index].lookup(indexes)

            except KeyError:
                # try looking up by offset
                for field_name, offset in list(self.offsets.items()):
                    if offset == index:
                        return self.fields[field_name].lookup(indexes)

                raise

        return 0, self

class varStringBuf(varComposite):
    def __init__(self, *args, strlen=1, data_type='strbuf', **kwargs):
        super().__init__(*args, data_type=data_type, **kwargs)

        self.strlen = strlen
        self._init_val = None

    @property
    def init_val(self):
        return self._init_val

    @init_val.setter
    def init_val(self, value):
        self._init_val = value

        if value is None:
            self.strlen = 0

        elif isinstance(value, varStringLiteral):
            self.strlen = value.strlen

        else:
            self.strlen = len(self._init_val)

    @property
    def length(self):
        return int(self.strlen / 4) + 1

    @property
    def size(self):
        return self.length

class varStringRef(varRef):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='strref', **kwargs)

        self.target = self

    @property
    def scalar_type(self):
        return 'strref'

class varStringLiteral(varStringBuf):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='strlit', **kwargs)    

    def assemble(self):
        # create a null terminated c string
        c_string = self.init_val.encode('utf-8') + bytes([0])

        # # add zero padding to strings to align on 32 bits
        padding_len = (4 - (len(c_string) % 4)) % 4

        c_string += bytes([0] * padding_len)

        # break the byte string into 32 bit words
        words = []

        while len(c_string) > 0:
            chunk = c_string[:4]
            c_string = c_string[4:]

            words.append(struct.pack('BBBB', *chunk))

        return words


_BASE_TYPES = {
    'var': varScalar(), 
    'offset': varOffset(),  
    'i32': varInt32(), 
    'Number': varInt32(), 
    'f16': varFixed16(),
    'gfx16': varGfx16(),
    'Fixed16': varFixed16(),
    'strlit': varStringLiteral(),
    'strbuf': varStringBuf(),
    'strref': varStringRef(),
    'obj': varObject(),
    'objref': varObjectRef(),
    'pixref': varObjectRef(),
    'func': varFunction(),
    'funcref': varFunctionRef(),
    'Function': varFunctionRef(),
    'ref': varRef(),
    'PixelArray': varPixelArray(),
}

class TypeManager(object):
    def __init__(self):
        self.types = {}
        for t, v in _BASE_TYPES.items():
            self.create_type(t, v)

    def __str__(self):
        s = 'Types:\n'

        for k, v in self.types.items():
            s += f'\t{k} -> {v}\n'

        return s

    def lookup(self, name):
        return self.types[name]

    def create_type(self, name, base):
        self.types[name] = deepcopy(base)

    def create_var_from_type(self, name, data_type, dimensions=[], keywords={}, **kwargs):
        if not keywords:
            keywords = {}

        # remember init val so we can restore after the deepcopy
        # this preserves references properly
        if 'init_val' in keywords:
            old_init_val = keywords['init_val']

        keywords = deepcopy(keywords)

        if 'init_val' not in keywords:
            keywords['init_val'] = None

        else:
            keywords['init_val'] = old_init_val

        assert data_type is not None

        var = self.types[data_type].build(name, **kwargs)
        var.init_val = keywords['init_val']
        del keywords['init_val']

        if 'strlen' in keywords:
            var.strlen = keywords['strlen']
            del keywords['strlen']

        var.keywords = keywords

        if var.data_type is None:
            var.data_type = data_type

        for dim in dimensions:
            var = varArray(name, element=var, length=dim, lineno=kwargs['lineno'])

        return VarContainer(var)


if __name__ == '__main__':
    m = TypeManager()

    # print(m)

    v = m.create_var_from_type('meow', 'Number', [4], lineno=-1)
    print(v)
    print(v.lookup([7])) # 0
    # print(v.lookup([0,0,1])) # 1
    # print(v.lookup([0,1,0])) # 2
    # print(v.lookup([1,0,0])) # 6
    # print(v.lookup([1])[1])

    # print(v.stuff)
    # print(v.var)

    # from copy import copy
    # v1 = copy(v)
    # print(v1)
    # print(v1.var)
    # print(v1.stuff)
    
    # print(copy(v).stuff)

    # a = varArray(element=varInt32(), length=4)
    # b = varArray('my_array', element=a, length=3)
    # print(a)
    # print(b)

    # s = varStruct(fields={'a': varInt32(), 'b': varArray(element=varInt32(), length=4)})

    # print(s)
    # print(s.lookup(['b']))

    # s = varString(string='meow')
    # print(s)
    # r = varRef('my_ref', target=s)
    # print(r)