

# def func():
#     pixels[1].hue = 2
    

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

# # # a = Number()[4]
# # # def obj(a: Number):
# # a = Number()
# def obj():
    
#     b = Number()
# #     # b += 1

# #     # if True:
# #         # a = Number()
# #         # a = 4

# #     a += 4


# #     return a

#     s2 = String()
#     s2 = s

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