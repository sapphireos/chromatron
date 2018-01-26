# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>

import unittest
import code_gen

empty_program = """
def init():
    pass

def loop():
    pass
"""

basic_vars = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    pass

def loop():
    pass

"""

basic_assign = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 1
    b = 2
    c = 3

    d = Number(publish=True)
    d = 4

def loop():
    pass

"""

basic_math = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)
g = Number(publish=True)

def init():
    a = 1 + 2
    b = 3 + a
    c = b + a
    d = 3 - 2
    e = 2 * 5
    f = 6 / 3
    g = 7 % 3

def loop():
    pass

"""


constant_folding = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = 1 + 2 + 3 + 4
    b = 1 + 2 + a + 3 + 4

def loop():
    pass

"""

basic_compare_gt = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 > 1
    b = 1 > 1
    c = 2 > 1

def loop():
    pass

"""

basic_compare_gte = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 >= 1
    b = 1 >= 1
    c = 2 >= 1

def loop():
    pass

"""

basic_compare_lt = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 < 1
    b = 1 < 1
    c = 2 < 1

def loop():
    pass

"""

basic_compare_lte = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 <= 1
    b = 1 <= 1
    c = 2 <= 1

def loop():
    pass

"""


basic_compare_eq = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = 0 == 1
    b = 1 == 1

def loop():
    pass

"""

basic_compare_neq = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = 0 != 1
    b = 1 != 1

def loop():
    pass

"""

basic_if = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    if 0:
        a = 2
    else:
        a = 1

    if 1:
        b = 2
    else:
        b = 1

def loop():
    pass

"""

basic_for = """
a = Number(publish=True)

def init():
    for i in 10:
        a += 1

def loop():
    pass

"""

basic_while = """
a = Number(publish=True)

def init():
    i = Number(publish=True)

    while i < 10:
        a += 1
        i += 1

def loop():
    pass

"""


basic_call = """
a = Number(publish=True)

def test():
    a = 5

def init():
    test()

def loop():
    pass

"""


basic_return = """
a = Number(publish=True)

def test():
    return 8

def init():
    a = test()

def loop():
    pass

"""

basic_logic = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)

def init():
    a = 0 and 0
    b = 0 and 1
    c = 1 and 1
    d = 0 or 0
    e = 0 or 1
    f = 1 or 1

def loop():
    pass

"""

call_with_params = """

a = Number(publish=True)
b = Number(publish=True)

def test(_a, _b):
    a = _a
    b = _b

def init():
    test(1, 2)

def loop():
    pass

"""

if_expr = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    if 1 + 2 == 3:
        a = 1

    if 1 + 2 != 3:
        a = 2

    if test(1, 2) == 3:
        b = 1

    if test(1, 2) != 3:
        b = 2

    if test(1, 2) + 3 == 3:
        c = 1

    if test(1, 2) + 3 != 3:
        c = 2

def loop():
    pass

"""


call_expr = """
a = Number(publish=True)
b = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    t = Number(publish=True)
    t = 5

    a = test(1 + 2, t)
    b = test(a + 2, t + 3)

def loop():
    pass

"""

call_register_reuse = """
a = Number(publish=True)

def test(_a, _b):
    test2(0, 0)
    return _a + _b

def test2(_a, _b):
    _a = 3
    _b = 4

def init():
    a = test(1, 1)

def loop():
    pass

"""


for_expr = """
a = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    t = Number(publish=True)
    t = 3

    for i in test(1, t) + 5:
        a += 1

def loop():
    pass

"""

while_expr = """
a = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    t = Number(publish=True)
    t = 3

    i = Number(publish=True)

    while i < test(1, t):
        a += 1
        i += 1

def loop():
    pass

"""



aug_assign_test = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():
    a += 1
    b -=1

    c = 3
    c *= 3

    d = 16
    d /= 4

    e = 10
    e %= 4

def loop():
    pass

"""


