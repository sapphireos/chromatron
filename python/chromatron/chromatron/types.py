


class Var(object):
    def __init__(self, name, data_type=None):
        self.basename = name
        self.type = data_type

class varScalar(Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class varInt32(varScalar):
    def __init__(self, name, **kwargs):
        super().__init__(name, data_type='i32', **kwargs)

class varFixed16(varScalar):
    def __init__(self, name, **kwargs):
        super().__init__(name, data_type='f16', **kwargs)


class varRef(Var):
    def __init__(self, name, ref, **kwargs):
        super().__init__(name, **kwargs)
        self.ref = ref

class varObject(Var):
    def __init__(self, name, **kwargs):
        super().__init__(name, data_type='obj', **kwargs)

class varArray(varObject):
    def __init__(self, name, dimension=[1], **kwargs):
        super().__init__(name, data_type='ary', **kwargs)
        self.dimensions = dimensions



