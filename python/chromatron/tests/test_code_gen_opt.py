
import pytest

from chromatron import code_gen
from chromatron.ir2 import OptPasses


def run_code(source, *args, opt_passes=OptPasses.SSA):
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

load_store_5 = """
a = Number()

def init():
    if 1:
        a = 2

"""

load_store_6 = """
a = Number()
i = Number()

def init():
    while i < 10:
        i += 1
        a += 1

"""

load_store_7 = """
i = Number()

def init():
    for x in 4:
        i += 1
"""

load_store_8 = """
i = Number()

def init():
    for x in 4:
        i = 0

        while i < 10:
            i += 1
"""


# @pytest.mark.skip
@pytest.mark.parametrize("opt_passes", [OptPasses.SSA, OptPasses.GVN, OptPasses.LOOP, OptPasses.LS_SCHED])
def test_load_store_scheduler(opt_passes):
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

    regs = run_code(load_store_5, opt_passes=opt_passes)
    assert regs['a'] == 2

    regs = run_code(load_store_6, opt_passes=opt_passes)
    assert regs['a'] == 10
    assert regs['i'] == 10

    regs = run_code(load_store_7, opt_passes=opt_passes)
    assert regs['i'] == 4

    regs = run_code(load_store_8, opt_passes=opt_passes)
    assert regs['i'] == 10


loop_invariant_code_motion_basic = """

def init():
    i = Number()
    i = 4

    a = Number()

    while i > 0:
        i -= 1

        a = 2 + 3

    assert a == 5

"""

loop_invariant_code_motion_ifelse = """

def init():
    i = Number()
    i = 4

    a = Number()

    while i > 0:
        i -= 1

        if i == 10:
            a = 2 + 3

    assert a == 0

    i = 11

    while i > 0:
        i -= 1

        if i == 10:
            a = 2 + 3


    assert a == 5

"""


loop_invariant_code_motion_ifbreak = """

def init():
    i = Number()
    i = 4

    a = Number()

    while i > 0:
        i -= 1

        if i == 10:
            break

        a = 2 + 3

    # a should be 5 here unless i inits to 11

    assert a == 5

    a = 0
    i = 11

    while i > 0:
        i -= 1

        if i == 10:
            break

        a = 2 + 3

    assert a == 0

"""

@pytest.mark.parametrize("opt_passes", [OptPasses.SSA, OptPasses.GVN, OptPasses.LOOP, OptPasses.LS_SCHED])
def test_loop_invariant_code_motion(opt_passes):
    run_code(loop_invariant_code_motion_basic, opt_passes=opt_passes)
    run_code(loop_invariant_code_motion_ifelse, opt_passes=opt_passes)
    run_code(loop_invariant_code_motion_ifbreak, opt_passes=opt_passes)
