# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2019  Jeremy Billheimer
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
d = Number(publish=True)

def init():
    a = 1
    b = 2
    c = 3
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
i = Number(publish=True)

def init():
    
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
t = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    t = 5

    a = test(1 + 2, t)
    b = test(a + 2, t + 3)

def loop():
    pass

"""

call_register_reuse = """
a = Number(publish=True)

def test2(_a, _b):
    _a = 3
    _b = 4

def test(_a, _b):
    test2(0, 0)
    return _a + _b

def init():
    a = test(1, 1)

def loop():
    pass

"""


for_expr = """
a = Number(publish=True)
t = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    t = 3

    for i in test(1, t) + 5:
        a += 1

def loop():
    pass

"""

while_expr = """
a = Number(publish=True)
t = Number(publish=True)
i = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    t = 3

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



break_node_while = """

i = Number(publish=True)

def init():

    while i < 10:
        if i > 5:
            break

        i += 1

def loop():
    pass

"""


continue_node_while = """

a = Number(publish=True)
i = Number(publish=True)

def init():
    
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
i = Number(publish=True)

def init():

    for a in 4:
        while i < 10:
            if i > 5:
                break

            i += 1

    global_a = a

def loop():
    pass

"""


double_break_node_while2 = """

global_a = Number(publish=True)
i = Number(publish=True)

def init():
    for a in 4:
        while i < 10:
            if i > 5:
                break

            i += 1
        
        if a > 1:
            break

    global_a = a

def loop():
    pass

"""


double_continue_node_while = """

a = Number(publish=True)
global_x = Number(publish=True)
i = Number(publish=True)

def init():

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
b = Number(publish=True)

def init():
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


test_db_array_access = """

db_len = Number(publish=True)
db_len2 = Number(publish=True)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)


def init():
    db_len = len(db.kv_test_array)
    db_len2 = len(db.kv_test_key)

    db.kv_test_array[0] = 1
    db.kv_test_array[8] = 2
    db.kv_test_array[5] = 3
    db.kv_test_array[2] = 4
    
    a = db.kv_test_array[0]
    b = db.kv_test_array[8]
    c = db.kv_test_array[5]
    d = db.kv_test_array[2]

"""

test_array_index = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary[0] = 1
    ary[1] = 2
    ary[2] = 3
    ary[3] = 4
    ary[4] = 5

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""

test_array_assign = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary = 123

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""


test_array_add = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary[0] = 1
    ary[1] = 2
    ary[2] = 3
    ary[3] = 4
    ary[4] = 5

    ary += 123

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""



test_array_sub = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary[0] = 1
    ary[1] = 2
    ary[2] = 3
    ary[3] = 4
    ary[4] = 5

    ary -= 123

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""

test_array_mul = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary[0] = 1
    ary[1] = 2
    ary[2] = 3
    ary[3] = 4
    ary[4] = 5

    ary *= 123

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""

test_array_div = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary[0] = 100
    ary[1] = 200
    ary[2] = 300
    ary[3] = 400
    ary[4] = 500

    ary /= 100

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""


test_array_mod = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)

def init():

    ary[0] = 101
    ary[1] = 102
    ary[2] = 103
    ary[3] = 104
    ary[4] = 105

    ary %= 100

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""


test_array_aug_assign = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)

def init():

    ary[0] = 456

    ary[0] += 123

    a = ary[0]
    b = ary[1]

"""

test_array_iteration = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i + 1

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]

"""


test_array_len = """

ary = Array(4)

a = Number(publish=True)

def init():

    a = len(ary)

"""


test_array_max = """

ary = Array(4)

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i + 1

    a = max(ary)

"""


test_array_min = """

ary = Array(4)

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[len(ary) - 1 - i] = i + 1

    a = min(ary)

"""

test_array_sum = """

ary = Array(4)

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i * 4

    a = sum(ary)

"""



test_array_avg = """

ary = Array(4)

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i * 4

    a = avg(ary)

"""


test_array_index_expr = """

ary = Array(4)

a = Number(publish=True)
b = Number()

def init():
    for i in len(ary):
        ary[i] = i + 1

    b = 1
    a = ary[b + 1]

"""

test_base_record_assign = """

rec = Record(a=Number(), b=Number(), c=Number())

r = rec()

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    r['a'] = 1
    r['b'] = 2
    r['c'] = 3

    a = r['a']
    b = r['b']
    c = r['c']


"""


test_array_index_3d = """

ary = Array(2, 2, 2)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)
g = Number(publish=True)
h = Number(publish=True)
i = Number(publish=True)


