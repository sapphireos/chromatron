
from chromatron import code_gen
from chromatron.ir2 import OptPasses


def run_code(source, *args, opt_passes=OptPasses.LS_SCHED):
    prog = code_gen.compile_text(source, opt_passes=opt_passes)
    func = prog.init_func
    func.run(*args)

    return func.program.dump_globals()


load_store_1 = """
a = Number()

def init(param: Number):
    if param:
        a += 1

    else:
        a += 2
"""

load_store_2 = """
a = Number()

def init(param: Number):
    a += 1

    if param:
        a += 1

    else:
        a += 2

    a += 1

"""


load_store_3 = """
a = Number()
b = Number()

def init(param: Number):
    if param:
        a += 3

    else:
        a += 2
        b += 3

        return

    a += 1

"""

load_store_4 = """
a = Number()

def init():
    a += 1
    fence()
    a += 1

"""


def test_load_store_scheduler(opt_passes=OptPasses.LS_SCHED):
    regs = run_code(load_store_1, 0, opt_passes=opt_passes)
    assert regs['a'] == 2

    regs = run_code(load_store_1, 1, opt_passes=opt_passes)
    assert regs['a'] == 1

    regs = run_code(load_store_2, 0, opt_passes=opt_passes)
    assert regs['a'] == 4

    regs = run_code(load_store_2, 1, opt_passes=opt_passes)
    assert regs['a'] == 3

    regs = run_code(load_store_3, 0, opt_passes=opt_passes)
    assert regs['a'] == 2
    assert regs['b'] == 3

    regs = run_code(load_store_3, 1, opt_passes=opt_passes)
    assert regs['a'] == 4
    assert regs['b'] == 0

    regs = run_code(load_store_4, opt_passes=opt_passes)
    assert regs['a'] == 2


