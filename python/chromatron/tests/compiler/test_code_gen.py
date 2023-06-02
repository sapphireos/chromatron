# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
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

import pytest
import unittest
from chromatron import code_gen
from conftest import *

empty_program = """
def init():
    pass
"""

basic_vars = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
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

"""


constant_folding = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = 1 + 2 + 3 + 4
    b = 1 + 2 + a + 3 + 4

"""

basic_compare_gt = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 > 1
    b = 1 > 1
    c = 2 > 1

"""

basic_compare_gte = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 >= 1
    b = 1 >= 1
    c = 2 >= 1

"""

basic_compare_lt = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 < 1
    b = 1 < 1
    c = 2 < 1

"""

basic_compare_lte = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    a = 0 <= 1
    b = 1 <= 1
    c = 2 <= 1

"""


basic_compare_eq = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = 0 == 1
    b = 1 == 1

"""

basic_compare_neq = """
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = 0 != 1
    b = 1 != 1

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

"""

basic_for = """
a = Number(publish=True)

def init():
    for i in 10:
        a += 1

"""

basic_while = """
a = Number(publish=True)
i = Number(publish=True)

def init():
    
    while i < 10:
        a += 1
        i += 1

"""


basic_call = """
a = Number(publish=True)

def test():
    a = 5

def init():
    test()

"""


basic_return = """
a = Number(publish=True)

def test():
    return 8

def init():
    a = test()

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

"""

call_with_params = """

a = Number(publish=True)
b = Number(publish=True)

def test(_a, _b):
    a = _a
    b = _b

def init():
    test(1, 2)

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

"""



break_node_while = """

i = Number(publish=True)

def init():

    while i < 10:
        if i > 5:
            break

        i += 1

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

"""


break_node_for = """

global_i = Number(publish=True)

def init():
    for i in 10:
        if i > 5:
            break

    global_i = i

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
e = Number(publish=True)


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

    e = db.kv_test_array[5] + db.kv_test_array[2]

"""

test_array_index = """

ary = Number()[4]

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

ary = Number()[4]

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

ary = Number()[4]

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

ary = Number()[4]

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

ary = Number()[4]

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

ary = Number()[4]

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

ary = Number()[4]

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

ary = Number()[4]

a = Number(publish=True)
b = Number(publish=True)

def init():

    ary[0] = 456

    ary[0] += 123

    a = ary[0]
    b = ary[1]

"""

test_array_iteration = """

ary = Number()[4]

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

ary = Number()[4]

a = Number(publish=True)

def init():

    a = len(ary)

"""


test_array_max = """

ary = Number()[4]

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i + 1

    a = max(ary)

"""


test_array_min = """

ary = Number()[4]

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[len(ary) - 1 - i] = i + 1

    a = min(ary)

"""

test_array_sum = """

ary = Number()[4]

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i * 4

    a = sum(ary)

"""



test_array_avg = """

ary = Number()[4]

a = Number(publish=True)

def init():
    for i in len(ary):
        ary[i] = i * 4

    a = avg(ary)

"""


test_array_index_expr = """

ary = Number()[4]

a = Number(publish=True)
b = Number()

def init():
    for i in len(ary):
        ary[i] = i + 1

    b = 1
    a = ary[b + 1]

"""


test_array_index_expr2 = """

a = Number(publish=True)
ary = Number()[4]

def init():


    for x in 2:
        ary[x + 2] = 1
        ary[x + 2 + 1] = 2

    a = ary[0]

"""


test_array_index_expr3 = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
ary = Number()[4]

def init():
    for x in 2:
        for y in 2:
            ary[x + 2] = 1
            ary[x + 2] = 2

    a = ary[0]
    b = ary[1]
    c = ary[2]
    d = ary[3]
    
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

ary = Number()[2][2][2]

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

ary = Number()[2][2][2]

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

ary = Fixed16()[4]
ary2 = Number()[4]

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

ary = Fixed16()[4]

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

ary = Fixed16()[4]

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

ary = Fixed16()[4]

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

ary = Fixed16()[4]

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

ary = Fixed16()[4]

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

ary = Fixed16()[4]

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

ary = Number()[4]

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


test_compare_db = """

a = Number(publish=True)

def init():
    db.kv_test_key = 123
    
    if db.kv_test_key == 123:
        a = 1

    else:
        a = 2

"""

test_expr_db = """

a = Number(publish=True)
b = Number(publish=True)

def init():
    db.kv_test_key = 123
    
    a = db.kv_test_key + 1
    b = db.kv_test_key + db.kv_test_key

"""

test_expr_db_ret = """

a = Number(publish=True)

def get_db():
    return db.kv_test_key

def init():
    db.kv_test_key = 123

    a = get_db()
"""

test_expr_db_ret_binop = """

a = Number(publish=True)

def get_db():
    return db.kv_test_key + 1

def init():
    db.kv_test_key = 123

    a = get_db()
"""

test_expr_db_f16 = """

a = Fixed16(publish=True)
b = Fixed16(publish=True)

def init():
    db.kv_test_key = 123
    
    a = db.kv_test_key + 1.0 # note that since we don't really do DB type conversion, db.kv_test_key gets treated as an f16 here.  this is ok for now.
    b = db.kv_test_key + db.kv_test_key

"""

test_db_augassign = """

a = Number(publish=True)

def init():
    db.kv_test_key = 123
    db.kv_test_key += 1
    
    a = db.kv_test_key

"""

test_db_assign_binop = """

a = Number(publish=True)

def init():
    db.kv_test_key = 123
    db.kv_test_key = db.kv_test_key + 1
    
    a = db.kv_test_key

"""

test_db_augassign_indexed = """

a = Number(publish=True)

def init():
    db.kv_test_array[1] = 123
    db.kv_test_array[1] += 1
    
    a = db.kv_test_array[1]

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
    ary = Number()[3]
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
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)
e = Number(publish=True)
f = Number(publish=True)

def init():
    a = test_lib_call()
    b = test_lib_call(1)
    c = test_lib_call(1, 2)
    d = test_lib_call(1, 2, 3)
    e = test_lib_call(1, 2, 3, 4)
    f = test_gfx_lib_call()

"""


test_array_index_call = """

a = Number(publish=True)
ary = Number()[4]

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

#ary = Number(1, 2)[2]
#ary2 = Fixed16(1.1, 2.1)[2]

#ary1_0 = Number(publish=True)
#ary1_1 = Number(publish=True)
#ary2_0 = Fixed16(publish=True)
#ary2_1 = Fixed16(publish=True)

def init():
    pass
    #ary1_0 = ary[0]
    #ary1_1 = ary[1]
    #ary2_0 = ary2[0]
    #ary2_1 = ary2[1]

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

ary = Number()[4][4]
ary2 = Number()[4][4]

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

ary = Number()[4][4]
ary2 = Fixed16()[4][4]

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

ary = Number()[4]

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

test_complex_assignments = """

a = Number(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)
f = Fixed16(publish=True)
g = Fixed16(publish=True)
h = Fixed16(publish=True)
i = Fixed16(publish=True)

ary = Number()[4]
ary1 = Fixed16()[4]

def init():
    pixels.hue = 123
    db.kv_test_key = pixels[0].hue
    a = db.kv_test_key

    pixels[0].val = db.kv_test_key
    b = pixels[0].val

    ary[0] = db.kv_test_key
    c = ary[0]

    ary[1] = 456
    db.kv_test_key = ary[1]
    d = db.kv_test_key

    ary1[1] = pixels[0].val
    e = ary1[1]

    ary1[2] = 0.123
    pixels[0].val = ary1[2]
    f = pixels[0].val

    pixels[1].val = pixels[0].val
    g = pixels[1].val

    pixels[0].val = 0.333
    db.kv_test_array[0] = pixels[0].val
    h = db.kv_test_array[0]

    db.kv_test_array[0] = 456
    pixels[0].val = db.kv_test_array[0]
    i = pixels[0].val
"""

