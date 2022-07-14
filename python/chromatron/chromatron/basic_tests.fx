
def var_init():
    a = Number()
    assert a == 0

def arith():
	assert 1 + 2 == 3
	assert 3 - 2 == 1
	assert 3 * 2 == 6
	assert 8 / 4 == 2
	assert 8 % 5 == 3

def arith2():
	assert 1 + 2 - 3 * 10 / 5 == -3

# NAMED_CONST = 4
# def named_const():
# 	assert NAMED_CONST == 4
# 	assert NAMED_CONST + 1 == 5

# 	a = Number()

# 	a = NAMED_CONST * 3
# 	assert a == 12


def simple_ifelse():
    a = Number()
    b = Number()

    if a > 0:
        b = 1

    else:
        b = 2

    assert b == 2
    assert a == 0

def simple_ifelse2():
    a = Number()
    b = Number()

    a = 1

    if a > 0:
        b = 1

    else:
        b = 2

    assert b == 1
    assert a == 1

def simple_if():
    a = Number()
    b = Number()

    b = 2

    if a > 0:
        b = 1

    assert b == 2
    assert a == 0


def simple_if2():
    a = Number()
    b = Number()

    b = 2

    if a == 0:
        b = 1

    assert b == 1
    assert a == 0

def if_expr():
    if 1 + 2 == 3:
        assert True

    else:
        assert False

def multi_if_1():
    a = Number()
    b = Number()

    a = 0

    if a > 0:
        b = 1

    elif b == 0:
        a = 1

    else:
        b = 2

    assert b == 0
    assert a == 1


def multi_if_2():
    a = Number()
    b = Number()

    a = 1

    if a > 0:
        b = 1

    elif b == 0:
        a = 1

    else:
        b = 2

    assert b == 1
    assert a == 1

def multi_if_3():
    a = Number()
    b = Number()

    a = -1
    b = 1

    if a > 0:
        b = 1

    elif b == 0:
        a = 1

    else:
        b = 2

    assert b == 2
    assert a == -1


def nested_ifelse_0():
    a = Number()
    b = Number()
    c = Number()

    a = 0
    b = 0

    if a <= 0:
        if b == 0:
            c = 1

        else:
            c = 2

    else:
        if b == 0:
            c = 3

        else:
            c = 4

    assert c == 1

def nested_ifelse_1():
    a = Number()
    b = Number()
    c = Number()

    a = 0
    b = 1

    if a <= 0:
        if b == 0:
            c = 1

        else:
            c = 2

    else:
        if b == 0:
            c = 3

        else:
            c = 4

    assert c == 2

def nested_ifelse_2():
    a = Number()
    b = Number()
    c = Number()

    a = 1
    b = 0

    if a <= 0:
        if b == 0:
            c = 1

        else:
            c = 2

    else:
        if b == 0:
            c = 3

        else:
            c = 4

    assert c == 3

def nested_ifelse_3():
    a = Number()
    b = Number()
    c = Number()

    a = 1
    b = 1

    if a <= 0:
        if b == 0:
            c = 1

        else:
            c = 2

    else:
        if b == 0:
            c = 3

        else:
            c = 4

    assert c == 4

def for_loop():
    a = Number()

    for i in 4:
        a += 1

    assert a == 4

def double_for_loop():
    a = Number()

    for i in 4:
        for j in 3:
            a += 1

    assert a == 12

def for_loop_break():
    a = Number()

    for i in 4:
        if i == 2:
            break

        a += 1

    assert a == 2

def for_loop_continue():
    a = Number()

    for i in 4:
        if i >= 2:
            continue

        a += 1

    assert a == 2

def while_loop():
    a = Number()

    while a < 4:
        a += 1

    assert a == 4

def while_loop2():
    i = Number()
    i = 4

    a = Number()
    a = 1

    while i > 0:
        i -= 1

        a += 1

    assert a == 5
    assert i == 0

def double_while_loop():
    i = Number()
    i = 4

    a = Number()

    while i > 0:
        i -= 1
        j = Number()

        j = 5

        while j > 0:
            j -= 1

            a += 1

    assert a == 20

def while_loop_break():
    a = Number()

    while a < 4:
        if a == 2:
            break

        a += 1

    assert a == 2

def while_loop_continue():
    a = Number()
    b = Number()

    while a < 4:
        a += 1

        if a >= 2:
            continue

        b += 1

    assert a == 4
    assert b == 1

global_a = Number()
def global_var():
  global_a += 1

  assert global_a == 1

global_b = Number()
def global_readonly():
  assert global_b == 0
  

