
class SymbolTable(object):
    def __init__(self, parent=None):
        self.parent = parent
        self.children = []

        self.symbols = {}        
        
    def __str__(self):
        return f'{self.symbols.keys()}'
        # s = 'Symbols:\n'

        # for k, v in self.symbols:
        #     s += f'\t{k}: {v.data_type}\n'

        # return s

    def __contains__(self, name):
        return name in self.symbols

    def create_child_symbol_table(self):
        s = SymbolTable(parent=self)
        self.children.append(s)

        return s

    def lookup(self, name):
        if name in self.symbols:
            return self.symbols[name]

        if self.parent is not None:
            return self.parent.lookup(name)

        raise KeyError(name)

    @property
    def globals(self):
        if self.parent:
            g = self.parent.globals

        else:
            g = {}

        g.update({k: v for k, v in self.symbols.items() if v.is_global and not v.is_container})

        return g

    @property
    def loaded_globals(self):
        if self.parent:
            g = self.parent.globals

        else:
            g = {}

        g.update({k: v for k, v in self.symbols.items() if v.is_global and v.is_container})

        return g

    def add(self, var):
        if var.name in self.symbols:
            raise KeyError(f'Symbol {var.name} already in symbol table')

        self.symbols[var.name] = var