test_basic_string = """
a = String(publish=True)
b = String(publish=True)
c = String(publish=True)
d = String("test3", publish=True)
e = String(publish=True)

def init():
    a = "test"
    s = String('test2')
    b = s
    c = a
    e = d
"""

test_basic_stringbuf = """
a = StringBuf(32, publish=True)
b = StringBuf('test2', publish=True)
c = StringBuf(32, publish=True)

def init():
    a = 'test3'
    c = a
    a = 'test'

"""

test_string_len = """
s1 = StringBuf(32)
s2 = StringBuf('test2')
s3 = String('test')

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    a = len(s1)
    b = len(s2)
    c = len(s3)
    d = len('test_string')

"""


test_gfx_array_load = """

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

"""

test_div_by_zero = """

a = Fixed16(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    zero = Number()
    zero = 0

    a = 123.0 / zero
    b = 123 / zero
    c = 123 % zero

"""

test_pixels_count = """
    
a = Number(publish=True)
b = Number(publish=True)

def init():
    a = pixels.count

    for i in pixels.count:
        b += i

"""


test_pixels_count_assign = """
    
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    pixels.count = 1
    a = pixels.count

    pixels.count = pixels.count + 1
    b = pixels.count

    pixels.count += 1
    c = pixels.count

"""


test_is_v_fading = """

a = Number(publish=True)
b = Number(publish=True)

def init():
    a = pixels.is_v_fading

    pixels.val = 0.5

    b = pixels.is_v_fading

"""


test_is_v_fading2 = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    a = pixels[1].is_v_fading

    pixels[1].val = 0.5

    b = pixels[1].is_v_fading
    c = pixels[0].is_v_fading

    pixels.val = 1.0
    d = pixels[0].is_v_fading

"""


test_is_hs_fading = """

a = Number(publish=True)
b = Number(publish=True)

def init():
    a = pixels.is_hs_fading

    pixels.hue = 0.5

    b = pixels.is_hs_fading

"""


test_is_hs_fading2 = """

a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)
d = Number(publish=True)

def init():
    a = pixels[1].is_hs_fading

    pixels[1].hue = 0.5

    b = pixels[1].is_hs_fading
    c = pixels[0].is_hs_fading

    pixels.hue = 1.0
    d = pixels[0].is_hs_fading

"""

test_formatted_string = """

a = Number(publish=True)
b = StringBuf(32, publish=True)

def init():
    a = 456
    b = "test %5d %d" % (123, a)

