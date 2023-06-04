
p1 = PixelArray(3, 2) # 2 pixels starting at index 3 (4th pixel in array)

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

def init():
    obj_store_lookup()

# THIS NEEDS A TEST CASE!

# p1 = PixelArray(2, 12, size_x=3, size_y=4)

# def more_pixel_func(array: PixelArray):
#     # array.val = 0.5
#     # array[0].val = 1.0
    
#     for i in array.count:
#         pass

    
# def pixel_func(array: PixelArray):
    # pass
#     # for i in array.count:
#         # pass
#     # print(array.count)

#     # array.val = 1.0
#     # more_pixel_func(array)
#     # print(array)
#     # pass

#     for i in array.count:
#         array[i].hue = i * 0.1
#         # array[i].hue = 0.1
#     #     pass



# def init():
#     pixels.hue = 0.5

#     # pa = PixelArray()[2]
#     # pa[1] = p1
#     # pa[1].hue = 0.5

#     # assert p1[0].hue == 0.5
#     # assert p1[1].hue == 0.5
#     # assert p1[2].hue == 0.5

#     return
    
#     # pixels.val.sine(0.1)


    # pixels.val = 0.5
    # pixel_func(pixels)
    # pixel_func(p1)

    # array = PixelArray()
    # array = pixels


    # TEST CASE HERE!!!!
    # print(array.count)

    # still broken:
    # pixel_func(array)

    # for i in array.count:
        # pass

# a = Number()
# b = Number()

# def init():

#     a = pixels.count
#     b = pixels.count

#     return a + b


# a = Number(publish=True)
# b = Number(publish=True)

# def init():
#     a = pixels.is_hs_fading

#     pixels.hue = 0.5

#     b = pixels.is_hs_fading



# a = Number(publish=True)
# b = Number(publish=True)
# c = Number(publish=True)

# def init():
#     pixels.count = 1
#     a = pixels.count

#     pixels.count = pixels.count + 1
#     b = pixels.count

#     pixels.count += 1
#     c = pixels.count

# @schedule(hours=23, minutes=0, seconds=0)
# def dim_evening():
#     # db.gfx_sub_dimmer = 16384
#     # pass
#     # fx_debug += 1
#     pixels.hue = 0.0


# db('fx_db', 'uint8', 1)

# fx_thread1 = Number(publish=True)
# fx_thread2 = Number(publish=True)
# fx_loop = Number(publish=True)
# fx_cron = Number(publish=True)

# def thread1():
#     while True:
#         delay(1000)
#         fx_thread1 += 1        

# def thread2():
#     while True:
#         delay(2000)
#         fx_thread2 += 1        

# def init():
#     db.fx_db = 123
   
#     start_thread(thread1)
#     start_thread(thread2)

# def loop():
#     fx_loop += 1


# @schedule(seconds=0)
# @schedule(seconds=15)
# @schedule(seconds=30)
# @schedule(seconds=45)
# def turn_off():
#     fx_cron += 1


# pixel1 = PixelArray(0, 8)
# pixel2 = PixelArray(8, 8)

# fx_send_var = Number(publish=True)
# fx_vm_lib_test = Number(publish=True)

# fx_thread1 = Number(publish=True)
# fx_thread2 = Number(publish=True)

# send('kv_test_key', 'fx_send_var', ['test'], tag='meow')


# def thread1():
#     while True:
#         delay(1000)
#         fx_thread1 += 1        

# def thread2():
#     while True:
#         delay(2000)
#         fx_thread2 += 1        

# def init():
#     pixels.sat = 1.0
#     pixels.val = 1.0

#     pixel1.hue = 0.333
#     pixel2.hue = 0.667

#     fx_send_var = 123

#     fx_vm_lib_test = test_lib_call(1, 2)

#     db.kv_test_key = 456

#     start_thread(thread1)
#     start_thread(thread2)


# def loop():
#     pixel1.hue += 0.001
#     pixel2.hue += 0.001


# a = Number()

# def init():
# send('light_level', 'veml7700_filtered_als', ['shard_1'], tag='meow')
    # a = sync('light_level', ['shard_1'])

# var = Number(persist=True)

# vase = PixelArray(0, 55, reverse=False)
# top = PixelArray(55, 12, reverse=False)


# a = StringBuf(32, publish=True)

# b = Number(456)

# def init():
#     # a = f'meow {123:4} {b}'
#     a = "meow %12d %d" % (123, b)
#     # a = "meow %4d %d" % (123, b)
#     # a = 'meow'
#     # b = 5
#     print('hello')
#     print(a)
#     print('good bye!')

# a = StringBuf(32, publish=True)
# b = StringBuf('test2', publish=True)
# c = StringBuf(32, publish=True)

# ref = String("meow")

# def init():
#     # a = 'test3'
#     # c = a
#     # a = 'test'

#     # assert ref == 'meow'


#     # return len(b)
#     return len('meow')
#     return len(ref)

    # assert a == 'test'

# # sbuf = StringBuf('meow')
# s = StringBuf(32)

# meow = String('meow')

# # sref = String()
# # s2 = String('meow')
# # # s3 = String()
# # s = String()

# # ary = Number()[4]

# # n = Number(123)