def init():

    ary[0][0][0] = 1
    ary[0][0][1] = 2
    ary[0][1][0] = 3
    ary[0][1][1] = 4
    ary[1][0][0] = 5
    ary[1][0][1] = 6
    ary[1][1][0] = 7
    ary[1][1][1] = 8

    a = ary[0][0][0]
    b = ary[0][0][1]
    c = ary[0][1][0]
    d = ary[0][1][1]
    e = ary[1][0][0]
    f = ary[1][0][1]
    g = ary[1][1][0]
    h = ary[1][1][1]

    i = ary[3][0][1]
    
"""



test_array_index_3d_aug = """

ary = Array(2, 2, 2)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)
g = Number(publish=True)
h = Number(publish=True)
i = Number(publish=True)


def init():

    ary[0][0][0] += 1
    ary[0][0][1] += 2
    ary[0][1][0] += 3
    ary[0][1][1] += 4
    ary[1][0][0] += 5
    ary[1][0][1] += 6
    ary[1][1][0] += 7
    ary[1][1][1] += 8

    a = ary[0][0][0]
    b = ary[0][0][1]
    c = ary[0][1][0]
    d = ary[0][1][1]
    e = ary[1][0][0]
    f = ary[1][0][1]
    g = ary[1][1][0]
    h = ary[1][1][1]

    i = ary[3][0][1]
    
"""

test_complex_record_assign = """

rec = Record(a=Number(), b=Number(), c=Array(4))
r = rec()

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)

def init():
    r['a'] = 1
    r['b'] = 2
    r['c'][2] = 3

    a = r['a']
    b = r['b']
    c = r['c'][0]
    d = r['c'][1]
    e = r['c'][2]
    f = r['c'][3]

"""


test_complex_record_assign2 = """

rec = Record(a=Number(), b=Number(), c=Array(4))
r = rec()

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)

def init():
    r['a'] = 1
    r['b'] = 2
    r['c'] = 3

    a = r['a']
    b = r['b']
    c = r['c'][0]
    d = r['c'][1]
    e = r['c'][2]
    f = r['c'][3]

"""



test_complex_record_assign3 = """

rec = Record(a=Number(), b=Number(), c=Array(4))
r = Array(2, 3, type=rec())

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    
    r[0][1]['a'] = 1
    r[1][3]['a'] = 2
    r[2][6]['b'] = 3
    r[2][1]['c'][4] = 4

    a = r[0][1]['a']
    b = r[1][3]['a']
    c = r[2][6]['b']
    d = r[2][1]['c'][4]

"""



test_fix16 = """

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
n = Number(publish=True)

def init():
    a = 123.456
    n = a

    b = a + 32

    a -= 0.1

    c = 0.5 * 0.5

    d = 0.5 / 0.5

"""

test_type_conversions = """

a = Number(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Number(publish=True)

def init():
    a = 123.456
    b = 32

    c = 123
    c += 123

    d = 123.123
    d += 123.123

"""


test_type_conversions2 = """

a = Number(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Number(publish=True)

ary = Array(4, type=Fixed16)
ary2 = Array(4)

def init():
    ary = 3.123

    a = ary[1]
    b = ary[1]

    ary2 = 3.123    
    c = ary2[1]

    ary2 += 3.123
    d = ary2[1]

"""


test_type_conversions3 = """

a = Number(publish=True)
b = Fixed16(publish=True)

def init():
    a = 123
    b = a * 0.333

"""


test_array_assign_fixed16 = """

ary = Array(4, type=Fixed16)

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)

def init():

    ary = 123.123

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""


test_array_add_fixed16 = """

ary = Array(4, type=Fixed16)

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)

def init():

    ary[0] = 1.1
    ary[1] = 2.1
    ary[2] = 3.1
    ary[3] = 4.1
    ary[4] = 5.1

    ary += 123.1

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""



test_array_sub_fixed16 = """

ary = Array(4, type=Fixed16)

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)

def init():

    ary[0] = 1.1
    ary[1] = 2.1
    ary[2] = 3.1
    ary[3] = 4.1
    ary[4] = 5.1

    ary -= 123.1

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""

test_array_mul_fixed16 = """

ary = Array(4, type=Fixed16)

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)

def init():

    ary[0] = 1.1
    ary[1] = 2.1
    ary[2] = 3.1
    ary[3] = 4.1
    ary[4] = 5.1

    ary *= 123.1

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""

test_array_div_fixed16 = """

ary = Array(4, type=Fixed16)

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)

def init():

    ary[0] = 100.1
    ary[1] = 200.1
    ary[2] = 300.1
    ary[3] = 400.1
    ary[4] = 500.1

    ary /= 100.1

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""


test_array_mod_fixed16 = """

ary = Array(4, type=Fixed16)

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)

def init():

    ary[0] = 101.1
    ary[1] = 102.1
    ary[2] = 103.1
    ary[3] = 104.1
    ary[4] = 105.1

    ary %= 100.2

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    e = ary[4]

"""