"""

class CompilerTests(object):
    def run_test(self, program, expected={}, opt_passes=[OptPasses.SSA]):
        pass

    def test_formatted_string(self, opt_passes):
        self.run_test(test_formatted_string,
            opt_passes=opt_passes,
            expected={
                'a': 456,
                'b': "test   123 456",
            })

    def test_is_hs_fading2(self, opt_passes):
        self.run_test(test_is_hs_fading2,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
                'c': 0,
                'd': 1,
            })

    def test_is_hs_fading(self, opt_passes):
        self.run_test(test_is_hs_fading,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
            })

    def test_is_v_fading2(self, opt_passes):
        self.run_test(test_is_v_fading2,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
                'c': 0,
                'd': 1,
            })

    def test_is_v_fading(self, opt_passes):
        self.run_test(test_is_v_fading,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
            })

    def test_pixels_count_assign(self, opt_passes):
        self.run_test(test_pixels_count_assign,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
            })

    def test_pixels_count(self, opt_passes):
        self.run_test(test_pixels_count,
            opt_passes=opt_passes,
            expected={
                'a': 16,
                'b': 120,
            })

    def test_div_by_zero(self, opt_passes):
        self.run_test(test_div_by_zero,
            opt_passes=opt_passes,
            expected={
                'a': 0.0,
                'b': 0,
                'c': 0
            })

    @pytest.mark.skip
    def test_basic_string(self, opt_passes):
        self.run_test(test_basic_string,
            opt_passes=opt_passes,
            expected={
                'a': 'test',
                'b': 'test2',
                'c': 'test',
                'd': 'test3',
                'e': 'test3',
            })

    def test_basic_stringbuf(self, opt_passes):
        self.run_test(test_basic_stringbuf,
            opt_passes=opt_passes,
            expected={
                'a': 'test',
                'b': 'test2',
                'c': 'test3',
            })    

    def test_string_len(self, opt_passes):
        self.run_test(test_string_len,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 5,
                'c': 4,
                'd': 11,
            })    

    # skip - DB needs to be able to understand gfx16 types to properly handle conversions
    # def test_complex_assignments(self, opt_passes):
    #     self.run_test(test_complex_assignments,
    #         opt_passes=opt_passes,
    #         expected={
    #             'a': 123,
    #             'b': 0.0018768310546875,
    #             'c': 123.0,
    #             'd': 456.0,
    #             'e': 0.0018768310546875,
    #             'f': 0.12298583984375,
    #             'g': 0.12298583984375,
    #             'h': 21823.0,
    #             'i': 0.0069580078125,
    #         })

    def test_bad_data_count(self, opt_passes):
        self.run_test(test_bad_data_count,
            opt_passes=opt_passes,
            expected={
            })

    def test_indirect_load_func_arg(self, opt_passes):
        self.run_test(test_indirect_load_func_arg,
            opt_passes=opt_passes,
            expected={
                'a': 124,
                'b': 123,
            })

    def test_global_avoids_optimize_assign_targets(self, opt_passes):
        self.run_test(test_global_avoids_optimize_assign_targets,
            opt_passes=opt_passes,
            expected={
                'a': 0.0999908447265625,
                'b': 0.0999908447265625,
                'c': 0.0999908447265625,
            })

    def test_pixel_mirror_compile(self, opt_passes):
        self.run_test(test_pixel_mirror_compile,
            opt_passes=opt_passes,
            expected={
            })

    def test_temp_variable_redeclare_outside_scope(self, opt_passes):
        self.run_test(test_temp_variable_redeclare_outside_scope,
            opt_passes=opt_passes,
            expected={
            })

    def test_array_assign_direct_mixed_types(self, opt_passes):
        self.run_test(test_array_assign_direct_mixed_types,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_array_assign_direct(self, opt_passes):
        self.run_test(test_array_assign_direct,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_improper_const_data_type_reuse(self, opt_passes):
        self.run_test(test_improper_const_data_type_reuse,
            opt_passes=opt_passes,
            expected={
                'a': 6,
            })

    def test_var_overwrite(self, opt_passes):
        self.run_test(test_var_overwrite,
            opt_passes=opt_passes,
            expected={
                'a': 91,
            })
    
    def test_var_init(self, opt_passes):
        self.run_test(test_var_init,
            opt_passes=opt_passes,
            expected={
                'a': 5,
                'b': 1.2299957275390625,
                # 'ary1_0': 1,
                # 'ary1_1': 2,
                # 'ary2_0': 1.0999908447265625,
                # 'ary2_1': 2.0999908447265625,
            })

    def test_loop_var_redeclare(self, opt_passes):
        # no value check.
        # this test passes if the compilation
        # succeeds without error.
        self.run_test(test_loop_var_redeclare,
            opt_passes=opt_passes,
            expected={
            })

    def test_array_index_call(self, opt_passes):
        self.run_test(test_array_index_call,
            opt_passes=opt_passes,
            expected={
                'a': 4,
            })

    def test_lib_call(self, opt_passes):
        self.run_test(test_lib_call,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 6,
                'e': 10,
                'f': 1,
            })

    def test_local_declare(self, opt_passes):
        self.run_test(test_local_declare,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 0,
                'c': 0,
                'd': 0,
            })
    
    def test_compare_db(self, opt_passes):
        self.run_test(test_compare_db,
            opt_passes=opt_passes,
            expected={
                'a': 1,
            })

    def test_expr_db(self, opt_passes):
        self.run_test(test_expr_db,
            opt_passes=opt_passes,
            expected={
                'a': 124,
                'b': 246,
            })

    def test_expr_db_ret(self, opt_passes):
        self.run_test(test_expr_db_ret,
            opt_passes=opt_passes,
            expected={
                'a': 123,
            })

    def test_expr_db_ret_binop(self, opt_passes):
        self.run_test(test_expr_db_ret_binop,
            opt_passes=opt_passes,
            expected={
                'a': 124,
            })

    def test_expr_db_f16(self, opt_passes):
        self.run_test(test_expr_db_f16,
            opt_passes=opt_passes,
            expected={
                'a': 124.0,
                'b': 246.0,
            })

    def test_db_augassign(self, opt_passes):
        self.run_test(test_db_augassign,
            opt_passes=opt_passes,
            expected={
                'a': 124,
            })

    def test_db_assign_binop(self, opt_passes):
        self.run_test(test_db_assign_binop,
            opt_passes=opt_passes,
            expected={
                'a': 124,
            })

    def test_db_augassign_indexed(self, opt_passes):
        self.run_test(test_db_augassign_indexed,
            opt_passes=opt_passes,
            expected={
                'a': 124,
            })

    def test_array_expr_db(self, opt_passes):
        self.run_test(test_array_expr_db,
            opt_passes=opt_passes,
            expected={
                'a': 2,
                'b': 5,
            })

    def test_array_expr(self, opt_passes):
        self.run_test(test_array_expr,
            opt_passes=opt_passes,
            expected={
                'a': 3,
                'b': 4,
            })


    def test_array_mod_fixed16(self, opt_passes):
        self.run_test(test_array_mod_fixed16,
            opt_passes=opt_passes,
            expected={
                'a': 4.899993896484375,
                'b': 1.899993896484375,
                'c': 2.899993896484375,
                'd': 3.899993896484375,
                'e': 4.899993896484375,
            })


    def test_array_div_fixed16(self, opt_passes):
        self.run_test(test_array_div_fixed16,
            opt_passes=opt_passes,
            expected={
                'a': 4.996002197265625,
                'b': 1.998992919921875,
                'c': 2.9980010986328125,
                'd': 3.9969940185546875,
                'e': 4.996002197265625,
            })

    def test_array_mul_fixed16(self, opt_passes):
        self.run_test(test_array_mul_fixed16,
            opt_passes=opt_passes,
            expected={
                'a': 627.8088226318359,
                'b': 258.50885009765625,
                'c': 381.6088409423828,
                'd': 504.7088317871094,
                'e': 627.8088226318359,
            })

    def test_array_sub_fixed16(self, opt_passes):
        self.run_test(test_array_sub_fixed16,
            opt_passes=opt_passes,
            expected={
                'a': -118.0,
                'b': -121.0,
                'c': -120.0,
                'd': -119.0,
                'e': -118.0,
            })

    def test_array_add_fixed16(self, opt_passes):
        self.run_test(test_array_add_fixed16,
            opt_passes=opt_passes,
            expected={
                'a': 128.19998168945312,
                'b': 125.19998168945312,
                'c': 126.19998168945312,
                'd': 127.19998168945312,
                'e': 128.19998168945312,
            })

    def test_array_assign_fixed16(self, opt_passes):
        self.run_test(test_array_assign_fixed16,
            opt_passes=opt_passes,
            expected={
                'a': 123.12298583984375,
                'b': 123.12298583984375,
                'c': 123.12298583984375,
                'd': 123.12298583984375,
                'e': 123.12298583984375,
            })

    def test_type_conversions3(self, opt_passes):
        self.run_test(test_type_conversions3,
            opt_passes=opt_passes,
            expected={
                'a': 123,
                'b': 40.95808410644531,
            })

    def test_type_conversions2(self, opt_passes):
        self.run_test(test_type_conversions2,
            opt_passes=opt_passes,
            expected={
                'a': 3,
                'b': 3.12298583984375,
                'c': 3.0,
                'd': 6
            })

    def test_type_conversions(self, opt_passes):
        self.run_test(test_type_conversions,
            opt_passes=opt_passes,
            expected={
                'a': 123,
                'b': 32.0,
                'c': 246.0,
                'd': 246
            })

    def test_fix16(self, opt_passes):
        self.run_test(test_fix16,
            opt_passes=opt_passes,
            expected={
                'a': 123.35600280761719,
                'b': 155.45599365234375,
                'c': 0.25,
                'd': 1.0,
                'n': 123
            })


    # @pytest.mark.skip
    # def test_base_record_assign(self, opt_passes):
    #     self.run_test(test_base_record_assign,
    #         opt_passes=opt_passes,
    #         expected={
    #             'a': 1,
    #             'b': 2,
    #             'c': 3,
    #         })

    # @pytest.mark.skip
    # def test_complex_record_assign3(self, opt_passes):
    #     self.run_test(test_complex_record_assign3,
    #         opt_passes=opt_passes,
    #         expected={
    #             'a': 1,
    #             'b': 2,
    #             'c': 3,
    #             'd': 4,
    #         })

    # @pytest.mark.skip
    # def test_complex_record_assign2(self, opt_passes):
    #     self.run_test(test_complex_record_assign2,
    #         opt_passes=opt_passes,
    #         expected={
    #             'a': 1,
    #             'b': 2,
    #             'c': 3,
    #             'd': 3,
    #             'e': 3,
    #             'f': 3,
    #         })

    # @pytest.mark.skip
    # def test_complex_record_assign(self, opt_passes):
    #     self.run_test(test_complex_record_assign,
    #         opt_passes=opt_passes,
    #         expected={
    #             'a': 1,
    #             'b': 2,
    #             'c': 0,
    #             'd': 0,
    #             'e': 3,
    #             'f': 0,
    #         })

    def test_array_index_3d_aug(self, opt_passes):
        self.run_test(test_array_index_3d_aug,
            opt_passes=opt_passes,
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

    def test_array_index_3d(self, opt_passes):
        self.run_test(test_array_index_3d,
            opt_passes=opt_passes,
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

    def test_array_index_expr(self, opt_passes):
        self.run_test(test_array_index_expr,
            opt_passes=opt_passes,
            expected={
                'a': 3,
            })

    def test_array_index_expr2(self, opt_passes):
        self.run_test(test_array_index_expr2,
            opt_passes=opt_passes,
            expected={
                'a': 2,
            })

    def test_array_index_expr3(self, opt_passes):
        self.run_test(test_array_index_expr3,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 0,
                'c': 2,
                'd': 2,
            })

    def test_array_avg(self, opt_passes):
        self.run_test(test_array_avg,
            opt_passes=opt_passes,
            expected={
                'a': 6,
            })

    def test_array_sum(self, opt_passes):
        self.run_test(test_array_sum,
            opt_passes=opt_passes,
            expected={
                'a': 24,
            })

    def test_array_min(self, opt_passes):
        self.run_test(test_array_min,
            opt_passes=opt_passes,
            expected={
                'a': 1,
            })

    def test_array_max(self, opt_passes):
        self.run_test(test_array_max,
            opt_passes=opt_passes,
            expected={
                'a': 4,
            })

    
    def test_array_aug_assign(self, opt_passes):
        self.run_test(test_array_aug_assign,
            opt_passes=opt_passes,
            expected={
                'a': 579,
                'b': 0,
            })

    def test_array_iteration(self, opt_passes):
        self.run_test(test_array_iteration,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_array_len(self, opt_passes):
        self.run_test(test_array_len,
            opt_passes=opt_passes,
            expected={
                'a': 4,
            })

    def test_array_mod(self, opt_passes):
        self.run_test(test_array_mod,
            opt_passes=opt_passes,
            expected={
                'a': 5,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
            })

    def test_array_div(self, opt_passes):
        self.run_test(test_array_div,
            opt_passes=opt_passes,
            expected={
                'a': 5,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
            })

    def test_array_mul(self, opt_passes):
        self.run_test(test_array_mul,
            opt_passes=opt_passes,
            expected={
                'a': 615,
                'b': 246,
                'c': 369,
                'd': 492,
                'e': 615,
            })

    def test_array_sub(self, opt_passes):
        self.run_test(test_array_sub,
            opt_passes=opt_passes,
            expected={
                'a': -118,
                'b': -121,
                'c': -120,
                'd': -119,
                'e': -118,
            })

    def test_array_add(self, opt_passes):
        self.run_test(test_array_add,
            opt_passes=opt_passes,
            expected={
                'a': 128,
                'b': 125,
                'c': 126,
                'd': 127,
                'e': 128,
            })

    def test_array_assign(self, opt_passes):
        self.run_test(test_array_assign,
            opt_passes=opt_passes,
            expected={
                'a': 123,
                'b': 123,
                'c': 123,
                'd': 123,
                'e': 123,
            })

    def test_array_index(self, opt_passes):
        self.run_test(test_array_index,
            opt_passes=opt_passes,
            expected={
                'a': 5,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 5,
            })

    def test_db_access(self, opt_passes):
        self.run_test(test_db_access,
            opt_passes=opt_passes,
            expected={
                'a': 126,
                'b': 123,
                # 'kv_test_key': 126,
            })

    def test_db_array_access(self, opt_passes):
        self.run_test(test_db_array_access,
            opt_passes=opt_passes,
            expected={
                'db_len': 4,
                'db_len2': 1,
                'a': 2,
                'b': 2,
                'c': 3,
                'd': 4,
                'e': 7,
            })

    def test_empty(self, opt_passes):
        self.run_test(empty_program,
            opt_passes=opt_passes,
            expected={
            })

    def test_basic_vars(self, opt_passes):
        self.run_test(basic_vars,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 0,
                'c': 0,
            })

    def test_basic_assign(self, opt_passes):
        self.run_test(basic_assign,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
                'c': 3,
                'd': 4,
            })

    def test_basic_math(self, opt_passes):
        self.run_test(basic_math,
            opt_passes=opt_passes,
            expected={
                'a': 3,
                'b': 6,
                'c': 9,
                'd': 1,
                'e': 10,
                'f': 2,
                'g': 1,
            })

    def test_constant_folding(self, opt_passes):
        self.run_test(constant_folding,
            opt_passes=opt_passes,
            expected={
                'a': 10,
                'b': 20,
            })

    def test_compare_gt(self, opt_passes):
        self.run_test(basic_compare_gt,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 0,
                'c': 1,
            })

    def test_compare_gte(self, opt_passes):
        self.run_test(basic_compare_gte,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
                'c': 1,
            })

    def test_compare_lt(self, opt_passes):
        self.run_test(basic_compare_lt,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 0,
                'c': 0,
            })

    def test_compare_lte(self, opt_passes):
        self.run_test(basic_compare_lte,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 1,
                'c': 0,
            })

    def test_compare_eq(self, opt_passes):
        self.run_test(basic_compare_eq,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
            })

    def test_compare_neq(self, opt_passes):
        self.run_test(basic_compare_neq,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 0,
            })

    def test_basic_if(self, opt_passes):
        self.run_test(basic_if,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
            })

    def test_basic_for(self, opt_passes):
        self.run_test(basic_for,
            opt_passes=opt_passes,
            expected={
                'a': 10,
            })

    def test_basic_while(self, opt_passes):
        self.run_test(basic_while,
            opt_passes=opt_passes,
            expected={
                'a': 10,
            })

    def test_basic_call(self, opt_passes):
        self.run_test(basic_call,
            opt_passes=opt_passes,
            expected={
                'a': 5,
            })

    def test_basic_return(self, opt_passes):
        self.run_test(basic_return,
            opt_passes=opt_passes,
            expected={
                'a': 8,
            })

    def test_basic_logic(self, opt_passes):
        self.run_test(basic_logic,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 0,
                'c': 1,
                'd': 0,
                'e': 1,
                'f': 1,
            })

    def test_call_with_params(self, opt_passes):
        self.run_test(call_with_params,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 2,
            })

    def test_if_expr(self, opt_passes):
        self.run_test(if_expr,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': 1,
                'c': 2,
            })

    def test_call_expr(self, opt_passes):
        self.run_test(call_expr,
            opt_passes=opt_passes,
            expected={
                'a': 8,
                'b': 18,
            })

    def test_call_register_reuse(self, opt_passes):
        self.run_test(call_register_reuse,
            opt_passes=opt_passes,
            expected={
                'a': 2,
            })

    def test_for_expr(self, opt_passes):
        self.run_test(for_expr,
            opt_passes=opt_passes,
            expected={
                'a': 9,
            })

    def test_while_expr(self, opt_passes):
        self.run_test(while_expr,
            opt_passes=opt_passes,
            expected={
                'a': 4,
            })

    def test_aug_assign_test(self, opt_passes):
        self.run_test(aug_assign_test,
            opt_passes=opt_passes,
            expected={
                'a': 1,
                'b': -1,
                'c': 9,
                'd': 4,
                'e': 2,
            })

    def test_aug_assign_expr_test(self, opt_passes):
        self.run_test(aug_assign_expr_test,
            opt_passes=opt_passes,
            expected={
                'a': 2,
                'b': 2,
                'c': 3,
                'd': 8,
                'e': 10,
            })

    def test_break_node_while(self, opt_passes):
        self.run_test(break_node_while,
            opt_passes=opt_passes,
            expected={
                'i': 6,
            })

    def test_continue_node_while(self, opt_passes):
        self.run_test(continue_node_while,
            opt_passes=opt_passes,
            expected={
                'i': 10,
                'a': 5,
            })

    def test_break_node_for(self, opt_passes):
        self.run_test(break_node_for,
            opt_passes=opt_passes,
            expected={
                'global_i': 6,
            })

    def test_continue_node_for(self, opt_passes):
        self.run_test(continue_node_for,
            opt_passes=opt_passes,
            expected={
                'global_i': 10,
                'a': 6
            })

    def test_double_break_node_while(self, opt_passes):
        self.run_test(double_break_node_while,
            opt_passes=opt_passes,
            expected={
                'i': 6,
                'global_a': 4,
            })

    def test_double_break_node_while2(self, opt_passes):
        self.run_test(double_break_node_while2,
            opt_passes=opt_passes,
            expected={
                'i': 6,
                'global_a': 2,
            })

    def test_double_continue_node_while(self, opt_passes):
        self.run_test(double_continue_node_while,
            opt_passes=opt_passes,
            expected={
                'i': 10,
                'a': 20,
                'global_x': 4,
            })

    def test_double_break_node_for(self, opt_passes):
        self.run_test(double_break_node_for,
            opt_passes=opt_passes,
            expected={
                'global_i': 6,
                'global_a': 4,
                'b': 24,
            })

    def test_double_continue_node_for(self, opt_passes):
        self.run_test(double_continue_node_for,
            opt_passes=opt_passes,
            expected={
                'global_i': 10,
                'a': 24,
                'global_x': 4,
            })

    def test_no_loop_function(self, opt_passes):
        # we don't check anything, we just make sure
        # we can compile without a loop function.
        self.run_test(no_loop_function,
            opt_passes=opt_passes,
            expected={
            })

    def test_pixel_array(self, opt_passes):
        self.run_test(pixel_array,
            opt_passes=opt_passes,
            expected={
                'a': 2,
                'b': 12,
                'c': 3,
                'd': 4,
            })

    def test_multiple_comparison(self, opt_passes):
        self.run_test(multiple_comparison,
            opt_passes=opt_passes,
            expected={
                'e': 1,
                'f': 0,
                'g': 1,
            })

    def test_not(self, opt_passes):
        self.run_test(test_not,
            opt_passes=opt_passes,
            expected={
                'a': 0,
                'b': 1,
                'c': 1,
                'd': 0,
            })

    def test_gfx_array_load(self, opt_passes):
        hsv = self.run_test(test_gfx_array_load, 
            opt_passes=opt_passes,
            expected={
                'a': 0.0999908447265625,
                'b': 0.1999969482421875,
                'c': 0.29998779296875,
                'd': 0.399993896484375,
            })


hue_array_1 = """