# # ary = String()[4]

# def init():
#     s = 'jeremy rocks'
#     print(s)

#     meow = s
#     print(meow)

    # s = meow
    # print(s)

    # return meow == 'meow'
    # return len(meow)

    # ary[0] = 'meow'
    # ary[3] = 'woof'

    # s = String()
    # s = ary[0]

    # print(s)
    # print(ary[0])
    # print(s2)

    # db.kv_test_string = s


    # print(db.kv_test_string)

    # print(ary[3])

    # print(ary[0])

    # for i in len(ary):
    #     # print(ary[i])
    #     pass

    # s = 'meow'

    # s2 = String()
    # s2 = 'meow'
    # print(s2)

    # print(sref)
    # sref = 'meow'
    # print(sref)
    # sref = s2
    # print(sref)

    # pass        
    # n = Number(123)

    # print(n)
    # print(s2)
    # s = 'meow'

    # print(db.kv_test_key)
    # print(123)
    # print(s)
    # print('meow')
    
    # print(ary)

    # db.kv_test_key = 2048
    # db.kv_test_string = 'meow'

# def init():
#     return pixels.is_v_fading

# basic GVN test:
# def init():
#     a = Number(1)
#     b = Number(2)
#     c = Number(4)
#     d = Number(8)
#     e = Number(16)
#     f = Number(32)

#     u = Number()
#     v = Number()
#     w = Number()
#     x = Number()
#     y = Number()
#     z = Number()

#     u = a + b
#     v = c + d
#     w = e + f

#     if u:
#         x = c + d
#         y = c + d

#     else:
#         u = a + b
#         x = e + f
#         y = e + f

#     z = u + y
#     u = a + b

#     return z + u


# a = Number(publish=True)
# b = Number(publish=True)

# def init():
#     a = pixels.count

#     i = Number()
#     # while i < pixels.count:
#     for i in pixels.count:
#         b += i

        # i += 1


# def init():
#     # x = Number()
#     # y = Number()
#     # while x < 2:
#     #     while y < 2:
#     for x in 2:
#         # for y in 2:
#         pixels[x + 2].hue = 1
#         pixels[x + 2].hue = 2

#         # x += 1

#     # a = ary[0]

# current_hue = Fixed16()
# delay = Number()
# cursor = Number()
# # cursor2 = Number()
# target = Number()
# direction = Number()

# # def init():
# #     pixels.val = 1.0
# #     pixels.sat = 1.0

# #     pixels.hue = rand()

# #     pixels.hs_fade = 600

# #     db.gfx_frame_rate = 70

# #     current_hue = rand()

# #     cursor = rand()
# #     target = rand()


# global_i = Number(publish=True)

# def init():
#     for i in 10:
#         if i > 5:
#             break

#     global_i = i

# # def init():
#     a = Number()
#     a = 1

#     if a:
#         print(123)

#     print(2)






# global_a = Number()
# def init():
#     global_a += 1

#     # global_a = 1 
#     print(global_a)

#     assert global_a == 1

#     global_a = 2

# a = Number(publish=True)
# # global_x = Number(publish=True)
# i = Number()
# a = Number()
# b = Fixed16()

# def init():
#     i += 1
#     assert i == 1

#     i += 2
#     assert i == 3

#     i += 3
#     assert i == 6

    # for x in 4:
    #     i = 0

    #     while i < 10:
    #         i += 1

    #         i += 2

    #         i += 3

            # a += 1

# i = Number(publish=True)

# def init():
#     for x in 4:
#     # while i < 4:
#         i += 1

# a = Number(publish=True)
# i = Number(publish=True)

# def init():
    
#     while i < 10:
#         i += 1

#         if i > 5:
#             continue

#         a += 1

# # a = Number(publish=True)
# i = Number(publish=True)

# def init():
#     print(i)

#     if 1:
#         i += 1

#     else:
#         i += 2
#         # print(i)
#         # pass

#         # fence()

#         # i = 3

#     # print(a)
    
#     while i < 10:
#         # i += 1

#         # if i > 5:
#         #     continue

#         # a += 1
#         i += 1


    # return i

# i = Number()

# def init():
#     while i < 0:
#         # if 1:
#             # pass
#         # i += 1
#         pass
#         # i += 1



    # for x in pixels.size_x:
        # pass

# current_hue = Fixed16()
# delay = Number()
# cursor = Number()
# target = Number()
# direction = Number()

# def init():
#     if delay > 0:
#         delay -= 1
#         return

#     pixels[cursor].val = 1.0
#     pixels[cursor].hue = current_hue

#     if direction == 0:
#         cursor += 1
#     else:
#         cursor -= 1

#     if cursor < 0:
#         cursor = pixels.count - 1

#     if (cursor % pixels.count) == (target % pixels.count):
#         cursor = rand(0, pixels.count)
#         direction = rand(0, 2)
#         # target = cursor
#         target = rand()
#         current_hue = rand()

#         delay = rand(0, 10)

# a = Number()

# def init():
    
#     # b = Number()

#     while a < 4:
#         # for j in 3:
#         # b = a

#         a += 1

