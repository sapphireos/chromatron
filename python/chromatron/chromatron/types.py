
from copy import deepcopy

from .exceptions import *
from .instructions2 import *

class VarContainer(object):
    def __init__(self, var):
        self.var = var 
        self.reg = None # what register is signed to this container 
        self.is_container = True
        
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
        self.is_global = False
        self.is_container = False

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
        if self.const:
            return f'${self._name}'

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

    def build(self, name, lineno=None, **kwargs):
        base = deepcopy(self)
        base.name = name
        base.lineno = lineno

        return base

    def __str__(self):
        if self.addr is None:
            return f'{self.ssa_name}:{self.data_type}'

        else:
            return f'{self.ssa_name}:{self.data_type}@0x{self.addr}'

    def lookup(self, index):
        return self

    def generate(self):
        if self.addr is None:
            raise CompilerFatal(f"{self} does not have an address. Line: {self.lineno}")

        return self.addr

class varRegister(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    @property
    def size(self):
        return 1 # register vars are always size 1

class varScalar(varRegister):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def lookup(self, indexes=[]): # lookup returns address offset and type (self, for scalars)
        return self

class varInt32(varScalar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='i32', **kwargs)

    def build(self, *args, **kwargs):
        return super().build(*args, **kwargs)

class varFixed16(varScalar):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='f16', **kwargs)

class varOffset(varRegister):
    def __init__(self, *args, offset=None, **kwargs):
        super().__init__(*args, data_type='offset', **kwargs)
        self.offset = offset
        self.ref = None

    # @property
    # def name(self):
    #     return f'&{self._name}'

    # @name.setter
    # def name(self, value):
    #     self._name = value

    def __str__(self):
        # return f'{super().__str__()} -> {self.offset}'
        return f'{super().__str__()}'

class varRef(varRegister):
    def __init__(self, *args, data_type='ref', **kwargs):
        super().__init__(*args, data_type=data_type, **kwargs)

    # def __str__(self):
    #     return f'{super().__str__()} -> {self.target}'

class varFunction(Var):
    def __init__(self, *args, func=None, **kwargs):
        super().__init__(*args, data_type='func', **kwargs)
        self.func = func

class varComposite(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varObject(varComposite):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varArray(varComposite):
    def __init__(self, *args, element=None, length=1, **kwargs):
        super().__init__(*args, data_type=f'{element.data_type}[{length}]', **kwargs)
        self.length = length
        self.element = element

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

    # def lookup(self, indexes=[]):
    #     indexes = deepcopy(indexes)
        
    #     if len(indexes) > 0:
    #         offset = indexes.pop(0) * self.stride
    #         offset %= self.length

    #         addr, datatype = self.element.lookup(indexes)

    #         addr += offset

    #         return varOffset(offset=addr), datatype

    #     return varOffset(offset=0), self

    def lookup(self, indexes=[]):
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

    def lookup(self, indexes=[]):
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

class varStringLiteral(varComposite):
    def __init__(self, *args, string=None, max_length=None, **kwargs):
        super().__init__(*args, data_type='strlit', **kwargs)

        if string is not None:
            self.value = string
            self.max_length = len(string)

        else:
            assert max_length is not None
            self.value = '\0' * max_length
            self.max_length = max_length

        self._size = int(((self.max_length - 1) / 4) + 2) # space for characters + 32 bit length

    def __str__(self):
        return f'String("{self.value}"[{self.max_length}])'

    @property
    def size(self):
        return self._size

class varString(varRef):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, data_type='str', **kwargs)


_BASE_TYPES = {
    'var': varScalar(), 
    'offset': varOffset(), 
    'Number': varInt32(), 
    'i32': varInt32(), 
    'Fixed16': varFixed16(),
    'f16': varFixed16(),
    'str': varString(),
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

    def create_type(self, name, base):
        self.types[name] = deepcopy(base)

    def create_var_from_type(self, name, data_type, dimensions=[], keywords={}, **kwargs):
        if not keywords:
            keywords = {'init_val': None}

        var = self.types[data_type].build(name, **kwargs)
        var.init_val = keywords['init_val']

        for dim in reversed(dimensions):
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