def init():
    pixels[1].hue = 1.0
    pixels[1][2].hue = 0.5
    pixels[1][2].hue += 0.1

"""

hue_array_2 = """

def init():
    pixels.hue = 0.5

"""

hue_array_3 = """

def init():
    pixels.hue = 1

"""

hue_array_add = """

def init():
    pixels.hue += 0.1

"""

hue_array_add_2 = """

def init():
    pixels.hue = 1.0
    pixels.hue += 0.1

"""

hue_array_add_3 = """

def init():
    pixels.hue += 1

"""

hue_array_sub = """

def init():
    pixels.hue -= 0.1

"""

hue_array_sub_2 = """

def init():
    pixels.hue = 0.5
    pixels.hue -= 0.1

"""

hue_array_sub_3 = """

def init():
    pixels.hue = 2
    pixels.hue -= 1

"""

hue_array_mul = """

def init():
    pixels.hue = 0.5
    pixels.hue *= 0.5

"""

hue_array_mul_2 = """

def init():
    pixels.hue = 10
    pixels.hue *= 2

"""

hue_array_mul_f16 = """

def init():
    pixels.hue = 1.0
    pixels.hue *= 0.5

"""

hue_array_div = """

def init():
    pixels.hue = 0.5
    pixels.hue /= 2.0