test_array_expr = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)

def init():
    ary[0] = 0
    ary[1] = 1
    ary[2] = 2
    ary[3] = 3

    a = ary[2] + 1
    b = ary[3] + ary[1]

"""


test_expr_db = """

a = Number(publish=True)
b = Number(publish=True)

def init():
    db.kv_test_key = 123
    
    a = db.kv_test_key + 1
    b = db.kv_test_key + db.kv_test_key

"""


test_array_expr_db = """

a = Number(publish=True)
b = Number(publish=True)

def init():
    db.kv_test_array[0] = 1
    db.kv_test_array[1] = 2
    db.kv_test_array[2] = 3

    a = db.kv_test_array[0] + 1
    b = db.kv_test_array[1] + db.kv_test_array[2]

"""


test_local_declare = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)


def test():
    n = Number()
    a = n

    n += 1

def test2():
    ary = Array(3)
    ary = 0

    b = ary[0]
    c = ary[1]
    d = ary[2]

    ary += 1

def init():

    test()
    test()

    test2()
    test2()

"""


test_lib_call = """

a = Number(publish=True)

def init():
    a = test_lib_call(1, 2)

"""


test_array_index_call = """

a = Number(publish=True)
ary = Array(4)

def add(_a, _b):
    return _a + _b

def init():
    ary[add(1,2)] = 4

    a = 4

"""

test_loop_var_redeclare = """


def init():
    for i in 4:
        pass
    
    for i in 4:
        pass
"""


test_var_init = """

a = Number(5, publish=True)
b = Fixed16(1.23, publish=True)

ary = Array(2, type=Number, init_val=[1, 2])
ary2 = Array(2, type=Fixed16, init_val=[1.1, 2.1])

ary1_0 = Number(publish=True)
ary1_1 = Number(publish=True)
ary2_0 = Fixed16(publish=True)
ary2_1 = Fixed16(publish=True)

def init():
    ary1_0 = ary[0]
    ary1_1 = ary[1]
    ary2_0 = ary2[0]
    ary2_1 = ary2[1]

"""

test_var_overwrite = """

a = Number(publish=True)

def init():
    temp = Number()    
    temp = 90

    if temp < 10:
        a = 0
        
    elif temp >= 95:
        a = 1
        
    else:
        a = temp + 1

"""


test_improper_const_data_type_reuse = """

# note:
# this test checks for improper data type conversions when consts
# are incorrectly reused across data types.
# the constants used are specifically chosen.
# Fixed16 0.0001 maps to integer 6.
# if the bug this test covers occurs, the statement "a = 6"
# will take the Const(0.0001), which is a Fixed16, which will
# trigger an f16 to i32 conversion. In this case, that would
# set a to 0.

a = Number(publish=True)

def init():
    pixels.hue = 0.0001

    a = 6
    
"""



test_array_assign_direct = """

ary = Array(4, 4)
ary2 = Array(4, 4)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    ary[0][1] = 1
    ary[1][1] = 2
    ary[2][3] = 3
    ary[5][3] = 4

    ary2[2][5] = ary[0][1]
    ary2[0][2] = ary[1][1]
    ary2[5][3] = ary[2][3]
    ary2[1][1] = ary[5][3]

    a = ary2[2][5]
    b = ary2[0][2]
    c = ary2[5][3]
    d = ary2[1][1]

"""


test_array_assign_direct_mixed_types = """

ary = Array(4, 4)
ary2 = Array(4, 4, type=Fixed16)

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    ary[0][1] = 1
    ary[1][1] = 2
    ary[2][3] = 3
    ary[5][3] = 4

    ary2[2][5] = ary[0][1]
    ary2[0][2] = ary[1][1]
    ary2[5][3] = ary[2][3]
    ary2[1][1] = ary[5][3]

    a = ary2[2][5]
    b = ary2[0][2]
    c = ary2[5][3]
    d = ary2[1][1]

"""

test_temp_variable_redeclare_outside_scope = """

# no numeric checks, this test passes if it compiles without error
def init():
    if 1 == 2:
        temp = Number()

    else:
        temp = Number()
    
"""


test_pixel_mirror_compile = """

# no numeric checks, this test passes if it compiles without error

meow1 = PixelArray(0, 1, mirror='pixels')
meow2 = PixelArray(1, 2, mirror='meow1')
meow3 = PixelArray(0, 1, mirror=pixels)
meow4 = PixelArray(1, 2, mirror=meow1)
    
"""

test_global_avoids_optimize_assign_targets = """

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)

def init():
    a += 0.1

    b = a

    c = b
    
"""