ary = Number()[4]

def array_lookup():
    b = Number()
    ary = 0
    ary[b] = 1
    b = 1
    ary[b] = 2

    assert ary[0] == 1
    assert ary[1] == 2
    assert ary[2] == 0
    assert ary[3] == 0

def array_lookup_op():
    b = Number()
    ary = 3
    ary[b] = 1
    b = 1
    ary[b] += 2

    assert ary[0] == 1
    assert ary[1] == 5
    assert ary[2] == 3
    assert ary[3] == 3

def array_lookup_op2():
    b = Number()
    ary = 3
    ary[b] = 1
    b = 1
    ary[b] = ary[b] + 2

    assert ary[0] == 1
    assert ary[1] == 5
    assert ary[2] == 3
    assert ary[3] == 3

def array_store():
    ary = 0
    ary[0] = 1
    ary[1] = ary[0]

    assert ary[0] == 1
    assert ary[1] == 1
    assert ary[2] == 0
    assert ary[3] == 0

def array_store2():
    ary = 0
    b = Number()

    ary[b] = 1
    ary[b + 1] = ary[0]

    assert ary[0] == 1
    assert ary[1] == 1
    assert ary[2] == 0
    assert ary[3] == 0

def array_assign():
    ary = 0
    ary = 1
    assert ary[0] == 1
    assert ary[1] == 1
    assert ary[2] == 1
    assert ary[3] == 1

def array_vector():
    ary = 1
    ary += 1
    assert ary[0] == 2
    assert ary[1] == 2
    assert ary[2] == 2
    assert ary[3] == 2


ary2 = Number()[2][3]

def array2_lookup():
    b = Number()
    ary2[b][0] = 1
    ary2[b][1] = 2
    ary2[b][2] = 3
    b = 1
    ary2[b] = 2
    ary2[b][0] = 4
    ary2[b][1] = 5
    ary2[b][2] = 6

    assert ary2[0][0] == 1
    assert ary2[0][1] == 2
    assert ary2[0][2] == 3
    assert ary2[1][0] == 4
    assert ary2[1][1] == 5
    assert ary2[1][2] == 6

def array2_vector():
    ary2 = 3

    assert ary2[0][0] == 3
    assert ary2[0][1] == 3
    assert ary2[0][2] == 3
    assert ary2[1][0] == 3
    assert ary2[1][1] == 3
    assert ary2[1][2] == 3

    ary2 += 1

    assert ary2[0][0] == 4
    assert ary2[0][1] == 4
    assert ary2[0][2] == 4
    assert ary2[1][0] == 4
    assert ary2[1][1] == 4
    assert ary2[1][2] == 4

    ary2[0] = 5

    assert ary2[0][0] == 5
    assert ary2[0][1] == 5
    assert ary2[0][2] == 5
    assert ary2[1][0] == 4
    assert ary2[1][1] == 4
    assert ary2[1][2] == 4

    ary2[0] += 1

    assert ary2[0][0] == 6
    assert ary2[0][1] == 6
    assert ary2[0][2] == 6
    assert ary2[1][0] == 4
    assert ary2[1][1] == 4
    assert ary2[1][2] == 4


def local_array_lookup():
    local_ary = Number()[4]
    b = Number()
    local_ary[b] = 1
    b = 1
    local_ary[b] = 2

    assert local_ary[0] == 1
    assert local_ary[1] == 2
    assert local_ary[2] == 0
    assert local_ary[3] == 0

    local_ary[0] = 1
    local_ary[1] = local_ary[0]

    assert local_ary[0] == 1
    assert local_ary[1] == 1
    assert local_ary[2] == 0
    assert local_ary[3] == 0


def local_array_assign():
    local_ary = Number()[4]
    local_ary = 0
    assert local_ary[0] == 0
    assert local_ary[1] == 0
    assert local_ary[2] == 0
    assert local_ary[3] == 0

    local_ary = 1
    assert local_ary[0] == 1
    assert local_ary[1] == 1
    assert local_ary[2] == 1
    assert local_ary[3] == 1

def local_array_vector():
    local_ary = Number()[4]
    local_ary = 1

    assert local_ary[0] == 1
    assert local_ary[1] == 1
    assert local_ary[2] == 1
    assert local_ary[3] == 1

    local_ary += 1
    assert local_ary[0] == 2
    assert local_ary[1] == 2
    assert local_ary[2] == 2
    assert local_ary[3] == 2

def func_param(a: Number):
    a += 1
    
    assert a == 2


def sub(a: Number, b: Number) -> Number:
    return a - b

def add(a: Number, b: Number) -> Number:
    return a + b