#     assert a == 4
    # assert b == 3

    # a = Number()
    # b = Number()

    # if a > 0:
    #     b = 1


    # a += 1
    # # fence()
    # a += 2

    # while a > 0:
    #     if a:
    #         return

    #     a -= 1


    # if 1:

    #     a = 2

    # if 1:

    #     while a > 0:
    #     # for i in 2:
            

    #         if b:
    #             continue

    #         elif a == 3:
    #             # print(a)
    #             break

    #         a -= 1

    # else:

    #     print(1)


# # def loop():
# def init():
#     a = Number()
#     delay += 2

#     while delay > 0:
#         delay -= 1

#         a += 1

#     return a
    
#     # delay += 2

#     # # a = Number()
#     # # a = cursor
#     # # cursor += 1

#     # # fence(cursor)

#     # if delay > 0:
#     #     delay -= 1
#     #     return

#     # cursor += 1

#     # delay += 10



#     # pixels[cursor].val = 1.0
#     # pixels[cursor].hue = current_hue

#     # if direction == 0:
#     #     cursor += 1
#     # else:
#     #     cursor -= 1

#     # if cursor < 0:
#     #     cursor = pixels.count - 1

#     # if (cursor % pixels.count) == (target % pixels.count):
#     #     cursor = rand(0, pixels.count)
#     #     direction = rand(0, 2)
#     #     # target = cursor
#     #     target = rand()
#     #     current_hue = rand()

#     #     delay = rand(0, 10)






# c = Number()

# def init():
#     i = Number()
#     i = 4

#     a = Number()
#     b = Number()
#     b = c

#     while i > 0:
#         i -= 1

#         a = 2 + b

#     return a


# ary = Number()[4]

# a = Number(publish=True)
# b = Number(publish=True)
# c = Number(publish=True)
# d = Number(publish=True)
# e = Number(publish=True)

# def init():

#     ary[0] = 1
#     ary[1] = 2
#     ary[2] = 3
#     ary[3] = 4
#     ary[4] = 5

#     ary += 123

#     a = ary[0]
#     b = ary[1]
#     c = ary[2]
#     d = ary[3]
#     e = ary[4]

# a = Number(publish=True)
# # b = Number(publish=True)
# # c = Number(publish=True)

# # def test(_a, _b):
#     # return _a + _b

# def init():
#     # b = Number()
#     if 1 + 2 == 3:
#         a += 5
#         a += 4

#     if 1 + 2 != 3:
#         a += 2

#     a += 1

#     # return b + 1

#     # if test(1, 2) == 3:
#     #     b = 1

#     # if test(1, 2) != 3:
#     #     b = 2

#     # if test(1, 2) + 3 == 3:
#     #     c = 1

#     # if test(1, 2) + 3 != 3:
#     #     c = 2

# def init():
#     # a = Number()
#     b = Fixed16()

#     b = 1 + 2

#     return b

# global_a = Number()
# def init():
#   global_a += 1

#   assert global_a == 1

# global_b = Number()
# def init():
#   assert global_b == 0
  

# ary = Number()[4]

# a = Number(publish=True)
# b = Number(publish=True)
# # c = Number(publish=True)
# # d = Number(publish=True)
# # e = Number(publish=True)

# def init():

#     ary[0] += 1
#     ary[0] += 1

#     # ary[0] = 1
#     # ary[0] = 2

#     # ary[0] = 2
#     # ary[2] = 3
#     # ary[3] = 4
#     # ary[4] = 5

#     # ary += 123

#     # a = ary[0]
#     # b = ary[0]
#     # c = ary[2]
#     # d = ary[3]
#     # e = ary[4]


# ary = Number()[4]

# def init():
#     ary += 1
#     # b = Number()
#     # ary[0] += 1
    # ary[0] += 1

    # ary[0] = ary[0] + 1
    # b = 1
    # ary[b] = 2

    # assert ary[0] == 1
    # assert ary[1] == 2
    # assert ary[2] == 0
    # assert ary[3] == 0

# global_a = Number()
# def init():
#   global_a += 1

#   assert global_a == 1

# a = Number()
# b = Number()

# # def load_store_1():
# #     if 1:
# #         a += 1

# #     else:
# #         a += 2

# # def load_store_2():
# #     a += 1
    
# #     if 1:
# #         a += 1

# #     else:
# #         a += 2

# #     a += 1

# def load_store_3():
#     if 1:
#         a += 1

#     else:
#         a += 2
#         b += 3

#         return

#     a += 1

# # def load_store_4():
# #     a += 1
# #     fence()
# #     a += 1


# def init():
#     # load_store_1()
#     # load_store_2()
#     load_store_3()
#     # load_store_4()


# def init():
#     if 1:
#         b += 1

#     else:
#         a += 2

#         # fence()
#         # meow(a)

#         a += 4

#         # return

#     b += 3

#     # yield()
#     # return



# def init():
#     a = Number()
#     b = Fixed16()

#     a = 123
#     b = a * 0.333

#     # assert a == 123
#     assert b == 40.95808410644531
#     # assert b == 40.959

# def init():
#     a = Number()
#     b = Fixed16()
#     c = Fixed16()
#     d = Number()

#     a = 123.456
#     b = 32

#     c = 123
#     c += 123

#     d = 123.123
#     d += 123.123

