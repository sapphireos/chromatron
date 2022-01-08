

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

    assert p1[0].hue == 0.5
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
    pixels[1].hue = 1.0
    pixels[2].hue = 1.5

    assert pixels[0].hue == 0.5
    assert pixels[1].hue == 0.0
    assert pixels[2].hue == 0.5
    assert pixels[3].hue == 0.0

def pix_hue_store2():
    pixels.hue = 1.0
    assert pixels[0].hue == 0.0

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
    assert pixels[1].hue == 0.2


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