def func_call():
    assert sub(1, 2) == -1
    assert add(1, 2) == 3

def func_indirect_call():
    f = Function(sub)
    f = sub

    assert f(1, 2) == -1

    f = add

    assert f(1, 2) == 3

def func_indirect_call_array():
    f = Function(sub)[2]
    f[0] = sub
    f[1] = add

    assert f[0](1, 2) == -1
    assert f[1](1, 2) == 3

def func_call_throwaway_return():
    add(1, 2)
    # passes if it compiles and runs

def unused(a: Number, b: Number, c: Number) -> Number:
    return a + c

def func_unused_params():
    assert unused(1, 2, 3) == 4

def local_value_init():
    a = Number(5)

    assert a == 5

def type_conversions():
    a = Number()
    b = Fixed16()
    c = Fixed16()
    d = Number()

    a = 123.456
    b = 32

    c = 123
    c += 123

    d = 123.123
    d += 123.123

    assert a == 123
    assert b == 32
    assert c == 246.0
    assert d == 246

def type_conversions_array():
    a = Number()
    b = Fixed16()
    c = Fixed16()
    d = Number()
        
    ary = Fixed16()[4]
    ary2 = Number()[4]

    ary = 3.123

    a = ary[1]
    b = ary[1]

    ary2 = 3.123    
    c = ary2[1]

    ary2 += 3.123
    d = ary2[1]

    assert a == 3
    assert b == 3.12298583984375
    assert c == 3.0
    assert d == 6

def type_conversions_binop():
    a = Number()
    b = Fixed16()

    a = 123
    b = a * 0.333

    assert a == 123
    assert b == 40.95808410644531



p1 = PixelArray(2, 3) # 2 pixels starting at index 3 (4th pixel in array)

def obj_store_direct():
    pixels.hue = 0.0

    assert p1[0].hue == 0.0
    assert p1[1].hue == 0.0
    assert p1[2].hue == 0.0
    
    p1.hue = 0.5

    assert p1[0].hue == 0.5
    assert p1[1].hue == 0.5
    assert p1[2].hue == 0.5

def obj_store_lookup():
    pixels.hue = 0.0

    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]
    
    p.hue = 0.5    

    assert p1[0].hue == 0.5
    assert p1[1].hue == 0.5
    assert p1[2].hue == 0.5
    
def obj_store_lookup2():
    pixels.hue = 0.0

    pa = PixelArray()[2]
    pa[1] = p1
    pa[1].hue = 0.5

    assert p1[0].hue == 0.5
    assert p1[1].hue == 0.5
    assert p1[2].hue == 0.5
    
def obj_store_lookup3():
    pixels.hue = 0.0

    pa = PixelArray()[2]
    pa[1] = p1
    pa[1][2].hue = 0.5

    assert p1[0].hue == 0.5 # there are 2 pixels so the assign should wrap around
    assert p1[1].hue == 0.0
    assert p1[2].hue == 0.5

def obj_store_lookup4():
    pixels.hue = 0.0

    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]
    
    p[2].hue = 1.0    

def obj_load_direct():
    pixels.hue = 0.0

    a = Fixed16()

    a = p1[0].hue

    return a

def obj_load_indirect():
    pixels.hue = 0.0

    a = Fixed16()
    p = PixelArray()    

    p = p1
    a = p[0].hue

    return a

def obj_load_lookup():
    pixels.hue = 0.0

    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]
    
    a = Fixed16()

    a = p[1].hue

    return a

# def obj_load_lookup2():
#     pixels.hue = 0.0

#     a = Fixed16()
#     pa = PixelArray()[2]
#     pa[1] = p1
#     a = pa[1].hue # load from entire array, this will fail to compile (which is correct behavior)

#     return a

def obj_load_lookup3():
    pixels.hue = 0.0

    a = Fixed16()
    pa = PixelArray()[2]
    pa[1] = p1
    a = pa[1][2].hue

    return a

def obj_load_lookup4():
    pixels.hue = 0.0

    a = Fixed16()
    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]

    a = p[2].hue

    return a

def pix_hue_store():
    pixels.hue = 0.0

    pixels[0].hue = 0.5
    pixels[1].hue = 1.0 # hue will shortcut to 65535, instead of wrapping 1.0 to 0.0.  Anything above 1.0 will wrap.
    pixels[2].hue = 1.5

    assert pixels[0].hue == 0.5
    assert pixels[1].hue == 1.0
    assert pixels[2].hue == 0.5
    assert pixels[3].hue == 0.0