test_indirect_load_func_arg = """

ary = Array(4)

a = Number(publish=True)
b = Number(publish=True)

def test(_a):
    return _a + 1

def init():
    ary[2] = 123
    a = test(ary[2])
    b = ary[2]
    
"""


# this test just needs to compile
test_bad_data_count = """

i = Number()

def init():
    while i < 0:
        for x in pixels.size_x:
                pass

    for x in pixels.size_x:
        pass
    
    
"""


class CGTestsBase(unittest.TestCase):
    def run_test(self, program, expected={}):
        pass

    def test_bad_data_count(self):
        self.run_test(test_bad_data_count,
            expected={
            })

    def test_indirect_load_func_arg(self):
        self.run_test(test_indirect_load_func_arg,
            expected={
                'a': 124,
                'b': 123,
            })

    def test_global_avoids_optimize_assign_targets(self):
        self.run_test(test_global_avoids_optimize_assign_targets,
            expected={
                'a': 0.0999908447265625,
                'b': 0.0999908447265625,
                'c': 0.0999908447265625,
            })

    def test_pixel_mirror_compile(self):
        self.run_test(test_pixel_mirror_compile,
            expected={
            })

    def test_temp_variable_redeclare_outside_scope(self):
        self.run_test(test_temp_variable_redeclare_outside_scope,
            expected={
            })

    def test_array_assign_direct_mixed_types(self):
        self.run_test(test_array_assign_direct_mixed_types,
            expected={
                'a': 1.0,
                'b': 2.0,
                'c': 3.0,
                'd': 4.0,
            })

    def test_array_assign_direct(self):
        self.run_test(test_array_assign_direct,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_improper_const_data_type_reuse(self):
        self.run_test(test_improper_const_data_type_reuse,
            expected={
                'a': 6,
            })

    def test_var_overwrite(self):
        self.run_test(test_var_overwrite,
            expected={
                'a': 91,
            })

    def test_var_init(self):
        self.run_test(test_var_init,
            expected={
                'a': 5,
                'b': 1.2299957275390625,
                'ary1_0': 1,
                'ary1_1': 2,
                'ary2_0': 1.0999908447265625,
                'ary2_1': 2.0999908447265625,
            })

    def test_loop_var_redeclare(self):
        # no value check.
        # this test passes if the compilation
        # succeeds without error.
        self.run_test(test_loop_var_redeclare,
            expected={
            })

    def test_array_index_call(self):
        self.run_test(test_array_index_call,
            expected={
                'a': 4,
            })

    def test_lib_call(self):
        self.run_test(test_lib_call,
            expected={
                'a': 3,
            })

    def test_local_declare(self):
        self.run_test(test_local_declare,
            expected={
                'a': 0,
                'b': 0,
                'c': 0,
                'd': 0,
            })

    def test_expr_db(self):
        self.run_test(test_expr_db,
            expected={
                'a': 124,
                'b': 246,
            })

    def test_array_expr_db(self):
        self.run_test(test_array_expr_db,
            expected={
                'a': 2,
                'b': 5,
            })

    def test_array_expr(self):
        self.run_test(test_array_expr,
            expected={
                'a': 3,
                'b': 4,
            })


    def test_array_mod_fixed16(self):
        self.run_test(test_array_mod_fixed16,
            expected={
                'a': 4.899993896484375,
                'b': 1.899993896484375,
                'c': 2.899993896484375,
                'd': 3.899993896484375,
                'e': 4.899993896484375,
            })


    def test_array_div_fixed16(self):
        self.run_test(test_array_div_fixed16,
            expected={
                'a': 4.996002197265625,
                'b': 1.998992919921875,
                'c': 2.9980010986328125,
                'd': 3.9969940185546875,
                'e': 4.996002197265625,
            })

    def test_array_mul_fixed16(self):
        self.run_test(test_array_mul_fixed16,
            expected={
                'a': 627.8088226318359,
                'b': 258.50885009765625,
                'c': 381.6088409423828,
                'd': 504.7088317871094,
                'e': 627.8088226318359,
            })

    def test_array_sub_fixed16(self):
        self.run_test(test_array_sub_fixed16,
            expected={
                'a': -118.0,
                'b': -121.0,
                'c': -120.0,
                'd': -119.0,
                'e': -118.0,
            })

    def test_array_add_fixed16(self):
        self.run_test(test_array_add_fixed16,
            expected={
                'a': 128.19998168945312,
                'b': 125.19998168945312,
                'c': 126.19998168945312,
                'd': 127.19998168945312,
                'e': 128.19998168945312,
            })

    def test_array_assign_fixed16(self):
        self.run_test(test_array_assign_fixed16,
            expected={
                'a': 123.12298583984375,
                'b': 123.12298583984375,
                'c': 123.12298583984375,
                'd': 123.12298583984375,
                'e': 123.12298583984375,
            })

    def test_type_conversions3(self):
        self.run_test(test_type_conversions3,
            expected={
                'a': 123,
                'b': 40.95808410644531,
            })

    def test_type_conversions2(self):
        self.run_test(test_type_conversions2,
            expected={
                'a': 3,
                'b': 3.12298583984375,
                'c': 3.0,
                'd': 6
            })

    def test_type_conversions(self):
        self.run_test(test_type_conversions,
            expected={
                'a': 123,
                'b': 32,
                'c': 246.0,
                'd': 246
            })

    def test_fix16(self):
        self.run_test(test_fix16,
            expected={
                'a': 123.35600280761719,
                'b': 155.45599365234375,
                'c': 0.25,
                'd': 1.0,
                'n': 123
            })

    def test_complex_record_assign3(self):
        self.run_test(test_complex_record_assign3,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_complex_record_assign2(self):
        self.run_test(test_complex_record_assign2,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 3,
                'e': 3,
                'f': 3,
            })

    def test_complex_record_assign(self):
        self.run_test(test_complex_record_assign,
            expected={
                'a': 1,
                'b': 2,
                'c': 0,
                'd': 0,
                'e': 3,
                'f': 0,
            })

    def test_array_index_3d_aug(self):
        self.run_test(test_array_index_3d_aug,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
                'f': 6,
                'g': 7,
                'h': 8,
                'i': 6,
            })

    def test_array_index_3d(self):
        self.run_test(test_array_index_3d,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
                'f': 6,
                'g': 7,
                'h': 8,
                'i': 6,
            })

    def test_base_record_assign(self):
        self.run_test(test_base_record_assign,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
            })

    def test_array_index_expr(self):
        self.run_test(test_array_index_expr,
            expected={
                'a': 3,
            })

    def test_array_avg(self):
        self.run_test(test_array_avg,
            expected={
                'a': 6,
            })

    def test_array_sum(self):
        self.run_test(test_array_sum,
            expected={
                'a': 24,
            })

    def test_array_min(self):
        self.run_test(test_array_min,
            expected={
                'a': 1,
            })

    def test_array_max(self):
        self.run_test(test_array_max,
            expected={
                'a': 4,
            })

    
    def test_array_aug_assign(self):
        self.run_test(test_array_aug_assign,
            expected={
                'a': 579,
                'b': 0,
            })

    def test_array_iteration(self):
        self.run_test(test_array_iteration,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_array_len(self):
        self.run_test(test_array_len,
            expected={
                'a': 4,
            })

    def test_array_mod(self):
        self.run_test(test_array_mod,
            expected={
                'a': 5,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
            })

    def test_array_div(self):
        self.run_test(test_array_div,
            expected={
                'a': 5,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
            })

    def test_array_mul(self):
        self.run_test(test_array_mul,
            expected={
                'a': 615,
                'b': 246,
                'c': 369,
                'd': 492,
                'e': 615,
            })

    def test_array_sub(self):
        self.run_test(test_array_sub,
            expected={
                'a': -118,
                'b': -121,
                'c': -120,
                'd': -119,
                'e': -118,
            })

    def test_array_add(self):
        self.run_test(test_array_add,
            expected={
                'a': 128,
                'b': 125,
                'c': 126,
                'd': 127,
                'e': 128,
            })

    def test_array_assign(self):
        self.run_test(test_array_assign,
            expected={
                'a': 123,
                'b': 123,
                'c': 123,
                'd': 123,
                'e': 123,
            })

    def test_array_index(self):
        self.run_test(test_array_index,
            expected={
                'a': 5,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
            })

    def test_db_access(self):
        self.run_test(test_db_access,
            expected={
                'a': 126,
                'b': 123,
                'kv_test_key': 126,
            })

    def test_db_array_access(self):
        self.run_test(test_db_array_access,
            expected={
                'db_len': 4,
                'db_len2': 1,
                'a': 2,
                'b': 2,
                'c': 3,
                'd': 4,
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

    def test_double_break_node_while2(self):
        self.run_test(double_break_node_while2,
            expected={
                'i': 6,
                'global_a': 2,
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


hue_array_1 = """

def init():
    pixels[1].hue = 1.0
    pixels[1][2].hue = 0.5
    pixels[1][2].hue += 0.1

def loop():
    pass

"""

hue_array_2 = """

def init():
    pixels.hue = 0.5

def loop():
    pass

"""

hue_array_add = """

def init():
    pixels.hue += 0.1

def loop():
    pass

"""

hue_array_add_2 = """

def init():
    pixels.hue = 1.0
    pixels.hue += 0.1

def loop():
    pass

"""


hue_array_sub = """

def init():
    pixels.hue -= 0.1

def loop():
    pass

"""

hue_array_mul = """

def init():
    pixels.hue = 0.5
    pixels.hue *= 0.5

def loop():
    pass

"""


hue_array_mul_f16 = """

def init():
    pixels.hue = 1.0
    pixels.hue *= 0.5

def loop():
    pass

"""

hue_array_div = """

def init():
    pixels.hue = 0.5
    pixels.hue /= 2.0

def loop():
    pass

"""

hue_array_div_f16 = """

def init():
    pixels.hue = 0.8
    pixels.hue /= 2.0

def loop():
    pass

"""

hue_array_mod = """

def init():
    pixels.hue = 0.5
    pixels.hue %= 0.3

def loop():
    pass

"""



sat_array_1 = """

def init():
    pixels[1].sat = 1.0
    pixels[1][2].sat = 0.5

def loop():
    pass

"""

sat_array_2 = """

def init():
    pixels.sat = 0.5

def loop():
    pass

"""

sat_array_add = """

def init():
    pixels.sat += 0.1

def loop():
    pass

"""

sat_array_sub = """

def init():
    pixels.sat -= 0.1

def loop():
    pass

"""

sat_array_mul = """

def init():
    pixels.sat = 0.5
    pixels.sat *= 0.25

def loop():
    pass

"""

sat_array_div = """

def init():
    pixels.sat = 0.5
    pixels.sat /= 2.0

def loop():
    pass

"""

sat_array_mod = """

def init():
    pixels.sat = 0.5
    pixels.sat %= 0.3

def loop():
    pass

"""

val_array_1 = """

def init():
    pixels[1].val = 1.0
    pixels[1][2].val = 0.5

def loop():
    pass

"""

val_array_2 = """

def init():
    pixels.val = 0.5

def loop():
    pass

"""

val_array_add = """

def init():
    pixels.val += 0.1

def loop():
    pass

"""

val_array_sub = """

def init():
    pixels.val -= 0.1

def loop():
    pass

"""

val_array_mul = """

def init():
    pixels.val = 0.5
    pixels.val *= 0.5

def loop():
    pass

"""

val_array_div = """

def init():
    pixels.val = 0.5
    pixels.val /= 2.0

def loop():
    pass

"""

val_array_mod = """

def init():
    pixels.val = 0.5
    pixels.val %= 0.3

def loop():
    pass

"""


hs_fade_array_1 = """

def init():
    pixels[1].hs_fade = 0.123
    pixels[1][2].hs_fade = 500

def loop():
    pass

"""

hs_fade_array_2 = """

def init():
    pixels.hs_fade = 200

def loop():
    pass

"""

hs_fade_array_add = """

def init():
    pixels.hs_fade += 100

def loop():
    pass

"""

hs_fade_array_sub = """

def init():
    pixels.hs_fade = 200
    pixels.hs_fade -= 100

def loop():
    pass

"""

hs_fade_array_mul = """

def init():
    pixels.hs_fade = 500
    pixels.hs_fade *= 2

def loop():
    pass

"""

hs_fade_array_div = """

def init():
    pixels.hs_fade = 100
    pixels.hs_fade /= 2

def loop():
    pass

"""

hs_fade_array_mod = """

def init():
    pixels.hs_fade = 500
    pixels.hs_fade %= 3

def loop():
    pass

"""


v_fade_array_1 = """

def init():
    pixels[1].v_fade = 19
    pixels[1][2].v_fade = 250

def loop():
    pass

"""

v_fade_array_2 = """

def init():
    pixels.v_fade = 500

def loop():
    pass

"""

v_fade_array_add = """

def init():
    pixels.v_fade += 100

def loop():
    pass

"""

v_fade_array_sub = """

def init():
    pixels.v_fade = 200
    pixels.v_fade -= 100

def loop():
    pass

"""

v_fade_array_mul = """

def init():
    pixels.v_fade = 500
    pixels.v_fade *= 2

def loop():
    pass

"""

v_fade_array_div = """

def init():
    pixels.v_fade = 500
    pixels.v_fade /= 2

def loop():
    pass

"""

v_fade_array_mod = """

def init():
    pixels.v_fade = 500
    pixels.v_fade %= 3

def loop():
    pass

"""


gfx_array_indexing = """

a = Number(publish=True)

def init():
    pixels[a].val = 0.1
    pixels[a + 1].val = 0.2

    a += 1
    pixels[a].val = 0.3
    pixels[3].val = 0.4


def loop():
    pass

"""

gfx_array_load = """

a = Fixed16(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
i = Number(publish=True)

def init():
    pixels[0].val = 0.1
    pixels[1].val = 0.2
    pixels[2].val = 0.3
    pixels[3].val = 0.4

    a = pixels[0].val
    b = pixels[1].val
    i = 2
    c = pixels[2].val
    d = pixels[i + 1].val

def loop():
    pass

"""


test_pix_mov_from_array = """

ary = Array(4)

def init():
    ary[2] = 1

    pixels.hue = ary[2]

"""


test_pix_load_from_array = """

ary = Array(4)

def init():
    ary[2] = 1

    pixels[1].hue = ary[2]

"""


test_pix_load_from_pix = """

def init():
    pixels[1].hue = 1
    pixels.hue = pixels[1].hue

"""


class CGHSVArrayTests(unittest.TestCase):
    def test_pix_load_from_pix(self):
        builder = code_gen.compile_text(test_pix_load_from_pix, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hue'][1], 1)
        
    def test_pix_load_from_array(self):
        builder = code_gen.compile_text(test_pix_load_from_array, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hue'][1], 1)

    def test_pix_mov_from_array(self):
        builder = code_gen.compile_text(test_pix_mov_from_array, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hue'][0], 1)

    def test_hue_array_1(self):
        builder = code_gen.compile_text(hue_array_1, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hue'][1], 65535)
        self.assertEqual(hsv['hue'][9], 39321)

    def test_hue_array_2(self):
        builder = code_gen.compile_text(hue_array_2, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 32768)

    def test_hue_array_add(self):
        builder = code_gen.compile_text(hue_array_add, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 6553)

    def test_hue_array_add_2(self):
        builder = code_gen.compile_text(hue_array_add_2, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 6552)

    def test_hue_array_sub(self):
        builder = code_gen.compile_text(hue_array_sub, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 58983)

    def test_hue_array_mul(self):
        builder = code_gen.compile_text(hue_array_mul, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 16384)

    def test_hue_array_mul_f16(self):
        builder = code_gen.compile_text(hue_array_mul_f16, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 32767)

    def test_hue_array_div(self):
        builder = code_gen.compile_text(hue_array_div, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 16384)

    def test_hue_array_div_f16(self):
        builder = code_gen.compile_text(hue_array_div_f16, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 26214)

    def test_hue_array_mod(self):
        builder = code_gen.compile_text(hue_array_mod, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hue']:
            self.assertEqual(a, 13108)

    def test_sat_array_1(self):
        builder = code_gen.compile_text(sat_array_1, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['sat'][1], 65535)
        self.assertEqual(hsv['sat'][9], 32768)

    def test_sat_array_2(self):
        builder = code_gen.compile_text(sat_array_2, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 32768)

    def test_sat_array_add(self):
        builder = code_gen.compile_text(sat_array_add, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 6553)

    def test_sat_array_sub(self):
        builder = code_gen.compile_text(sat_array_sub, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 0)

    def test_sat_array_mul(self):
        builder = code_gen.compile_text(sat_array_mul, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 8192)

    def test_sat_array_div(self):
        builder = code_gen.compile_text(sat_array_div, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 16384)

    def test_sat_array_mod(self):
        builder = code_gen.compile_text(sat_array_mod, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['sat']:
            self.assertEqual(a, 13108)

    def test_val_array_1(self):
        builder = code_gen.compile_text(val_array_1, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['val'][1], 65535)
        self.assertEqual(hsv['val'][9], 32768)

    def test_val_array_2(self):
        builder = code_gen.compile_text(val_array_2, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 32768)

    def test_val_array_add(self):
        builder = code_gen.compile_text(val_array_add, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 6553)

    def test_val_array_sub(self):
        builder = code_gen.compile_text(val_array_sub, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 0)

    def test_val_array_mul(self):
        builder = code_gen.compile_text(val_array_mul, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 16384)

    def test_val_array_div(self):
        builder = code_gen.compile_text(val_array_div, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 16384)

    def test_val_array_mod(self):
        builder = code_gen.compile_text(val_array_mod, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['val']:
            self.assertEqual(a, 13108)


    def test_hs_fade_array_1(self):
        builder = code_gen.compile_text(hs_fade_array_1, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['hs_fade'][1], 8060)
        self.assertEqual(hsv['hs_fade'][9], 500)

    def test_hs_fade_array_2(self):
        builder = code_gen.compile_text(hs_fade_array_2, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 200)

    def test_hs_fade_array_add(self):
        builder = code_gen.compile_text(hs_fade_array_add, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 100)

    def test_hs_fade_array_sub(self):
        builder = code_gen.compile_text(hs_fade_array_sub, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 100)

    def test_hs_fade_array_mul(self):
        builder = code_gen.compile_text(hs_fade_array_mul, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 1000)

    def test_hs_fade_array_div(self):
        builder = code_gen.compile_text(hs_fade_array_div, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 50)

    def test_hs_fade_array_mod(self):
        builder = code_gen.compile_text(hs_fade_array_mod, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['hs_fade']:
            self.assertEqual(a, 2)

    def test_v_fade_array_1(self):
        builder = code_gen.compile_text(v_fade_array_1, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['v_fade'][1], 19)
        self.assertEqual(hsv['v_fade'][9], 250)

    def test_v_fade_array_2(self):
        builder = code_gen.compile_text(v_fade_array_2, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 500)

    def test_v_fade_array_add(self):
        builder = code_gen.compile_text(v_fade_array_add, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 100)

    def test_v_fade_array_sub(self):
        builder = code_gen.compile_text(v_fade_array_sub, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 100)

    def test_v_fade_array_mul(self):
        builder = code_gen.compile_text(v_fade_array_mul, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 1000)

    def test_v_fade_array_div(self):
        builder = code_gen.compile_text(v_fade_array_div, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 250)

    def test_v_fade_array_mod(self):
        builder = code_gen.compile_text(v_fade_array_mod, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        for a in hsv['v_fade']:
            self.assertEqual(a, 2)


    def test_gfx_array_indexing(self):
        builder = code_gen.compile_text(gfx_array_indexing, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        hsv = vm.dump_hsv()

        self.assertEqual(hsv['val'][0], 6553)
        self.assertEqual(hsv['val'][1], 19660)
        self.assertEqual(hsv['val'][2], 0)
        self.assertEqual(hsv['val'][3], 26214)

    def test_gfx_array_load(self):
        builder = code_gen.compile_text(gfx_array_load, debug_print=False)
        vm = code_gen.VM(builder)

        vm.run_once()

        regs = vm.dump_registers()

        self.assertEqual(regs['a'], 0.0999908447265625)
        self.assertEqual(regs['b'], 0.1999969482421875)
        self.assertEqual(regs['c'], 0.29998779296875)
        self.assertEqual(regs['d'], 0.399993896484375)


class CGTestsLocal(CGTestsBase):
    def run_test(self, program, expected={}):
        
        builder = code_gen.compile_text(program)
        vm = code_gen.VM(builder)
        
        vm.run_once()
        regs = vm.dump_registers()

        for reg, value in expected.iteritems():
            try:
                try:
                    self.assertEqual(regs[reg], value)

                except KeyError:
                    # try database
                    self.assertEqual(vm.db[reg], value)

            except AssertionError:
                print '\n*******************************'
                print program
                print 'Var: %s Expected: %s Actual: %s' % (reg, value, regs[reg])
                print '-------------------------------\n'
                raise



# import chromatron
# from catbus import ProtocolErrorException
# import time

# ct = chromatron.Chromatron(host='10.0.0.125')

# class CGTestsOnDevice(CGTestsBase):
#     def run_test(self, program, expected={}):
#         global ct
#         # ct = chromatron.Chromatron(host='usb', force_network=True)
#         # ct = chromatron.Chromatron(host='10.0.0.108')

#         tries = 3

#         while tries > 0:
#             try:
#                 # ct.load_vm(bin_data=program)
#                 builder = code_gen.compile_text(program, debug_print=False)
#                 builder.assemble()
#                 data = builder.generate_binary('test.fxb')

#                 ct.stop_vm()
                
#                 # change vm program
#                 ct.set_key('vm_prog', 'test.fxb')
#                 ct.put_file('test.fxb', data)
#                 ct.start_vm()

#                 for i in xrange(100):
#                     time.sleep(0.1)

#                     vm_status = ct.get_key('vm_status')

#                     if vm_status != 4 and vm_status != -127:
#                         # vm reports READY (4), we need to wait until it changes 
#                         # (READY means it is waiting for the internal VM to start)
#                         # -127 means the VM is not initialized
#                         break

#                 self.assertEqual(0, vm_status)

#                 ct.init_scan()
                
#                 for reg, expected_value in expected.iteritems():
#                     tries = 5
#                     while tries > 0:
#                         tries -= 1

#                         if reg == 'kv_test_key':
#                             actual = ct.get_key(reg)

#                         else:
#                             actual = ct.get_vm_reg(str(reg))

#                         try:
#                             self.assertEqual(expected_value, actual)

#                         except AssertionError:
#                             # print "Expected: %s Actual: %s Reg: %s" % (expected_value, actual, reg)
#                             # print "Resetting VM..."
#                             if tries == 0:
#                                 raise

#                             ct.reset_vm()
#                             time.sleep(0.2)

#                 return

#             except ProtocolErrorException:
#                 print "Protocol error, trying again."
#                 tries -= 1