aug_assign_expr_test = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    a += 1 + 1
    b -= a - 4

    c += test(1, 2)
    d += test(1, 2) + 5
    e += test(1, 2) + test(3, 4)

def loop():
    pass

"""

hue_array_1 = """

def init():
    pixels[1].hue = 1
    pixels[1][2].hue = 2
    pixels[1][2].hue += 1

def loop():
    pass

"""

hue_array_2 = """

def init():
    pixels.hue = 32768

def loop():
    pass

"""

hue_array_add = """

def init():
    pixels.hue += 3

def loop():
    pass

"""

hue_array_add_2 = """

def init():
    pixels.hue = 65535
    pixels.hue += 2

def loop():
    pass

"""


hue_array_sub = """

def init():
    pixels.hue -= 3

def loop():
    pass

"""

hue_array_mul = """

def init():
    pixels.hue = 1
    pixels.hue *= 5

def loop():
    pass

"""

hue_array_div = """

def init():
    pixels.hue = 6
    pixels.hue /= 3

def loop():
    pass

"""

hue_array_mod = """

def init():
    pixels.hue = 5
    pixels.hue %= 3

def loop():
    pass

"""



sat_array_1 = """

def init():
    pixels[1].sat = 1
    pixels[1][2].sat = 2

def loop():
    pass

"""

sat_array_2 = """

def init():
    pixels.sat = 32768

def loop():
    pass

"""

sat_array_add = """

def init():
    pixels.sat += 3

def loop():
    pass

"""

sat_array_sub = """

def init():
    pixels.sat -= 3

def loop():
    pass

"""

sat_array_mul = """

def init():
    pixels.sat = 1
    pixels.sat *= 5

def loop():
    pass

"""

sat_array_div = """

def init():
    pixels.sat = 6
    pixels.sat /= 3

def loop():
    pass

"""

sat_array_mod = """

def init():
    pixels.sat = 5
    pixels.sat %= 3

def loop():
    pass

"""

val_array_1 = """

def init():
    pixels[1].val = 1
    pixels[1][2].val = 2

def loop():
    pass

"""

val_array_2 = """

def init():
    pixels.val = 32768

def loop():
    pass

"""

val_array_add = """

def init():
    pixels.val += 3

def loop():
    pass

"""

val_array_sub = """

def init():
    pixels.val -= 3

def loop():
    pass

"""

val_array_mul = """

def init():
    pixels.val = 1
    pixels.val *= 5

def loop():
    pass

"""

val_array_div = """

def init():
    pixels.val = 6
    pixels.val /= 3

def loop():
    pass

"""

val_array_mod = """

def init():
    pixels.val = 5
    pixels.val %= 3

def loop():
    pass

"""


hs_fade_array_1 = """

def init():
    pixels[1].hs_fade = 1
    pixels[1][2].hs_fade = 2

def loop():
    pass

"""

hs_fade_array_2 = """

def init():
    pixels.hs_fade = 32768

def loop():
    pass

"""

hs_fade_array_add = """

def init():
    pixels.hs_fade += 3

def loop():
    pass

"""

hs_fade_array_sub = """

def init():
    pixels.hs_fade -= 3

def loop():
    pass

"""

hs_fade_array_mul = """

def init():
    pixels.hs_fade = 1
    pixels.hs_fade *= 5

def loop():
    pass

"""

hs_fade_array_div = """

def init():
    pixels.hs_fade = 6
    pixels.hs_fade /= 3

def loop():
    pass

"""

hs_fade_array_mod = """

def init():
    pixels.hs_fade = 5
    pixels.hs_fade %= 3

def loop():
    pass

"""


v_fade_array_1 = """

def init():
    pixels[1].v_fade = 1
    pixels[1][2].v_fade = 2

def loop():
    pass

"""

v_fade_array_2 = """

def init():
    pixels.v_fade = 32768

