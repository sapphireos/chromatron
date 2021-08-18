
from copy import deepcopy

class VarContainer(object):
    def __init__(self, var):
        self.var = var 
        self.reg = None # what register is signed to this container 

    def __str__(self):
        if self.reg:
            return f'{self.var}@{self.reg}'

        else:
            return f'{self.var}'

    def __getattr__(self, name):
        return getattr(self.var, name)

    def __setattr__(self, name, value):
        if name in ['var', 'reg']:
            super().__setattr__(name, value)

        else:
            setattr(self.var, name, value)


class Var(object):
    def __init__(self, name, data_type=None):
        self.name = name
        self.type = data_type
        self.addr = None # assigned address of this variable

    @property
    def size(self): # size in machine words (32 bits)
        return None

    def build(self, name, **kwargs):
        base = deepcopy(self)
        base.name = name

        return base

    def __str__(self):
        return f'{self.name}:{self.type}'

class varRegister(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    @property
    def size(self):
        return 1 # register vars are always size 1

class varScalar(varRegister):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varInt32(varScalar):
    def __init__(self, name, **kwargs):
        super().__init__(name, data_type='i32', **kwargs)

    def build(self, name, **kwargs):
        return super().build(name, **kwargs)

class varFixed16(varScalar):
    def __init__(self, name, **kwargs):
        super().__init__(name, data_type='f16', **kwargs)

class varRef(varRegister):
    def __init__(self, name, target, **kwargs):
        super().__init__(name, **kwargs)
        self.target = target


class varComposite(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varObject(varComposite):
    def __init__(self, name, **kwargs):
        super().__init__(name, data_type='obj', **kwargs)

class varArray(varComposite):
    def __init__(self, name, length=1, **kwargs):
        super().__init__(name, data_type='ary', **kwargs)
        self.length = length

class varStruct(varComposite):
    def __init__(self, name, fields={}, **kwargs):
        super().__init__(name, data_type='struct', **kwargs)
        self.fields = fields

class varString(varComposite):
    def __init__(self, name, max_length=None, **kwargs):
        super().__init__(name, data_type='str', **kwargs)
        self.max_length = max_length

 

_BASE_TYPES = [varInt32('Number'), varFixed16('Fixed16')]

class TypeManager(object):
    def __init__(self):
        self.types = {}
        for t in _BASE_TYPES:
            self.create_type(t.name, t)

    def __str__(self):
        s = 'Types:\n'

        for k, v in self.types.items():
            s += f'\t{k} -> {v}\n'

        return s

    def create_type(self, name, base):
        self.types[name] = deepcopy(base)

    def create_var(self, name, data_type, **kwargs):
        return VarContainer(self.types[data_type].build(name, **kwargs))


if __name__ == '__main__':
    m = TypeManager()

    print(m)

    v = m.create_var('meow', 'Number')
    print(v)