#     assert a == 123
#     assert b == 32
#     assert c == 246.0
#     assert d == 246




# g1 = Number()    
# g2 = Number()    

# def init():
#     i = Number()
#     i = 4

#     a = Number()
#     a = 1

#     while i > 0:
#         i -= 1

#         a += 1

#     assert a == 5
#     assert i == 0

# def init():
#     a = Number()

#     a = 0

#     while a < 4:
#         a += 1

#     assert a == 4




# def init():
#     i = Number()
#     i = 4

#     a = Number()

#     while i > 0:
#         i -= 1
#         j = Number()

#         j = 5

#         while j > 0:
#             j -= 1

#             a += 1

#     assert a == 20
    
# def init():
#     a = Number()
#     b = Number()

#     if a > 0:
#         b = 1

#     else:
#         b = 2

#     assert b == 2
#     assert a == 0


# def init():
#     a = Number()
#     b = Number()

#     b = 2

#     if a > 0:
#         b = 1

#     assert b == 2
#     assert a == 0


# def init():
#     a = Number()
#     b = Number()

#     b = 2

#     if a == 0:
#         b = 1

#     assert b == 1
#     assert a == 0




# def init():

#     a = Number()
#     b = Number()

#     b = 3

#     a = b + 1 + 2 + 7 + b
#     a = a + 2 + 3

#     return a

# def init():
#     a = Number()    
#     b = Number()

#     a = 4
#     b = a
#     a = b

#     return a



# def init():

#     a = Number()
#     b = Number()

    
#     a = 4
#     a = g1
#     b = g1

#     return a

# def array_vector():
#     ary = 1
#     ary += 1
#     assert ary[0] == 2
#     assert ary[1] == 2
#     assert ary[2] == 2
#     assert ary[3] == 2

# ary = Number()[4]

# def init():
# # #     ary[0] = 1
# # #     # ary[0] = 1 + ary[0]

#     return meow(ary)

#     # print(ary[0])

# a = Number(220000)
# string = String("hello!")
# string2 = String("world!")
# string2 = String(32)

# array = String(32)[4]

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

# def do_stuff():
#     s = String()
#     s = string
#     print(s)

# sg = Number()[4]

# def init():
#     s = Number()[4]

#     sg[0] = s[0]

# #     return s


# sg = String(32)
# sg2 = String(32)

# pix1 = PixelArray(2, 3)

# a = Number()[4]

# def woof(b: Number[4]):
#     return b[0] + b[2]

# def meow(b: Number[4]):
#     # return b[0] + b[2]
#     # return b
#     return woof(b)

# def init():
#     l = Number()[4]
#     # return a

#     l[0] = 1
#     l[1] = 2
#     l[2] = 3
#     l[3] = 4

#     return meow(l)

#     # f = Function()
#     # f = meow

#     # f2 = Function()[4]

#     # return f

#     # return meow


# # #     a = Number()

# #     a = l[0][1]

# #     return a + l[1][2]

#     # return l[0] + l[1]

#     return meow(l)

    # return woof(1)
    # return add(1, 2)
    # pass

    # min(a)
    # min(l)

#     s = String(32)
#     s2 = String(32)

#     s = 'meow'

#     sg = s

#     sg2 = 'woof'

#     sg = sg2

#     s2 = s

#     return s

#     s = array[2]
#     # array[2] = 'meow'

#     return s

#     # pass
#     global_var()

#     # global_a += 1
    
#     # i = Number()

#     # while i < 32:

#     #     global_a += 1

#     #     i += 1

#     global_a += 2

#     print(global_a)





    # # return 1 + 2
    # pixels.hue = 0.667
    # pixels[0].hue = 0.1
    # pixels.hue += 0.667
    # pixels[0].hue += 0.667
#     a = 123

#     meow = Number()[4]
#     meow = a

#     a = meow[1]

#     return a


    # for i in 32:
        # print(i)

    # i = Number()
    # while i < 32:
    #     i += 1



# def sub(a: Number, b: Number) -> Number:
#     return a - b

# def add(a: Number, b: Number) -> Number:
#     return a + b

# def func_call():
#     assert sub(1, 2) == -1
    # assert add(1, 2) == 3

    # f = Function()
    # f = sub

    # assert f(1, 2) == -1

    # f = add

    # assert f(1, 2) == 3

# receive('portal_hue', 'portal_hue', ['portal_gun'])

# def init():
    # receive('portal_hue', 'portal_hue', ['portal_gun'])

# global_a = Number(3)
# global_b = Number()
# global_c = Number(200000)

# def init():
    # func_call()

    # return 200001 + 200000

    
#     global_d = Number(2)



# p1 = PixelArray(2, 3)

# def obj_load_lookup3():
#     # pixels.hue = 0.0

#     a = Fixed16()
#     pa = PixelArray()[2]
#     pa[1] = p1
#     a = pa[1][2].hue

#     return a


# s = String("hello!")
# s1 = String(32)

# def str_func():

#     s2 = String()
#     # s2 = s1
#     s2 = "meow"

#     print(s2)

# def conv():

#     a = Number()
#     a = 1 + 2.0
#     # a = 1

#     # b = Fixed16()
#     # b = a

#     return a
    # return b