"""

hue_array_div_2 = """

def init():
    pixels.hue = 10
    pixels.hue /= 2

"""

hue_array_div_f16 = """

def init():
    pixels.hue = 0.8
    pixels.hue /= 2.0

"""

hue_array_mod = """

def init():
    pixels.hue = 0.5
    pixels.hue %= 0.3

"""

hue_array_mod_2 = """

def init():
    pixels.hue = 20
    pixels.hue %= 6

"""


sat_array_1 = """

def init():
    pixels[1].sat = 1.0
    pixels[1][2].sat = 0.5

"""

sat_array_2 = """

def init():
    pixels.sat = 0.5

"""

sat_array_3 = """

def init():
    pixels.sat = 15

"""

sat_array_add = """

def init():
    pixels.sat += 0.1

"""

sat_array_add_2 = """

def init():
    pixels.sat += 1

"""

sat_array_sub = """

def init():
    pixels.sat -= 0.1

"""

sat_array_sub_2 = """

def init():
    pixels.sat = 10
    pixels.sat -= 1

"""

sat_array_mul = """

def init():
    pixels.sat = 0.5
    pixels.sat *= 0.25

"""

sat_array_mul_2 = """

def init():
    pixels.sat = 10
    pixels.sat *= 2

"""

sat_array_div = """

def init():
    pixels.sat = 0.5
    pixels.sat /= 2.0

"""

sat_array_div_2 = """

def init():
    pixels.sat = 10
    pixels.sat /= 2

"""

sat_array_mod = """

def init():
    pixels.sat = 0.5
    pixels.sat %= 0.3

"""

sat_array_mod_2 = """

def init():
    pixels.sat = 20
    pixels.sat %= 6

"""

val_array_1 = """

def init():
    pixels[1].val = 1.0
    pixels[1][2].val = 0.5

"""

val_array_2 = """

def init():
    pixels.val = 0.5

"""

val_array_3 = """

def init():
    pixels.val = 25

"""

val_array_add = """

def init():
    pixels.val += 0.1

"""

val_array_add_2 = """

def init():
    pixels.val += 1

"""

val_array_sub = """

def init():
    pixels.val -= 0.1

"""

val_array_sub_2 = """

def init():
    pixels.val = 20
    pixels.val -= 2

"""

val_array_mul = """

def init():
    pixels.val = 0.5
    pixels.val *= 0.5

"""

val_array_mul_2 = """

def init():
    pixels.val = 20
    pixels.val *= 3

"""

val_array_div = """

def init():
    pixels.val = 0.5
    pixels.val /= 2.0

"""

val_array_div_2 = """

def init():
    pixels.val = 50
    pixels.val /= 2

"""

val_array_mod = """

def init():
    pixels.val = 0.5
    pixels.val %= 0.3

"""

val_array_mod_2 = """

def init():
    pixels.val = 20
    pixels.val %= 6

"""

hs_fade_array_1 = """

def init():
    pixels[1].hs_fade = 0.123
    pixels[2].hs_fade = 19
    pixels[1][2].hs_fade = 500

"""

hs_fade_array_2 = """

def init():
    pixels.hs_fade = 200

"""

hs_fade_array_add = """

def init():
    pixels.hs_fade += 100

"""

hs_fade_array_sub = """

def init():
    pixels.hs_fade = 200
    pixels.hs_fade -= 100

"""

hs_fade_array_mul = """

def init():
    pixels.hs_fade = 500
    pixels.hs_fade *= 2

"""

hs_fade_array_div = """

def init():
    pixels.hs_fade = 100
    pixels.hs_fade /= 2

"""

hs_fade_array_mod = """

def init():
    pixels.hs_fade = 500
    pixels.hs_fade %= 3

"""


v_fade_array_1 = """

def init():
    pixels[1].v_fade = 0.123
    pixels[2].v_fade = 19
    pixels[1][2].v_fade = 250

"""

v_fade_array_2 = """

def init():
    pixels.v_fade = 500

"""

v_fade_array_add = """

def init():
    pixels.v_fade += 100

"""

v_fade_array_sub = """

def init():
    pixels.v_fade = 200
    pixels.v_fade -= 100

"""

v_fade_array_mul = """

def init():
    pixels.v_fade = 500
    pixels.v_fade *= 2

"""

v_fade_array_div = """

def init():
    pixels.v_fade = 500
    pixels.v_fade /= 2

"""

v_fade_array_mod = """

def init():
    pixels.v_fade = 500
    pixels.v_fade %= 3

"""


gfx_array_indexing = """

a = Number(publish=True)

def init():
    pixels[a].val = 0.1
    pixels[a + 1].val = 0.2

    a += 1
    pixels[a].val = 0.3
    pixels[3].val = 0.4


"""


test_pix_mov_from_array = """

ary = Number()[4]

def init():
    ary[2] = 1

    pixels.hue = ary[2]

"""


test_pix_load_from_array = """

ary = Number()[4]

def init():
    ary[2] = 1

    pixels[1].hue = ary[2]

"""


test_pix_load_from_pix = """

def init():
    pixels[1].hue = 1
    pixels.hue = pixels[1].hue

"""

test_pix_load_from_pix_2 = """

def init():
    pixels[1].hue = 1
    pixels[2].hue = pixels[1].hue
    pixels[1].hue = 0

"""


test_vector_zero_div = """

