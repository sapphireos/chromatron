

def arith():
	assert 1 + 2 == 3
	assert 3 - 2 == 1
	assert 3 * 2 == 6
	assert 8 / 4 == 2
	assert 8 % 5 == 3

def constant_folding():
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


def while_loop():
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


global_a = Number()
def global_var():
  global_a += 1

  assert global_a == 1
  

ary = Number()[4]

def array_lookup():
    b = Number()
    ary[b] = 1
    b = 1
    ary[b] = 2

    assert ary[0] == 1
    assert ary[1] == 2
    assert ary[2] == 0
    assert ary[3] == 0

def array_store():
    ary[0] = 1
    ary[1] = ary[0]

    assert ary[0] == 1
    assert ary[1] == 1
    assert ary[2] == 0
    assert ary[3] == 0

def array_store2():
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


def local_array():
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

    
def func_param(a: Number):
    a += 1
    
    assert a == 1


def sub(a: Number, b: Number) -> Number:
    return a - b

def add(a: Number, b: Number) -> Number:
    return a + b

def func_call():
    assert sub(1, 2) == -1
    assert add(1, 2) == 3

def func_indirect_call():
    f = Function()
    f = sub

    assert f(1, 2) == -1

    f = add

    assert f(1, 2) == 3

def func_indirect_call_array():
    f = Function()[2]
    f[0] = sub
    f[1] = add

    assert f[0](1, 2) == -1
    assert f[1](1, 2) == 3


p1 = PixelArray(2, 12, size_x=3, size_y=4)

def obj_store_direct():
    p1.hue = 1.0

def obj_store_lookup():
    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]
    
    p.hue = 1.0    

def obj_store_lookup2():
    pa = PixelArray()[2]
    pa[1] = p1
    pa[1].hue = 1.0

def obj_store_lookup3():
    pa = PixelArray()[2]
    pa[1] = p1
    pa[1][2].hue = 1.0

def obj_store_lookup4():
    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]
    
    p[2].hue = 1.0    

def obj_load_direct():
    a = Fixed16()

    a = p1[0].hue

    return a

def obj_load_indirect():
    a = Fixed16()
    p = PixelArray()    

    p = p1
    a = p[0].hue

    return a

def obj_load_lookup():
    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]
    
    a = Fixed16()

    a = p[1].hue

    return a

def obj_load_lookup2():
    a = Fixed16()
    pa = PixelArray()[2]
    a = pa[1].hue

    return a

def obj_load_lookup3():
    a = Fixed16()
    pa = PixelArray()[2]
    a = pa[1][2].hue

    return a

def obj_load_lookup4():
    a = Fixed16()
    pa = PixelArray()[2]
    pa[1] = p1

    p = PixelArray()
    p = pa[1]

    a = p[2].hue

    return a
