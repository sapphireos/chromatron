
class SymbolTable(object):
    def __init__(self, parent=None):
        self.parent = parent
        self.children = []

        self.symbols = {}        
        
    def __str__(self):
        s = 'Symbols:\n'

        for k, v in self.symbols:
            s += f'\t{k}: {v.data_type}\n'

        return s

    def __contains__(self, name):
        return name in self.symbols

    def create_child_symbol_table(self):
        s = SymbolTable(parent=self)
        self.children.append(s)

        return s

    def lookup(self, name):
        if name in self.symbols:
            return self.symbols[name].copy()

        if self.parent is not None:
            return self.parent.lookup(name)

        raise KeyError(name)

    def add(self, var):
        if var.name in self.symbols:
            raise KeyError(f'Symbol {var.name} already in symbol table')

        self.symbols[var.name] = var




