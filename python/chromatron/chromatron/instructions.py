

opcodes = {
    'MOV':                  0x01,
    'COMP_EQ':              0x02,
    'COMP_NEQ':             0x03,
    'COMP_GT':              0x04,
    'COMP_GTE':             0x05,
    'COMP_LT':              0x06,
    'COMP_LTE':             0x07,
    'AND':                  0x08,
    'OR':                   0x09,
    'ADD':                  0x0A,
    'SUB':                  0x0B,
    'MUL':                  0x0C,
    'DIV':                  0x0D,
    'MOD':                  0x0E,
    'JMP':                  0x0F,

    'JMP_IF_Z':             0x10,
    'JMP_IF_NOT_Z':         0x11,
    'JMP_IF_Z_DEC':         0x12,
    'JMP_IF_GTE':           0x13,
    'JMP_IF_LESS_PRE_INC':  0x14,

    'PRINT':                0x15,

    'RET':                  0x16,
    'CALL':                 0x17,

    'INDEX':                0x18,

    'LOAD_INDIRECT':        0x19,
    'STORE_INDIRECT':       0x1A,

    'RAND':                 0x1B,
    'ASSERT':               0x1C,
    'HALT':                 0x1D,
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


class insLabel(BaseInstruction):
    def __init__(self, name=None):
        self.name = name

    def __str__(self):
        return "Label(%s)" % (self.name)


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


class BaseJmp(BaseInstruction):
    mnemonic = 'JMP'

    def __init__(self, label):
        super(BaseJmp, self).__init__()

        self.label = label

    def __str__(self):
        return "%s -> %s" % (self.mnemonic, self.label)

    # def assemble(self):
        # return [self.opcode, ('label', self.label.name), 0]

class insJmp(BaseJmp):
    pass

class insJmpConditional(BaseJmp):
    def __init__(self, op1, label):
        super(insJmpConditional, self).__init__(label)

        self.op1 = op1

    def __str__(self):
        return "%s, %s -> %s" % (self.mnemonic, self.op1, self.label)

    # def assemble(self):
        # return [self.opcode, self.op1.addr, ('label', self.label.name), 0]


class insJmpIfZero(insJmpConditional):
    mnemonic = 'JMP_IF_Z'

class insJmpNotZero(insJmpConditional):
    mnemonic = 'JMP_IF_NOT_Z'

class insJmpIfZeroPostDec(insJmpConditional):
    mnemonic = 'JMP_IF_Z_DEC'

class insJmpIfGte(BaseJmp):
    mnemonic = 'JMP_IF_GTE'

    def __init__(self, op1, op2, label):
        super(insJmpIfGte, self).__init__(label)

        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%s, %s >= %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)

    # def assemble(self):
        # return [self.opcode, self.op1.addr, self.op2.addr, ('label', self.label.name), 0]


class insJmpIfLessThanPreInc(BaseJmp):
    mnemonic = 'JMP_IF_LESS_PRE_INC'

    def __init__(self, op1, op2, label):
        super(insJmpIfLessThanPreInc, self).__init__(label)

        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%s, ++%s < %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)

    # def assemble(self):
        # return [self.opcode, self.op1.addr, self.op2.addr, ('label', self.label.name), 0]

class insReturn(BaseInstruction):
    mnemonic = 'RET'

    def __init__(self, op1):
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    # def assemble(self):
        # return [self.opcode, self.op1.addr]

class insCall(BaseInstruction):
    mnemonic = 'CALL'

    def __init__(self, target):
        self.target = target

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.target)

    # def assemble(self):
        # return [self.opcode, ('addr', self.target), 0]

class insIndex(BaseInstruction):
    mnemonic = 'INDEX'

    def __init__(self, result, target, indexes):
        self.result = result
        self.target = target
        self.indexes = indexes

    def __str__(self):
        indexes = ''
        for index in self.indexes:
            indexes += '[%s]' % (index)
        return "%s %s <- %s %s" % (self.mnemonic, self.result, self.target, indexes)

class insIndirectLoad(BaseInstruction):
    mnemonic = 'LOAD_INDIRECT'

    def __init__(self, dest, addr):
        self.dest = dest
        self.addr = addr

    def __str__(self):
        return "%s %s <- *%s" % (self.mnemonic, self.dest, self.addr)

    # def assemble(self):
        # return [self.opcode, self.dest.addr, self.src.addr, self.index.addr]



class insIndirectStore(BaseInstruction):
    mnemonic = 'STORE_INDIRECT'

    def __init__(self, src, addr):
        self.src = src
        self.addr = addr

    def __str__(self):
        return "%s *%s <- %s" % (self.mnemonic, self.addr, self.src)

    # def assemble(self):
        # return [self.opcode, self.dest.addr, self.src.addr, self.index.addr]










class insRand(BaseInstruction):
    mnemonic = 'RAND'

    def __init__(self, dest, start=0, end=65535):
        self.dest = dest
        self.start = start
        self.end = end

    def __str__(self):
        return "%s %s <- rand(%s, %s)" % (self.mnemonic, self.dest, self.start, self.end)

    # def assemble(self):
        # return [self.opcode, self.dest.addr, self.start.addr, self.end.addr]

class insAssert(BaseInstruction):
    mnemonic = 'ASSERT'

    def __init__(self, op1):
        self.op1 = op1

    def __str__(self):
        return "%s %s == TRUE" % (self.mnemonic, self.op1)

    # def assemble(self):
        # return [self.opcode, self.op1.addr]

class insHalt(BaseInstruction):
    mnemonic = 'HALT'
    
    def __init__(self):
        pass

    def __str__(self):
        return "%s" % (self.mnemonic)

    # def assemble(self):
        # return [self.opcode]