def loop():
    pass

"""

v_fade_array_add = """

def init():
    pixels.v_fade += 3

def loop():
    pass

"""

v_fade_array_sub = """

def init():
    pixels.v_fade -= 3

def loop():
    pass

"""

v_fade_array_mul = """

def init():
    pixels.v_fade = 1
    pixels.v_fade *= 5

def loop():
    pass

"""

v_fade_array_div = """

def init():
    pixels.v_fade = 6
    pixels.v_fade /= 3

def loop():
    pass

"""

v_fade_array_mod = """

def init():
    pixels.v_fade = 5
    pixels.v_fade %= 3

def loop():
    pass

"""


array_indexing = """

a = Number(publish=True)

def init():
    pixels[a].val = 1
    pixels[a + 1].val = 2

    a += 2
    pixels[a].val = 3
    pixels[3].val = 4


def loop():
    pass

"""

array_load = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
i = Number(publish=True)

def init():
    pixels[0].val = 1
    pixels[1].val = 2
    pixels[2].val = 3
    pixels[3].val = 4

    a = pixels[0].val
    b = pixels[1].val
    i = 2
    c = pixels[2].val
    d = pixels[i + 1].val

def loop():
    pass

"""


break_node_while = """

def init():
    i = Number(publish=True)

    while i < 10:
        if i > 5:
            break

        i += 1

def loop():
    pass

"""


continue_node_while = """

a = Number(publish=True)

def init():
    i = Number(publish=True)

    while i < 10:
        i += 1

        if i > 5:
            continue

        a += 1

def loop():
    pass

"""


break_node_for = """

global_i = Number(publish=True)

def init():
    for i in 10:
        if i > 5:
            break

    global_i = i

def loop():
    pass

"""


continue_node_for = """

a = Number(publish=True)
global_i = Number(publish=True)

def init():
    for i in 10:
        if i > 5:
            continue

        a += 1

    global_i = i

def loop():
    pass

"""


double_break_node_while = """

global_a = Number(publish=True)

def init():
    i = Number(publish=True)

    for a in 4:
        while i < 10:
            if i > 5:
                break

            i += 1

    global_a = a

def loop():
    pass

"""


double_continue_node_while = """

a = Number(publish=True)
global_x = Number(publish=True)

def init():
    i = Number(publish=True)

    for x in 4:
        i = 0

        while i < 10:
            i += 1

            if i > 5:
                continue

            a += 1
    
    global_x = x

def loop():
    pass

"""


double_break_node_for = """

global_a = Number(publish=True)
global_i = Number(publish=True)

def init():
    b = Number(publish=True)
    for a in 4:
        for i in 10:
            if i > 5:
                break

            b += 1

    global_i = i
    global_a = a

def loop():
    pass

"""


double_continue_node_for = """

a = Number(publish=True)
global_i = Number(publish=True)
global_x = Number(publish=True)

def init():
    for x in 4:
        for i in 10:
            if i > 5:
                continue

            a += 1
    
    global_i = i
    global_x = x

def loop():
    pass

"""

no_loop_function = """

def init():
    pass

"""


pixel_array = """

p1 = PixelArray(2, 12, size_x=3, size_y=4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    a = p1.index
    b = p1.count
    c = p1.size_x
    d = p1.size_y

"""

multiple_comparison = """
   
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)
g = Number(publish=True)
h = Number(publish=True)

def init():
    a = 1
    b = 2
    c = 3
    d = 4

    if a > 0 or b > 0 or c > 0 or d > 0:
        e = 1

    if a > 1 or b > 2 or c > 3 or d > 4:
        f = 1

    if a > 1 or b > 2 or c > 3 or d > 3:
        g = 1

    if a > 10 or b > 10 or c > 0:
        h = 1


"""

test_not = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    a = not 1
    b = not 0

    c = not 1 > 2

    d = not (a == 0 or not a > 2)