# def stuff(a: Number) -> Number:
#     if a > 3:
#         return 1 + 2

#     return 4.0

# def stuff(a: Number, b: Number) -> Number:
#     return a - b


# def meow(f: Function):
    # return f()


# b = Number()



# def func2():
#     # b = Number()
#     # b = a[3]

#     # p1[a[3]].hue = 1

#     # a[0] = 1
#     # a.hue = 1

# # def my_func():
#     # pm = PixelArray()
# #     pm.hue = 1

#     # return stuff(1, 2)
#     # return my_func()
#     # f = Function()
#     # f = stuff
#     # return f(1, 2)

#     # pixels.hue = 0

#     # p1[1].hue = 1

#     pa = PixelArray()[2]
#     pa[1] = p1

#     # p1.hue = 1.0
#     # pa[1].hue = 1.0
#     p = PixelArray()
#     p = pa[1]

#     p.hue = 1

    # return p1

    # f = Function()
    # f = my_func
    # f()


    # f = Function()[2]
    # f[1] = stuff
    # return f[1](1, 2)

    # m = Function()
    # m = f[1]
    # return m(1, 2)
    

    # return f[1]

    # a = Number()[2]

    # a[0] = 3
    # a[1] = 4
    # a[3] = 8    

    # return a[1]
    # return b

    # return f
    # f()


# def func():
    # db.meow[1] = 2
    # pixels[1].hue += 2
    # pixels.hue += 2
    # pixels.hue = 2
    # b = Number()
    # b = pixels[1].hue
    # return pixels[1].hue

# A = 4 # named constant.  recommend all caps for these (like C's convention)
      # these can be used the same as a const number.
      # instances of A will be replaced with 4.

# meow = Number()[A]

# a = Number()
# a = Number()[4] # array of 4 Numbers
# c = Number()[4][2] # 2D array of 4x2 (8) Numbers

# My_struct = Struct(a=Number(), b=Number()[2]) # declares a new struct type, not an instance of a struct
# # for consistency and readability, let's require that struct typedefs are capitalized
# # to match the other type declare functions

# s = My_struct(a=1, b=2)
# s = My_struct()
# a = s['a'] # record element access


# st = String(32) # 32 char empty string
# st = String("hello!") # declare an initialized string
# st = "meow!" # assign new string to var st.  this is an assign, not a declare

# dt = Datetime() # this is a built in struct type

# all composite data types live in memory and fields are loaded to registers as needed.
# what about temporary composites created for the duration of a function?
# if function stack frames are placed in the memory segment, this is probably fine?

# for memory lookups - all functions have a base register set when called
# for their function local storage.  this is placed after globals and grows
# like a stack.
# we could optimize this with a different load/store instruction that accesses
# a different memory bank?

# Objects
# use the . syntax:
# object.property
# object.method()

# objects are an API construct provided by the underlying VM system.
# they cannot be declared or created within an FX program, only used when available.
# pixel arrays and the KV database are the main object APIs the compiler knows about.
# generic objects could be provided by the VM and discovered dynamically.



# Link system:
# change to system library call.

# s = String("hello!")
# s1 = String(32)

# # # # a = Number()[4]
# # # # def obj(a: Number):
# # # a = Number()
# def obj():

#     s2 = String()
#     # s2 = s1
#     s2 = "meow"

#     print(s2)


    
# #     b = Number()
# # #     # b += 1

# # #     # if True:
# # #         # a = Number()
# # #         # a = 4

# # #     a += 4


# # #     return a
    # return s2

#     b = s2.len

#     # s2 = "meow"
#     # s2 = "woofmeow"
#     return s2
#     pass

#     a += 1
#     return a
    # pass

    # b = Number()

    # return len(a)

    # b += a

    # return a[b]

    # b = Number()
    # b = pixels[7].hue

    # return b

    # pixels[7][6].hue += 1 

#     a[b] = 1

#     b = 1

#     a[b] = 2

#     return a[0]

    # a[1] = 2

    # return a[0] + a[1]

    # i = Number()
    # while i < 4:
    #     a[i] = i + 1

    #     i += 1

    # return a[0]

    # pass
    # a = 1 + 2
    # a = 1 + 2
    # a[1] += 2

#     s['a'] += 1
    # return s

# #     # a += A

    
# #     b = 1
# #     b = 2

    # b = Number()

    # b = a[1]
    # # a[a[1]] = b


    # # b = 3
    # # # b = a[a[a[1]]]


    # return b

    # a[1] = a[2][b][a[3]]
    # a[0] = 1
    # a = a[1]
    # a = 1

    # a[0] += a['a']

    # return a[0]

    # global_a += 1

  
    # meow.x += 1
    
    # return meow.x + meow.y

    # a = Number()

    # a = meow.x


# a = Number()
# b = Number()
# c = Number()

# def simple_binop():
#     b = 1
#     a = b + 3
#     c = b + 3
        
#     # b = 1 + 2 + 3 # 6

#     # a = b + 2 # 8
#     # c = a # 8

#     # b = b + 2 # 8


#     # b += 3 # 11

#     # a = a * c * 3 # 193

#     return a + c


# def copy_prop():
#   a = Number()
#   b = Number()
#   c = Number()
#   d = Number()
#   e = Number()
#   f = Number()
    
