
# import import2.fx

# a = Number()
# b = Number()


# # # def init():
# # #     pass

# def add_func(_a, _b):
#     return _a + _b + 2 + 3

# # def do_nothing():
# #     pass

# # def meow():
# #     return 1

# # def local_var():
# #     c = Number()

# #     c = 1 + 2

# #     return 123 + c

# def call_func(_a):
#     c = Number()
#     c = add_func(5 + 6, 2)

#     return add_func(3, _a + 1) + c

# # # def comp():
# # #     return 1 < 2

# def branch():
#     if a < 0:
#         return -1
#     else:
#         return call_func(a)

# # def aug_assign():
# #     a += 1 + 2


# # def my_loop():
# #     for i in add_func(10, 5):
# #         a = a + i

# #     for i in a:
# #         b += i

# def break_cont():
#     for i in a:
#         if i == 15:
#             break

#         a += 1

#     for i in b:
#         if i == -1:
#             continue

#         b += 1


# # def func(x): 
#     # c = Number()
#     # c = add(1, c)

#     # if add(x, x + 456) < 3:
#     #     a = c + 2
#     #     return 4
#     # else:
#     #     b = 2
        

#     # a = c
#     # c = a

#     # return 5


# # def assert_func():
# # #     assert 0 == 0

# MyRec = Record(a=Number(), b=Fixed16(), c=Array(4))
# ary = Array(4)
# ary2 = Array(3, 2, 2, type=MyRec)

# def my_array():
#     a = max(ary)
    # a = len(ary2[1][2])

# #     ary += 1 + 2
#     # ary = 1
    # ary[1] = 2
    # a = ary[2]
    # a = ary
 
#     # a = ary[a] + ary[b]
#     ary[b] = b
#     a = ary[1]
# #     ary[1] = 2

#     # a += ary[1]
# # 
#     # ary[1] = ary[2]

#     # ary[4] += 5
#     # ary[9] = ary[7] + 8

# # # def undefined():
# #     # z = 1


# ary2 = Array(4, 3, 2)

# def array_2d():
#     a = ary2[7][8][9]
    # ary2[1][2][3] = a
    # ary2 = a

#     # ary2[1][2][3] += 1

#     # ary2[1][2][3] = ary2[1][2][3] + 1

#     # a = ary2[1][2][3] + 1

# MyRec = Record(a=Number(), b=Fixed16(), c=Array(4))
# rec = MyRec()
# ary = Array(2, type=MyRec())

# ary2 = Array(2, 2, type=MyRec())
# ary3 = Array(2, 2)
# # # # # # rec2 = MyRec()

# def my_record():
#     c = Number()
# #     c = 1
#     # c = rec['a']

#     ary2[1][2]['c'][0] += 123

    # ary3[1][2] = 1

    # c += 1
    # ary += 1
    # rec['c'] += 1
    # c = ary2[0][2]
    # ary2[1] = 1

    # rec['b'] = 123
    # rec['c'] = 123

    # ary[1]['c'] = 123
    # ary2[1][2]['c'][1] = 123
    # c = ary[1]['c'][0]
    # c = ary2[1][2]

    # ary[1]['a'] = 1
    # ary[1]['c'][3] = 2
    # ary[1]['c'] = 4

# # #     # rec2 = MyRec()  

#     a = rec.a
#     rec.b = b
#     rec.a += 1

#     a = rec.c[1]

#     rec.c = 1
#     rec.c[1] += 1

# # rec_array = Array(4, 2, type=MyRec)

# # def record_ary():
# #     rec_array[0][1].a = 123

# #     # rec_array[1][3].c[4] = 345

# #     # rec.c[4] = 456


# fix16 = Fixed16()

# def try_fix16():
#     # a = 1
#     # fix16 = a
#     # a = 0.2

#     # a = fix16
#     # a = 0.1

#     # fix16 = 2.1 - 0.2
#     # fix16 = 2.1 - 1
#     # fix16 = -0.1
#     # a = fix16

#     # a = 2.1 + 2

#     if fix16 > a:
#         a = 1


#     # fix16 += 0.2

# parray = PixelArray(0, 55, reverse=True)

# def pix_array():
#     # pixels.val = 1.0
#     # pixels.hue = 256
#     # parray.sat = 123.45

#     # pixels.val += 0.1
#     pixels[1][3].val = 1.0
#     # parray[1][3].val += 1.0
#     # parray[1][3].val += 1
    # # a = pixels[1][3].val
    # a += pixels[1][3].val

    # a = pixels.count
    # pixels.count = 123




# a = Number(publish=True)
# b = Number(publish=True)
# c = Number(publish=True)
# d = Fixed16(publish=True)

# def meow(a1, b1):
#     return a1 + b1

# def init():

#     # ary = Array(4)
#     temp = Number()

#     # ary2 = Array(4)
#     # b = ary2[1]

#     temp = meow(1, 2)

#     if a < 2:
#         a = b + c + d + 3
#         temp = b + c

#         if b > 3:
#             temp = 4

#         if temp > 2:
#             return -3

#         # ary += 1

#     else:
#         b = c + d + 1


#     # return temp + ary[0]
#     return b

# @cron('0 1 2 * *')
# @cron('0 0 0 * *')
# def cron_test():
#     pass

# a = Number()

# def init():

#     a = pixels.count

#     for i in pixels.count:
#         pass

# def init():
    # receive('fft', 'fft', ['audio'])

    # pixels[1].v_fade = 500
    # pixels[1][2].v_fade = 250
    # a = rand(2)



# fx_a = Number(publish=True)
# fx_b = Number(publish=True)
# fx_c = Number(publish=True)

# def init():
#     # fx_a = 1
#     # fx_b = 2

#     # fx_c = fx_a + fx_b
#     # # pass


#     # pixels.hue = 0.0
#     pixels[0].hue = 0.1
#     pixels[1][2].val = 0.2

#     fx_a = pixels[0].hue
#     fx_b = pixels[1][2].val

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