def pix_hue_store2():
    pixels.hue = 1.0 # see note above about hue shortcut
    assert pixels[0].hue == 1.0

    pixels.hue = 1.5
    assert pixels[0].hue == 0.5

    pixels.hue = -1.0
    assert pixels[0].hue == 0.0
    
def pix_val_store():
    pixels.val = 0.0

    pixels[0].val = 0.5
    pixels[1].val = 1.0
    pixels[2].val = 1.5

    assert pixels[0].val == 0.5
    assert pixels[1].val == 1.0
    assert pixels[2].val == 1.0
    assert pixels[3].val == 0.0

def pix_val_store2():
    pixels.val = 1.0
    assert pixels[0].val == 1.0

    pixels.val = 1.5
    assert pixels[0].val == 1.0

    pixels.val = -1.0
    assert pixels[0].val == 0.0

def pix_sat_store():
    pixels.sat = 0.0

    pixels[0].sat = 0.5
    pixels[1].sat = 1.0
    pixels[2].sat = 1.5

    assert pixels[0].sat == 0.5
    assert pixels[1].sat == 1.0
    assert pixels[2].sat == 1.0
    assert pixels[3].sat == 0.0

def pix_sat_store2():
    pixels.sat = 1.0
    assert pixels[0].sat == 1.0

    pixels.sat = 1.5
    assert pixels[0].sat == 1.0

    pixels.sat = -1.0
    assert pixels[0].sat == 0.0

def pix_count():
    assert pixels.count == 16

def pix_add_hue():
    pixels.hue = 0.0

    pixels.hue += 0.1
    pixels[1].hue += 0.1

    assert pixels[0].hue == 0.1
    assert pixels[1].hue == 0.199981689453125 # round off error, this is ok


def use_ref_array(b: Number[4]):
    return b[0] + b[1] + b[2] + b[3]

def ref_array_func_call():
    l = Number()[4]

    l[0] = 1
    l[1] = 2
    l[2] = 3
    l[3] = 4

    assert use_ref_array(l) == 10



# test our hello world!
current_hue = Fixed16()
def rainbow_loop():
    current_hue += 0.005

    a = Fixed16()
    a = current_hue

    for i in pixels.count:
        pixels[i].hue = a
        
        a += 1.0 / pixels.count



# string = String("hello!")
# string2 = String(32)

# def load_string():
#     s = String()
#     s = string

#     return s

# def load_string2():
#     s = String()
#     s = string2
    
#     return s

# def load_string_literal():
#     s = String()
#     s = "meow"
    
#     return s


# optimizer tests
def opt_constant_fold():

    a = Number()
    b = Number()

    b = 3

    a = b + 1 + 2 + 7 + b

    assert a == 16

    a = a + 2 + 3
    
    assert a == 21


def redundant_assign():
    a = Number()    
    b = Number()

    a = 4
    b = a
    a = b

    assert a == 4



def init():
    var_init()
    arith()
    arith2()
    simple_ifelse()
    simple_ifelse2()
    simple_if()
    simple_if2()
    if_expr()
    multi_if_1()
    multi_if_2()
    multi_if_3()
    nested_ifelse_0()
    nested_ifelse_1()
    nested_ifelse_2()
    nested_ifelse_3()
    for_loop()
    double_for_loop()
    for_loop_break()
    for_loop_continue()
    while_loop()
    while_loop2()
    double_while_loop()
    while_loop_break()
    while_loop_continue()
    global_var()
    global_readonly()
    array_lookup()
    array_lookup_op()
    array_lookup_op2()
    array_store()
    array_store2()
    array_assign()
    array_vector()
    array2_lookup()
    array2_vector()
    local_array_lookup()
    local_array_assign()
    local_array_vector()
    func_param(1)
    func_call()
    func_indirect_call()
    func_indirect_call_array()
    func_call_throwaway_return()
    func_unused_params()
    local_value_init()
    type_conversions()
    type_conversions_array()
    type_conversions_binop()
    obj_store_direct()
    obj_store_lookup()
    obj_store_lookup2()
    obj_store_lookup3()
    obj_store_lookup4()
    obj_load_direct()
    obj_load_indirect()
    obj_load_lookup()
    # obj_load_lookup2() # this function will not compile (and this is expected behavior!)
    obj_load_lookup3()
    obj_load_lookup4()
    pix_hue_store()
    pix_hue_store2()
    pix_val_store()
    pix_val_store2()
    pix_sat_store()
    pix_sat_store2()
    pix_count()
    pix_add_hue()
    ref_array_func_call()
    rainbow_loop()

    opt_constant_fold()
    redundant_assign()


    