#   u = Number()
#   v = Number()
#   w = Number()
    
#   x = Number()
#   y = Number()
#   z = Number()

#   a = f
#   b = a
#   c = f

#   u = b + a
#   v = c + d
#   w = e + f

#   if True:
#       x = c + d
#       y = c + d

#   else:
#       u = a + b
#       x = e + f
#       y = e + f

#   z = u + y
#   u = a + b
    
# def double_while_loop():
#     i = Number()
#     i = 4

#     while i > 0:
#         i -= 1
#         j = Number()

#         j = 4

#         while j > 0:
#             j -= 1



# def while_if_while_expr():
#     a = Number()
#     b = Number()

#     while a > 0:
#         if a > 0:
#             while a > 0:
#                 b += a

#     return b
        

# def another_while_if():
#     a = Number()

#     while a > 0:
#         if True:
#             pass


# def func():
#     a = Number()
    
#     if a > 0:
#         while True:
#             if a > a:
#                 pass
    

# def while_while_expr():
#     a = Number()
#     b = Number()

#     while a > 0:
#         while a > 0:
#             b += 1

#     return b
        
# def if_while_expr():
#     a = Number()
#     b = Number()

#     if a > 0:
#         while a > 0:
#             b += a

#     return b

# def while_if_while():
#     a = Number()
#     b = Number()

#     while a > 0:
#         if a > 0:
#             while a > 0:
#                 b += a
#     return b


# def const_const_if_if():    
#     a = Number()
#     if True:
#         a += -4
        
#     if -4 < -4:
#         pass
    
# def if_if_expr():
#     a = Number()

#     if a > 0:
#         if a > 0:
#             a += 1

#     return a
        

# def another_while_if():
#     a = Number()

#     while a > 0:
#         if True:
#             pass


# def simple_ifelse():
#     a = Number()
#     b = Number()

#     if a > 0:
#         b = 1

#     else:
#         b = 2

#     return b


# def two_ifelse():
#     a = Number()
#     b = Number()

#     if a > 0:
#         b = 1 + 2 + 3 + 4 + 5

#     else:
#         b = 2 + 1

#     if b < 0:
#         a = 1

#     else:
#         b = 2

#     return b


# def two_ifelse_expr():
#     a = Number()
#     b = Number()

#     if a > 0:
#         a = 1

#     else:
#         b = 2
#         a = 8

#     if b < 0:
#         a = 3

#     else:
#         b = 4

#     return b + a
#     # return a



# def while_loop():
#     i = Number()
#     i = 4

#     while i > 0:
#         i -= 1


# def double_while_loop():
#     i = Number()
#     i = 4

#     while i > 0:
#         i -= 1
#         j = Number()

#         j = 4

#         while j > 0:
#             j -= 1



# def while_with_if_loop():
#     i = Number()
#     i = 4
#     a = Number()
#     b = Number()

#     while i > 0:
#         i -= 1

#         if a < b:
#             i = 0


# def stuff():
#   i = Number()
#   # y = Number()
#   # z = Number()

#   while i > 1:
#       # y = i
#       # i += 1

#       # if i == 1:
#       #   pass

#       while i < 2:
#       #   a = Number()
#       #   a += 1
#           pass


    # z = i + 1


# def lost_copy_problem():
#   i = Number()
#   y = Number()
#   z = Number()

#   while True:
#       y = i
#       i += 1

#   z = i + 1


# def swap_problem():
#   x = Number()
#   y = Number()
#   t = Number()
#   a = Number()
#   b = Number()

#   while True:
#       t = x
#       x = y
#       y = t

#   a = x
    # b = y

# def loop():
#   i = Number()
#   # i = 2 + 3
#   i = 4

#   # a = Number()

#   while i > 0:
#       i -= 1

#       # if i == 10:
#       #   break
#           # pass
            
#       # a = 2 + 3

#   # return a
#   return i



# def lvn():
#   a = Number()
#   b = Number()
#   c = Number()
    
#   z = Number()

#   a = z
#   b = a
#   c = z

# def copy_prop():
#   a = Number()
#   b = Number()
#   c = Number()
#   d = Number()
#   e = Number()
#   f = Number()
    
#   u = Number()
#   v = Number()
#   w = Number()
    
#   x = Number()
#   y = Number()
#   z = Number()

#   a = f
#   b = a
#   c = f

#   u = b + a
#   v = c + d
#   w = e + f

#   if True:
#       x = c + d
#       y = c + d

#   else:
#       u = a + b
#       x = e + f
#       y = e + f

#   z = u + y
#   u = a + b




# def test():
#   a = Number()
#   b = Number()
#   c = Number()

#   a = 2 + 3
#   b = a
#   c = b

#   return c



# def double_while_loop():
#   i = Number()
#   i = 4

#   while i > 0:
#       i -= 1

#       j = Number()
#       j = 5
#       while j < 10:
#           j += 1


# def while_loop():
#   i = Number()
#   i = 4

#   while i > 0:
#       i -= 1

#   return i

# def loop_invariant_code_motion2():
#   i = Number()
#   # i = 4

#   a = Number()
#   # a = 4

#   while i > 0:
#       # i -= 1