"""


test_db_access = """

a = Number(publish=True)
b = Number(publish=True)

def init():
    db.kv_test_key = 123
    a = 2
    a += db.kv_test_key + 1

    b = db.kv_test_key
    
    db.kv_test_key = a

"""



class CGTestsBase(unittest.TestCase):
    def run_test(self, program, expected={}):
        pass

    def test_db_access(self):
        self.run_test(test_db_access,
            expected={
                'a': 126,
                'b': 123,
                'kv_test_key': 126,
            })

    def test_empty(self):
        self.run_test(empty_program,
            expected={
            })

    def test_basic_vars(self):
        self.run_test(basic_vars,
            expected={
                'a': 0,
                'b': 0,
                'c': 0,
            })

    def test_basic_assign(self):
        self.run_test(basic_assign,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_basic_math(self):
        self.run_test(basic_math,
            expected={
                'a': 3,
                'b': 6,
                'c': 9,
                'd': 1,
                'e': 10,
                'f': 2,
                'g': 1,
            })

    def test_constant_folding(self):
        self.run_test(constant_folding,
            expected={
                'a': 10,
                'b': 20,
            })

    def test_compare_gt(self):
        self.run_test(basic_compare_gt,
            expected={
                'a': 0,
                'b': 0,
                'c': 1,
            })

    def test_compare_gte(self):
        self.run_test(basic_compare_gte,
            expected={
                'a': 0,
                'b': 1,
                'c': 1,
            })

    def test_compare_lt(self):
        self.run_test(basic_compare_lt,
            expected={
                'a': 1,
                'b': 0,
                'c': 0,
            })

    def test_compare_lte(self):
        self.run_test(basic_compare_lte,
            expected={
                'a': 1,
                'b': 1,
                'c': 0,
            })

    def test_compare_eq(self):
        self.run_test(basic_compare_eq,
            expected={
                'a': 0,
                'b': 1,
            })

    def test_compare_neq(self):
        self.run_test(basic_compare_neq,
            expected={
                'a': 1,
                'b': 0,
            })

    def test_basic_if(self):
        self.run_test(basic_if,
            expected={
                'a': 1,
                'b': 2,
            })

    def test_basic_for(self):
        self.run_test(basic_for,
            expected={
                'a': 10,
            })

    def test_basic_while(self):
        self.run_test(basic_while,
            expected={
                'a': 10,
            })

    def test_basic_call(self):
        self.run_test(basic_call,
            expected={
                'a': 5,
            })

    def test_basic_return(self):
        self.run_test(basic_return,
            expected={
                'a': 8,
            })

    def test_basic_logic(self):
        self.run_test(basic_logic,
            expected={
                'a': 0,
                'b': 0,
                'c': 1,
                'd': 0,
                'e': 1,
                'f': 1,
            })

    def test_call_with_params(self):
        self.run_test(call_with_params,
            expected={
                'a': 1,
                'b': 2,
            })

    def test_if_expr(self):
        self.run_test(if_expr,
            expected={
                'a': 1,
                'b': 1,
                'c': 2,
            })

    def test_call_expr(self):
        self.run_test(call_expr,
            expected={
                'a': 8,
                'b': 18,
            })

    def test_call_register_reuse(self):
        self.run_test(call_register_reuse,
            expected={
                'a': 2,
            })

    def test_for_expr(self):
        self.run_test(for_expr,
            expected={
                'a': 9,
            })

    def test_while_expr(self):
        self.run_test(while_expr,
            expected={
                'a': 4,
            })

    def test_aug_assign_test(self):
        self.run_test(aug_assign_test,
            expected={
                'a': 1,
                'b': -1,
                'c': 9,
                'd': 4,
                'e': 2,
            })

    def test_aug_assign_expr_test(self):
        self.run_test(aug_assign_expr_test,
            expected={
                'a': 2,
                'b': 2,
                'c': 3,
                'd': 8,
                'e': 10,
            })

    def test_break_node_while(self):
        self.run_test(break_node_while,
            expected={
                'i': 6,
            })

    def test_continue_node_while(self):
        self.run_test(continue_node_while,
            expected={
                'i': 10,
                'a': 5,
            })

    def test_break_node_for(self):
        self.run_test(break_node_for,
            expected={
                'global_i': 6,
            })

    def test_continue_node_for(self):
        self.run_test(continue_node_for,
            expected={
                'global_i': 10,
                'a': 6
            })

    def test_double_break_node_while(self):
        self.run_test(double_break_node_while,
            expected={
                'i': 6,
                'global_a': 4,
            })

    def test_double_continue_node_while(self):
        self.run_test(double_continue_node_while,
            expected={
                'i': 10,
                'a': 20,
                'global_x': 4,
            })

    def test_double_break_node_for(self):
        self.run_test(double_break_node_for,
            expected={
                'global_i': 6,
                'global_a': 4,
                'b': 24,
            })

    def test_double_continue_node_for(self):
        self.run_test(double_continue_node_for,
            expected={
                'global_i': 10,
                'a': 24,
                'global_x': 4,
            })

    def test_no_loop_function(self):
        # we don't check anything, we just make sure
        # we can compile without a loop function.
        self.run_test(no_loop_function,
            expected={
            })

    def test_pixel_array(self):
        self.run_test(pixel_array,
            expected={
                'a': 2,
                'b': 12,
                'c': 3,
                'd': 4,
            })

    def test_multiple_comparison(self):
        self.run_test(multiple_comparison,
            expected={
                'e': 1,
                'f': 0,
                'g': 1,
            })

    def test_not(self):
        self.run_test(test_not,
            expected={
                'a': 0,
                'b': 1,
                'c': 1,
                'd': 0,
            })


class CGHSVArrayTests(unittest.TestCase):
    def test_hue_array_1(self):
        code = code_gen.compile_text(hue_array_1, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hue'][1], 1)
        self.assertEqual(hsv['hue'][9], 3)

    def test_hue_array_2(self):
        code = code_gen.compile_text(hue_array_2, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 32768)

    def test_hue_array_add(self):
        code = code_gen.compile_text(hue_array_add, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 3)

    def test_hue_array_add_2(self):
        code = code_gen.compile_text(hue_array_add_2, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 1)

    def test_hue_array_sub(self):
        code = code_gen.compile_text(hue_array_sub, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 65533)

    def test_hue_array_mul(self):
        code = code_gen.compile_text(hue_array_mul, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 5)

    def test_hue_array_div(self):
        code = code_gen.compile_text(hue_array_div, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 2)

    def test_hue_array_mod(self):
        code = code_gen.compile_text(hue_array_mod, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 2)

    def test_sat_array_1(self):
        code = code_gen.compile_text(sat_array_1, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['sat'][1], 1)
        self.assertEqual(hsv['sat'][9], 2)

    def test_sat_array_2(self):
        code = code_gen.compile_text(sat_array_2, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 32768)

    def test_sat_array_add(self):
        code = code_gen.compile_text(sat_array_add, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 3)

    def test_sat_array_sub(self):
        code = code_gen.compile_text(sat_array_sub, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 0)

    def test_sat_array_mul(self):
        code = code_gen.compile_text(sat_array_mul, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 5)

    def test_sat_array_div(self):
        code = code_gen.compile_text(sat_array_div, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 2)

    def test_sat_array_mod(self):
        code = code_gen.compile_text(sat_array_mod, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 2)

    def test_val_array_1(self):
        code = code_gen.compile_text(val_array_1, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['val'][1], 1)
        self.assertEqual(hsv['val'][9], 2)

    def test_val_array_2(self):
        code = code_gen.compile_text(val_array_2, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 32768)

    def test_val_array_add(self):
        code = code_gen.compile_text(val_array_add, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 3)

    def test_val_array_sub(self):
        code = code_gen.compile_text(val_array_sub, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 0)

    def test_val_array_mul(self):
        code = code_gen.compile_text(val_array_mul, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 5)

    def test_val_array_div(self):
        code = code_gen.compile_text(val_array_div, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 2)

    def test_val_array_mod(self):
        code = code_gen.compile_text(val_array_mod, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 2)


    def test_hs_fade_array_1(self):
        code = code_gen.compile_text(hs_fade_array_1, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hs_fade'][1], 1)
        self.assertEqual(hsv['hs_fade'][9], 2)

    def test_hs_fade_array_2(self):
        code = code_gen.compile_text(hs_fade_array_2, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 32768)

    def test_hs_fade_array_add(self):
        code = code_gen.compile_text(hs_fade_array_add, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 3)

    def test_hs_fade_array_sub(self):
        code = code_gen.compile_text(hs_fade_array_sub, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 0)

    def test_hs_fade_array_mul(self):
        code = code_gen.compile_text(hs_fade_array_mul, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 5)

    def test_hs_fade_array_div(self):
        code = code_gen.compile_text(hs_fade_array_div, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 2)

    def test_hs_fade_array_mod(self):
        code = code_gen.compile_text(hs_fade_array_mod, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 2)

    def test_v_fade_array_1(self):
        code = code_gen.compile_text(v_fade_array_1, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['v_fade'][1], 1)
        self.assertEqual(hsv['v_fade'][9], 2)

    def test_v_fade_array_2(self):
        code = code_gen.compile_text(v_fade_array_2, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 32768)

    def test_v_fade_array_add(self):
        code = code_gen.compile_text(v_fade_array_add, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 3)

    def test_v_fade_array_sub(self):
        code = code_gen.compile_text(v_fade_array_sub, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 0)

    def test_v_fade_array_mul(self):
        code = code_gen.compile_text(v_fade_array_mul, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 5)

    def test_v_fade_array_div(self):
        code = code_gen.compile_text(v_fade_array_div, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 2)

    def test_v_fade_array_mod(self):
        code = code_gen.compile_text(v_fade_array_mod, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 2)


    def test_array_indexing(self):
        code = code_gen.compile_text(array_indexing, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['val'][0], 1)
        self.assertEqual(hsv['val'][1], 2)
        self.assertEqual(hsv['val'][2], 3)
        self.assertEqual(hsv['val'][3], 4)

    def test_array_load(self):
        code = code_gen.compile_text(array_load, debug_print=False)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        regs = vm.dump_registers()

        self.assertEqual(regs['a'], 1)
        self.assertEqual(regs['b'], 2)
        self.assertEqual(regs['c'], 3)
        self.assertEqual(regs['d'], 4)


class CGTestsLocal(CGTestsBase):
    def run_test(self, program, expected={}):
        code = code_gen.compile_text(program)
        vm = code_gen.VM(code['vm_code'], code['vm_data'])

        vm.run_once()

        regs = vm.dump_registers()

        for reg, value in expected.iteritems():
            try:
                self.assertEqual(regs[reg], value)

            except AssertionError:
                print reg, regs[reg], value
                raise


import chromatron
import time

ct = chromatron.Chromatron(host='10.0.0.108')

class CGTestsOnDevice(CGTestsBase):
    def run_test(self, program, expected={}):
        global ct
        # ct = chromatron.Chromatron(host='usb', force_network=True)
        # ct = chromatron.Chromatron(host='10.0.0.108')

        ct.load_vm(bin_data=program)

        time.sleep(0.2)

        ct.init_scan()

        for reg, expected_value in expected.iteritems():
            if reg == 'kv_test_key':
                actual = ct.get_key(reg)

            else:
                actual = ct.get_vm_reg(str(reg))

            self.assertEqual(expected_value, actual)
            


