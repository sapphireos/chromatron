
from chromatron import code_gen
from chromatron.ir2 import OptLevels


def run_code(source, param, opt_level=OptLevels.LS_SCHED):
    prog = code_gen.compile_text(source, opt_level=opt_level)
    func = prog.init_func
    func.run(param)

    return func.program.dump_globals()


load_store_1 = """
a = Number()

def init(param: Number):
    if param:
        a += 1

    else:
        a += 2

"""

def test_load_store_scheduler(opt_level=OptLevels.LS_SCHED):
    regs = run_code(load_store_1, 0, opt_level=opt_level)
    assert regs['a'] == 2

    regs = run_code(load_store_1, 1, opt_level=opt_level)
    assert regs['a'] == 1