def init():
    pixels.hue /= 0
    pixels.sat /= 0
    pixels.val /= 0
    pixels.hs_fade /= 0
    pixels.v_fade /= 0

"""

test_vector_zero_mod = """

def init():
    pixels.hue %= 0
    pixels.sat %= 0
    pixels.val %= 0
    pixels.hs_fade %= 0
    pixels.v_fade %= 0

"""

test_point_zero_div = """

def init():
    pixels[1].hue /= 0
    pixels[1].sat /= 0
    pixels[1].val /= 0
    pixels[1].hs_fade /= 0
    pixels[1].v_fade /= 0

"""

test_point_zero_mod = """

def init():
    pixels[1].hue /= 0
    pixels[1].sat /= 0
    pixels[1].val /= 0
    pixels[1].hs_fade /= 0
    pixels[1].v_fade /= 0

"""

test_hue_p_mov = """

def init():
    pixels[1].hue = 0.1

"""

test_hue_p_add = """

def init():
    pixels[1].hue += 0.1

"""

test_hue_p_sub = """

def init():
    pixels[1].hue -= 0.1

"""

test_hue_p_mul = """

def init():
    pixels[1].hue = 20
    pixels[1].hue *= 2

"""

test_hue_p_mul_f16 = """

def init():
    pixels[1].hue = 0.5
    pixels[1].hue *= 0.5

"""

test_hue_p_div = """

def init():
    pixels[1].hue = 20
    pixels[1].hue /= 2

"""

test_hue_p_div_f16 = """

def init():
    pixels[1].hue = 0.5
    pixels[1].hue /= 2.0

"""

test_hue_p_mod = """

def init():
    pixels[1].hue = 0.5
    pixels[1].hue %= 0.3

"""

test_sat_p_mov = """

def init():
    pixels[1].sat = 0.1

"""

test_sat_p_add = """

def init():
    pixels[1].sat += 0.1

"""

test_sat_p_sub = """

def init():
    pixels[1].sat = 0.2
    pixels[1].sat -= 0.1

"""

test_sat_p_mul = """

def init():
    pixels[1].sat = 20
    pixels[1].sat *= 2

"""

test_sat_p_mul_f16 = """

def init():
    pixels[1].sat = 0.5
    pixels[1].sat *= 0.5

"""

test_sat_p_div = """

def init():
    pixels[1].sat = 20
    pixels[1].sat /= 2

"""

test_sat_p_div_f16 = """

def init():
    pixels[1].sat = 0.5
    pixels[1].sat /= 2.0

"""

test_sat_p_mod = """

def init():
    pixels[1].sat = 0.5
    pixels[1].sat %= 0.3

"""

test_val_p_mov = """

def init():
    pixels[1].val = 0.1

"""

test_val_p_add = """

def init():
    pixels[1].val += 0.1

"""

test_val_p_sub = """

def init():
    pixels[1].val = 0.2
    pixels[1].val -= 0.1

"""

test_val_p_mul = """

def init():
    pixels[1].val = 20
    pixels[1].val *= 2

"""

test_val_p_mul_f16 = """

def init():
    pixels[1].val = 0.5
    pixels[1].val *= 0.5

"""

test_val_p_div = """

def init():
    pixels[1].val = 20
    pixels[1].val /= 2

"""

test_val_p_div_f16 = """

def init():
    pixels[1].val = 0.5
    pixels[1].val /= 2.0

"""

test_val_p_mod = """

def init():
    pixels[1].val = 0.5
    pixels[1].val %= 0.3

"""

test_hs_fade_p_mov = """

def init():
    pixels[1].hs_fade = 10

"""

test_hs_fade_p_add = """

def init():
    pixels[1].hs_fade += 10

"""

test_hs_fade_p_sub = """

def init():
    pixels[1].hs_fade = 20
    pixels[1].hs_fade -= 10

"""

test_hs_fade_p_mul = """

def init():
    pixels[1].hs_fade = 20
    pixels[1].hs_fade *= 2

"""

test_hs_fade_p_div = """

def init():
    pixels[1].hs_fade = 20
    pixels[1].hs_fade /= 2

"""

test_hs_fade_p_mod = """

def init():
    pixels[1].hs_fade = 20
    pixels[1].hs_fade %= 6

"""

test_v_fade_p_mov = """

def init():
    pixels[1].v_fade = 10

"""

test_v_fade_p_add = """

def init():
    pixels[1].v_fade += 10

"""

test_v_fade_p_sub = """

def init():
    pixels[1].v_fade = 20
    pixels[1].v_fade -= 10

"""

test_v_fade_p_mul = """

def init():
    pixels[1].v_fade = 20
    pixels[1].v_fade *= 2

"""

test_v_fade_p_div = """

def init():
    pixels[1].v_fade = 20
    pixels[1].v_fade /= 2

"""

test_v_fade_p_mod = """

def init():
    pixels[1].v_fade = 20
    pixels[1].v_fade %= 6

"""

test_pixelarray_as_func_arg = """

def more_pixel_func(array: PixelArray):
    array.val = 0.75
    array[0].val = 0.9

def pixel_func(array: PixelArray):
    array.val = 0.25
    more_pixel_func(array)

def init():
    pixels.val = 0.5
    pixel_func(pixels)