#       if i == 100:
#           a = 2 + 3

#           # break

#           # pass
#       # else:
#           # a = 5
#           # pass

            

#       # break
#       # pass

#   # return a


# def loop_invariant_code_motion_induction():
#   i = Number()
#   i = 4

#   while i > 0:
#       # i -= 1
#       if i == 1:
#           break

#       # if i == 1:
#       i = 2

#   return i


# global_a = Number()
# def global_var():
#   global_a += 1

#   return global_a

# def while_loop():
#   i = Number()
#   i = 4

#   while i > 0:
#       i -= 1

#   return i

# global_b = Number()
# def constant_folding():
#   a = Number()

#   # a = 1 + 2

#   # a += 1

#   return a - 0

#   # return a

#   a = 3 + 4 + 5

#   return a


#   a = 6 * global_b - 7

    # global_b = 0
    # a = 1 - a

# global_a = Fixed16()
# global_b = Number()

# def simple_ifelse():
#   a = Number()
#   b = Number()

#   if a > 0:
#       b = 1

#   else:
#       b = 2

#   return b



# def while_loop_multiple_condition():
#   i = Number()
#   i = 4

#   a = Number()
#   a = 1

#   # this breaks liveness!!!
#   while i > 0 and a == 2:
#   # while i > 0 and a == 2:
#   # while a == 2:
#       i -= 1

#       a = 8 + 8

#   return a


# def while_loop_multiple_condition():
#   a = Number()
#   while a == 2:
#       pass

    # return a



# def while_loop():
#   i = Number()
#   i = 4

#   # if i == 1:
#       # return 2

#   a = Number()


#   # this breaks liveness!!!
#   while i > 0 and a == 2:
#       i -= 1



#       # a = 2 + i

#       # a = 3 + 4

#       # a = Number()
#       # a = 2 + i + 4
#       # # a = 4

#       # while a > 0:
#       #   a -= 1
#       #   if a == 0:
#       #       i -= 2
#       #   else:
#       #       i = 1

#       # if i == 1:
#           # i = 2

#       # else:
#           # i += 3

#       # while a == 2:
#       #   a += 1
#       #   i -= 3
#           # i = a + 2 + 3

#       # i += 1

#   return a

# def simple_array():
#   a = Number(count=4)
#   # pass

#   b = Number()

#   # a = b[0] + a

#   # a[0][1].h = b
#   global_a += 1

#   global_b = 3


#   global_a = 1 + 2

#   global_a = a + b

#   return 2
    # b = a.x.y
    # b[0] = a[1]

    # b = a[b + 3]

    # b = a[0].v[1].z.x
    # a[0].v[1].z.x = b
    # a[0].v[1].z.x = b[2].w[3][4]
    
    # a = Array(4, 3, 2)

    # return a[0][1][2]

    # pixels[2].val += 1
    # pixels.val += 1

    # b = Number()

    # b = 1 + 2 + 3 +4

    # b = 0

    # if b == 1:
    #   return 2

    # return b + 4


# def func():
#   b = Number() # defines b0

#   # return 0

# #     b += 1 # uses b0 and defines b1

#   if b > 0: # uses B1
# #         b = b + 4 # defines b2

#       return b

#   else:
# # #       # b = 3 # defines b3
#       b += 3
# # # #         # pass

# # #       if b > 2:
# # #           b = 0

#   return b # uses b
# #     # need to define b4 and use b4 here

# def func():
#   b = Number()
#   i = Number()
#   i = 4

#   while i > 0:
#       i -= 1

#       b += i

#       if i < 2:
#           i = 3
        

#   if b == 0:
#       b = 2

#   return b



# # def func(b: Number):
# def func():
#   b = Number() 
#   temp_a = Number()

#   b += 1

#   # b = 1.0 + 2
#   # temp_b = Number()

#   if temp_a > 0:
#       # temp_b = Fixed16()
#       # temp_b = 1.0 + b
#       b = temp_a + 3
        
#   else:
#       if temp_a > 0:
#           # temp_a += 1
#           b = 1

#       # temp_b = Fixed16()
        
#       # temp_a += 2
#       # b += 3

#   # temp_b += 1
        
#   return b


# ********************************
# Func code:
# ********************************
# 3 temp_a = Number()
#           Var(temp_a_v1, i32) = Const(0, i32)
# 5 b += 1
#           Var(%0, i32) = Var(b_v0, i32) add Const(1, i32)
#           Var(b_v1, i32) = Var(%0, i32)
# 7 if temp_a > 0:
#           Var(%1, i32) = Var(temp_a_v1, i32) gt Const(0, i32)
#           Var(%2, i32) = Var(%1, i32)
#           BR Z Var(%2, i32) -> if.else.0 (Line 0)
# LABEL if.then.0
# 8 b = temp_a + 3
#           Var(%3, i32) = Var(temp_a_v1, i32) add Const(3, i32)
#           Var(b_v2, i32) = Var(%3, i32)
#           JMP -> if.end.0 (Line 7)
# LABEL if.else.0
# LABEL if.end.0
#           Var(b_v3, i32) = PHI(b)[func.2]
# 26    return b
#           RET Var(b_v3, i32)

# Control flow for this simple function:

# | * BLOCK START 0
# | * Predecessors: None
# | * Successors: [1, 2]
# | * In: b_v0. temp_a_v1
# | * Out: b_v1, temp_a_v1
# | # 3 temp_a = Number()
# | Var(temp_a_v1, i32) = Const(0, i32)
# | # 5 b += 1
# | Var(%0, i32) = Var(b_v0, i32) add Const(1, i32)
# | Var(b_v1, i32) = Var(%0, i32)
# | # 7 if temp_a > 0:
# | Var(%1, i32) = Var(temp_a_v1, i32) gt Const(0, i32)
# | Var(%2, i32) = Var(%1, i32)
# | BR Z Var(%2, i32) -> if.else.0 (Line 0) # jumps to 2 (if.else.0), falls through to 1 (if.then.0) (2 targets)

#   | * BLOCK START 1
#   | * Predecessors: [0]
#   | * Successors: [3]
#   | * In: b_v1. temp_a_v1
#   | * Out: b_v2
#   | LABEL if.then.0
#   | # 8 b = temp_a + 3
#   | Var(%3, i32) = Var(temp_a_v1, i32) add Const(3, i32)
#   | Var(b_v2, i32) = Var(%3, i32)
#   | JMP -> if.end.0 # jumps to if.end.0 (only one target)

#   | * BLOCK START 2
#   | * Predecessors: [0]
#   | * Successors: [3]
#   | * In: 
#   | * Out: 
#   | LABEL if.else.0

# | * BLOCK START 3
# | * Predecessors: [1, 2]
# | * Successors: None
# | * In: b_v1. b_v2, temp_a_v1
# | * Out: b_v3, temp_a_v1
# | LABEL if.end.0
# | Var(b_v3, i32) = PHI(b)[b_v1, b_v2]
# | # 26 return b
# | RET Var(b_v3, i32)




# After phi is resolved:

# | * BLOCK START
# | * In: b_v0. temp_a_v1
# | * Out: b_v1, temp_a_v1
# | # 3 temp_a = Number()
# | Var(temp_a_v1, i32) = Const(0, i32)
# | # 5 b += 1
# | Var(%0, i32) = Var(b_v0, i32) add Const(1, i32)
# | Var(b_v1, i32) = Var(%0, i32)
# | # 7 if temp_a > 0:
# | Var(%1, i32) = Var(temp_a_v1, i32) gt Const(0, i32)
# | Var(%2, i32) = Var(%1, i32)
# | BR Z Var(%2, i32) -> if.else.0 (Line 0)

#   | * BLOCK START
#   | * In: b_v1. temp_a_v1
#   | * Out: b_v2
#   | LABEL if.then.0
#   | # 8 b = temp_a + 3
#   | Var(%3, i32) = Var(temp_a_v1, i32) add Const(3, i32)
#   | Var(b_v2, i32) = Var(%3, i32)
#   | Var(b_v3, i32) = Var(b_v2, i32)
#   | JMP -> if.end.0

#   | * BLOCK START
#   | * In: 
#   | * Out: 
#   | LABEL if.else.0
#   | Var(b_v3, i32) = Var(b_v1, i32)

# | * BLOCK START
# | * In: b_v1. b_v2, temp_a_v1
# | * Out: b_v3, temp_a_v1
# | LABEL if.end.0
# | # 26 return b
# | RET Var(b_v3, i32)








# def func(b: Number):
#   temp_a = Number()

#   b += 1

#   if temp_a > 0:
#       b = temp_a + 3
#       # temp_a = 1
        
#   else:
#       # b += 2
#       # temp_a += 1
#       pass
#       # if temp_a == 2:
#           # b = 4 + b


#   # if temp_a > 5:
#   #   b = 3 + b
        
#   # else:
#   #   # b = 1 + 2
#   #   temp_a = 5

#   return b


# def func2(b: Number):
#   temp_a = Number() # temp_a_v1

#   b += 1  # b_v1 = b_v0 + 1

#   temp_a = b + 2 # temp_a_v2 = b_v1 + 2

#   if temp_a > 0: # temp_v2
#       b = temp_a + 3 # b_v2 = temp_a_v2 + 3

#       if b < 0: # b_v2
#           b = 1 # b_v3
#           b = 2 # b_v4

#           temp_a = 6 # temp_a_v3

#           b = b + temp_a # b_v5 = b_v4 + temp_a_v3
#           # should reduce to 2 + 6

#       # elif b == 1:
#       #   b = 2

#       else: # equivalent to elif
#           if b == 1: # b_v2
#               b = 2 # b_v6

#           # phi(b_v6)

#       # phi(b_v5, b_v6)

#   else:
#       b = 4 + b

#   temp_a = b

#   return temp_a



# global_a = Number()
# global_b = Number()

# def func1():
#     # global_a += 1
#     global_a = global_a + 1
    

# def func2(b: Number):
#   temp_a = Number()

#   b += 2
    
#   temp_a = global_a + 2 + b

#   b = temp_a + 3

#   temp_a = b

#   return temp_a

# compile error here!


# def func3():
#   for i in 4:
#       func1()

#       global_b = func2(i)


#   if 1 == 1:
#       global_b += 1

#   else:
#       global_a += 1