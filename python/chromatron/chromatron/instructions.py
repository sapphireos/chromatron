

opcodes = {
    'MOV':              0x01,
    'COMP_EQ':          0x02,
    'COMP_NEQ':         0x03,
    'COMP_GT':          0x04,
    'COMP_GTE':         0x05,
    'COMP_LT':          0x06,
    'COMP_LTE':         0x07,
    'AND':              0x08,
    'OR':               0x09,
    'ADD':              0x0A,
    'SUB':              0x0B,
    'MUL':              0x0C,
    'DIV':              0x0D,
    'MOD':              0x0E,
}


class BaseInstruction(object):
    mnemonic = 'NOP'
    opcode = None

    def __str__(self):
        return self.mnemonic

    def assemble(self):
        raise NotImplementedError(self.mnemonic)

    def len(self):
        return len(self.assemble())

    @property
    def opcode(self):
        global opcodes
        return opcodes[self.mnemonic]


# pseudo instruction - does not actually produce an opcode
class insAddr(BaseInstruction):
    def __init__(self, addr=None):
        self.addr = addr

    def __str__(self):
        return "Addr(%s)" % (self.addr)
    
    def assemble(self):
        return [self.addr]


class insMov(BaseInstruction):
    mnemonic = 'MOV'

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.src)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.dest.assemble())
        bc.extend(self.src.assemble())

        return bc


class insBinop(BaseInstruction):
    def __init__(self, result, op1, op2):
        super(insBinop, self).__init__()
        self.result = result
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%-16s %16s <- %16s %4s %16s" % (self.mnemonic, self.result, self.op1, self.symbol, self.op2)

    def assemble(self):
        bc = [self.opcode]
        bc.extend(self.result.assemble())
        bc.extend(self.op1.assemble())
        bc.extend(self.op2.assemble())

        return bc

class insCompareEq(insBinop):
    mnemonic = 'COMP_EQ'
    symbol = "=="

class insCompareNeq(insBinop):
    mnemonic = 'COMP_NEQ'
    symbol = "!="

class insCompareGt(insBinop):
    mnemonic = 'COMP_GT'
    symbol = ">"

class insCompareGtE(insBinop):
    mnemonic = 'COMP_GTE'
    symbol = ">="

class insCompareLt(insBinop):
    mnemonic = 'COMP_LT'
    symbol = "<"

class insCompareLtE(insBinop):
    mnemonic = 'COMP_LTE'
    symbol = "<="

class insAnd(insBinop):
    mnemonic = 'AND'
    symbol = "AND"

class insOr(insBinop):
    mnemonic = 'OR'
    symbol = "OR"

class insAdd(insBinop):
    mnemonic = 'ADD'
    symbol = "+"

class insSub(insBinop):
    mnemonic = 'SUB'
    symbol = "-"

class insMul(insBinop):
    mnemonic = 'MUL'
    symbol = "*"

class insDiv(insBinop):
    mnemonic = 'DIV'
    symbol = "/"

class insMod(insBinop):
    mnemonic = 'MOD'
    symbol = "%"