"""


class HSVArrayTests(object):
    def assertEqual(self, actual, expected):
        assert actual == expected

    def run_test(self, program, opt_passes=[OptPasses.SSA]):
        pass

    def test_pixelarray_as_func_arg(self, opt_passes):
        hsv = self.run_test(test_pixelarray_as_func_arg, opt_passes=opt_passes)

        self.assertEqual(hsv['val'][0], 58982)        

        for val in hsv['val'][1:]:
            self.assertEqual(val, 49152)        

    def test_pix_load_from_pix_2(self, opt_passes):
        hsv = self.run_test(test_pix_load_from_pix_2, opt_passes=opt_passes)

        self.assertEqual(hsv['hue'][0], 0)
        self.assertEqual(hsv['hue'][1], 0)
        self.assertEqual(hsv['hue'][2], 1)

    def test_pix_load_from_pix(self, opt_passes):
        hsv = self.run_test(test_pix_load_from_pix, opt_passes=opt_passes)
        
        self.assertEqual(hsv['hue'][1], 1)
        
    def test_pix_load_from_array(self, opt_passes):
        hsv = self.run_test(test_pix_load_from_array, opt_passes=opt_passes)
        
        self.assertEqual(hsv['hue'][1], 1)

    def test_pix_mov_from_array(self, opt_passes):
        hsv = self.run_test(test_pix_mov_from_array, opt_passes=opt_passes)
        
        self.assertEqual(hsv['hue'][0], 1)

    def test_hue_array_1(self, opt_passes):
        hsv = self.run_test(hue_array_1, opt_passes=opt_passes)
        
        self.assertEqual(hsv['hue'][1], 65535)
        self.assertEqual(hsv['hue'][9], 39321)

    def test_hue_array_2(self, opt_passes):
        hsv = self.run_test(hue_array_2, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 32768)

    def test_hue_array_3(self, opt_passes):
        hsv = self.run_test(hue_array_3, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 1)

    def test_hue_array_add(self, opt_passes):
        hsv = self.run_test(hue_array_add, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 6553)

    def test_hue_array_add_2(self, opt_passes):
        hsv = self.run_test(hue_array_add_2, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 6552)

    def test_hue_array_add_3(self, opt_passes):
        hsv = self.run_test(hue_array_add_3, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 1)

    def test_hue_array_sub(self, opt_passes):
        hsv = self.run_test(hue_array_sub, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 58983)

    def test_hue_array_sub_2(self, opt_passes):
        hsv = self.run_test(hue_array_sub_2, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 26215)

    def test_hue_array_sub_3(self, opt_passes):
        hsv = self.run_test(hue_array_sub_3, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 1)

    def test_hue_array_mul(self, opt_passes):
        hsv = self.run_test(hue_array_mul, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 16384)

    def test_hue_array_mul_2(self, opt_passes):
        hsv = self.run_test(hue_array_mul_2, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 20)

    def test_hue_array_mul_f16(self, opt_passes):
        hsv = self.run_test(hue_array_mul_f16, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 32767)

    def test_hue_array_div(self, opt_passes):
        hsv = self.run_test(hue_array_div, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 16384)

    def test_hue_array_div_2(self, opt_passes):
        hsv = self.run_test(hue_array_div_2, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 5)

    def test_hue_array_div_f16(self, opt_passes):
        hsv = self.run_test(hue_array_div_f16, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 26214)

    def test_hue_array_mod(self, opt_passes):
        hsv = self.run_test(hue_array_mod, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 13108)

    def test_hue_array_mod_2(self, opt_passes):
        hsv = self.run_test(hue_array_mod_2, opt_passes=opt_passes)
        
        for a in hsv['hue']:
            self.assertEqual(a, 2)

    def test_sat_array_1(self, opt_passes):
        hsv = self.run_test(sat_array_1, opt_passes=opt_passes)
        
        self.assertEqual(hsv['sat'][1], 65535)
        self.assertEqual(hsv['sat'][9], 32768)

    def test_sat_array_2(self, opt_passes):
        hsv = self.run_test(sat_array_2, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 32768)

    def test_sat_array_3(self, opt_passes):
        hsv = self.run_test(sat_array_3, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 15)

    def test_sat_array_add(self, opt_passes):
        hsv = self.run_test(sat_array_add, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 6553)

    def test_sat_array_add_2(self, opt_passes):
        hsv = self.run_test(sat_array_add_2, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 1)

    def test_sat_array_sub(self, opt_passes):
        hsv = self.run_test(sat_array_sub, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 0)

    def test_sat_array_sub_2(self, opt_passes):
        hsv = self.run_test(sat_array_sub_2, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 9)

    def test_sat_array_mul(self, opt_passes):
        hsv = self.run_test(sat_array_mul, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 8192)

    def test_sat_array_mul_2(self, opt_passes):
        hsv = self.run_test(sat_array_mul_2, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 20)

    def test_sat_array_div(self, opt_passes):
        hsv = self.run_test(sat_array_div, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 16384)

    def test_sat_array_div_2(self, opt_passes):
        hsv = self.run_test(sat_array_div_2, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 5)

    def test_sat_array_mod(self, opt_passes):
        hsv = self.run_test(sat_array_mod, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 13108)

    def test_sat_array_mod_2(self, opt_passes):
        hsv = self.run_test(sat_array_mod_2, opt_passes=opt_passes)
        
        for a in hsv['sat']:
            self.assertEqual(a, 2)

    def test_val_array_1(self, opt_passes):
        hsv = self.run_test(val_array_1, opt_passes=opt_passes)
        
        self.assertEqual(hsv['val'][1], 65535)
        self.assertEqual(hsv['val'][9], 32768)

    def test_val_array_2(self, opt_passes):
        hsv = self.run_test(val_array_2, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 32768)

    def test_val_array_3(self, opt_passes):
        hsv = self.run_test(val_array_3, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 25)

    def test_val_array_add(self, opt_passes):
        hsv = self.run_test(val_array_add, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 6553)

    def test_val_array_add_2(self, opt_passes):
        hsv = self.run_test(val_array_add_2, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 1)

    def test_val_array_sub(self, opt_passes):
        hsv = self.run_test(val_array_sub, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 0)

    def test_val_array_sub_2(self, opt_passes):
        hsv = self.run_test(val_array_sub_2, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 18)

    def test_val_array_mul(self, opt_passes):
        hsv = self.run_test(val_array_mul, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 16384)

    def test_val_array_mul_2(self, opt_passes):
        hsv = self.run_test(val_array_mul_2, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 60)

    def test_val_array_div(self, opt_passes):
        hsv = self.run_test(val_array_div, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 16384)

    def test_val_array_div_2(self, opt_passes):
        hsv = self.run_test(val_array_div_2, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 25)

    def test_val_array_mod(self, opt_passes):
        hsv = self.run_test(val_array_mod, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 13108)

    def test_val_array_mod_2(self, opt_passes):
        hsv = self.run_test(val_array_mod_2, opt_passes=opt_passes)
        
        for a in hsv['val']:
            self.assertEqual(a, 2)

    def test_hs_fade_array_1(self, opt_passes):
        hsv = self.run_test(hs_fade_array_1, opt_passes=opt_passes)
        
        self.assertEqual(hsv['hs_fade'][1], 0)
        self.assertEqual(hsv['hs_fade'][2], 19)
        self.assertEqual(hsv['hs_fade'][9], 500)

    def test_hs_fade_array_2(self, opt_passes):
        hsv = self.run_test(hs_fade_array_2, opt_passes=opt_passes)
        
        for a in hsv['hs_fade']:
            self.assertEqual(a, 200)

    def test_hs_fade_array_add(self, opt_passes):
        hsv = self.run_test(hs_fade_array_add, opt_passes=opt_passes)
        
        for a in hsv['hs_fade']:
            self.assertEqual(a, 100)

    def test_hs_fade_array_sub(self, opt_passes):
        hsv = self.run_test(hs_fade_array_sub, opt_passes=opt_passes)
        
        for a in hsv['hs_fade']:
            self.assertEqual(a, 100)

    def test_hs_fade_array_mul(self, opt_passes):
        hsv = self.run_test(hs_fade_array_mul, opt_passes=opt_passes)
       
        for a in hsv['hs_fade']:
            self.assertEqual(a, 1000)

    def test_hs_fade_array_div(self, opt_passes):
        hsv = self.run_test(hs_fade_array_div, opt_passes=opt_passes)
        
        for a in hsv['hs_fade']:
            self.assertEqual(a, 50)

    def test_hs_fade_array_mod(self, opt_passes):
        hsv = self.run_test(hs_fade_array_mod, opt_passes=opt_passes)
        
        for a in hsv['hs_fade']:
            self.assertEqual(a, 2)

    def test_v_fade_array_1(self, opt_passes):
        hsv = self.run_test(v_fade_array_1, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 0)
        self.assertEqual(hsv['v_fade'][2], 19)
        self.assertEqual(hsv['v_fade'][9], 250)

    def test_v_fade_array_2(self, opt_passes):
        hsv = self.run_test(v_fade_array_2, opt_passes=opt_passes)
        
        for a in hsv['v_fade']:
            self.assertEqual(a, 500)

    def test_v_fade_array_add(self, opt_passes):
        hsv = self.run_test(v_fade_array_add, opt_passes=opt_passes)
        
        for a in hsv['v_fade']:
            self.assertEqual(a, 100)

    def test_v_fade_array_sub(self, opt_passes):
        hsv = self.run_test(v_fade_array_sub, opt_passes=opt_passes)
        
        for a in hsv['v_fade']:
            self.assertEqual(a, 100)

    def test_v_fade_array_mul(self, opt_passes):
        hsv = self.run_test(v_fade_array_mul, opt_passes=opt_passes)
        
        for a in hsv['v_fade']:
            self.assertEqual(a, 1000)

    def test_v_fade_array_div(self, opt_passes):
        hsv = self.run_test(v_fade_array_div, opt_passes=opt_passes)
        
        for a in hsv['v_fade']:
            self.assertEqual(a, 250)

    def test_v_fade_array_mod(self, opt_passes):
        hsv = self.run_test(v_fade_array_mod, opt_passes=opt_passes)
        
        for a in hsv['v_fade']:
            self.assertEqual(a, 2)

    def test_gfx_array_indexing(self, opt_passes):
        hsv = self.run_test(gfx_array_indexing, opt_passes=opt_passes)
        
        self.assertEqual(hsv['val'][0], 6553)
        self.assertEqual(hsv['val'][1], 19660)
        self.assertEqual(hsv['val'][2], 0)
        self.assertEqual(hsv['val'][3], 26214)

    def test_vector_zero_div(self, opt_passes):
        hsv = self.run_test(test_vector_zero_div, opt_passes=opt_passes)
        
        for v in hsv.values():
            for a in v:
                self.assertEqual(a, 0)

    def test_vector_zero_mod(self, opt_passes):
        hsv = self.run_test(test_vector_zero_mod, opt_passes=opt_passes)
        
        for v in hsv.values():
            for a in v:
                self.assertEqual(a, 0)

    def test_point_zero_div(self, opt_passes):
        hsv = self.run_test(test_point_zero_div, opt_passes=opt_passes)
        
        for v in hsv.values():
            for a in v:
                self.assertEqual(a, 0)

    def test_point_zero_mod(self, opt_passes):
        hsv = self.run_test(test_point_zero_mod, opt_passes=opt_passes)
        
        for v in hsv.values():
            for a in v:
                self.assertEqual(a, 0)

    def test_hue_p_mov(self, opt_passes):
        hsv = self.run_test(test_hue_p_mov, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 6553)

    def test_hue_p_add(self, opt_passes):
        hsv = self.run_test(test_hue_p_add, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 6553)

    def test_hue_p_sub(self, opt_passes):
        hsv = self.run_test(test_hue_p_sub, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 58983)

    def test_hue_p_mul(self, opt_passes):
        hsv = self.run_test(test_hue_p_mul, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 40)

    def test_hue_p_mul_f16(self, opt_passes):
        hsv = self.run_test(test_hue_p_mul_f16, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 16384)

    def test_hue_p_div(self, opt_passes):
        hsv = self.run_test(test_hue_p_div, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 10)

    def test_hue_p_div_f16(self, opt_passes):
        hsv = self.run_test(test_hue_p_div_f16, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 16384)

    def test_hue_p_mod(self, opt_passes):
        hsv = self.run_test(test_hue_p_mod, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hue'][1], 13108)

    def test_sat_p_mov(self, opt_passes):
        hsv = self.run_test(test_sat_p_mov, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 6553)

    def test_sat_p_add(self, opt_passes):
        hsv = self.run_test(test_sat_p_add, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 6553)

    def test_sat_p_sub(self, opt_passes):
        hsv = self.run_test(test_sat_p_sub, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 6554)

    def test_sat_p_mul(self, opt_passes):
        hsv = self.run_test(test_sat_p_mul, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 40)

    def test_sat_p_mul_f16(self, opt_passes):
        hsv = self.run_test(test_sat_p_mul_f16, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 16384)

    def test_sat_p_div(self, opt_passes):
        hsv = self.run_test(test_sat_p_div, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 10)

    def test_sat_p_div_f16(self, opt_passes):
        hsv = self.run_test(test_sat_p_div_f16, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 16384)

    def test_sat_p_mod(self, opt_passes):
        hsv = self.run_test(test_sat_p_mod, opt_passes=opt_passes)
            
        self.assertEqual(hsv['sat'][1], 13108)

    def test_val_p_mov(self, opt_passes):
        hsv = self.run_test(test_val_p_mov, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 6553)

    def test_val_p_add(self, opt_passes):
        hsv = self.run_test(test_val_p_add, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 6553)

    def test_val_p_sub(self, opt_passes):
        hsv = self.run_test(test_val_p_sub, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 6554)

    def test_val_p_mul(self, opt_passes):
        hsv = self.run_test(test_val_p_mul, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 40)

    def test_val_p_mul_f16(self, opt_passes):
        hsv = self.run_test(test_val_p_mul_f16, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 16384)

    def test_val_p_div(self, opt_passes):
        hsv = self.run_test(test_val_p_div, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 10)

    def test_val_p_div_f16(self, opt_passes):
        hsv = self.run_test(test_val_p_div_f16, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 16384)

    def test_val_p_mod(self, opt_passes):
        hsv = self.run_test(test_val_p_mod, opt_passes=opt_passes)
            
        self.assertEqual(hsv['val'][1], 13108)

    def test_hs_fade_p_mov(self, opt_passes):
        hsv = self.run_test(test_hs_fade_p_mov, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hs_fade'][1], 10)

    def test_hs_fade_p_add(self, opt_passes):
        hsv = self.run_test(test_hs_fade_p_add, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hs_fade'][1], 10)

    def test_hs_fade_p_sub(self, opt_passes):
        hsv = self.run_test(test_hs_fade_p_sub, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hs_fade'][1], 10)

    def test_hs_fade_p_mul(self, opt_passes):
        hsv = self.run_test(test_hs_fade_p_mul, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hs_fade'][1], 40)
    
    def test_hs_fade_p_div(self, opt_passes):
        hsv = self.run_test(test_hs_fade_p_div, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hs_fade'][1], 10)

    def test_hs_fade_p_mod(self, opt_passes):
        hsv = self.run_test(test_hs_fade_p_mod, opt_passes=opt_passes)
            
        self.assertEqual(hsv['hs_fade'][1], 2)

    def test_v_fade_p_mov(self, opt_passes):
        hsv = self.run_test(test_v_fade_p_mov, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 10)

    def test_v_fade_p_add(self, opt_passes):
        hsv = self.run_test(test_v_fade_p_add, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 10)

    def test_v_fade_p_sub(self, opt_passes):
        hsv = self.run_test(test_v_fade_p_sub, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 10)

    def test_v_fade_p_mul(self, opt_passes):
        hsv = self.run_test(test_v_fade_p_mul, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 40)
    
    def test_v_fade_p_div(self, opt_passes):
        hsv = self.run_test(test_v_fade_p_div, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 10)

    def test_v_fade_p_mod(self, opt_passes):
        hsv = self.run_test(test_v_fade_p_mod, opt_passes=opt_passes)
            
        self.assertEqual(hsv['v_fade'][1], 2)

# @pytest.mark.skip
@pytest.mark.local
@pytest.mark.parametrize("opt_passes", TEST_OPT_PASSES)
class TestCompilerLocal(CompilerTests):
    def run_test(self, program, expected={}, opt_passes=[OptPasses.SSA]):
        prog = code_gen.compile_text(program, opt_passes=opt_passes)
        func = prog.init_func

        # we don't use the image or stream, but this
        # makes sure they compile to bytecode.
        image = prog.assemble()
        stream = image.render()

        ret_val = func.run()

        regs = func.program.dump_globals()

        for reg, expected_value in expected.items():

            reg_value = regs[reg]

            if isinstance(expected_value, float):
                reg_value /= 65536.0

            try:                
                assert reg_value == expected_value
    
            except AssertionError:
                print('\n*******************************')
                print(program)
                print('Var: %s Expected: %s Actual: %s' % (reg, expected_value, regs[reg]))
                print('-------------------------------\n')
                raise

@pytest.mark.local
@pytest.mark.parametrize("opt_passes", TEST_OPT_PASSES)
class TestHSVArrayLocal(HSVArrayTests):
    def run_test(self, program, opt_passes=[OptPasses.SSA]):
        prog = code_gen.compile_text(program, opt_passes=opt_passes)
        func = prog.init_func

        # we don't use the image or stream, but this
        # makes sure they compile to bytecode.
        image = prog.assemble()
        stream = image.render()

        ret_val = func.run()

        return func.program.dump_hsv()


