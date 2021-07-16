# def loop_invariant_code_motion2():
# 	i = Number()
# 	i = 4

# 	a = Number()

# 	while i > 0:
# 		i -= 1

# 		if i == 1:
# 			i = 2

# 		a = 2 + 3

# 	return a


# global_a = Number()
# def global_var():
# 	global_a += 1

# 	return global_a

# def while_loop():
# 	i = Number()
# 	i = 4

# 	while i > 0:
# 		i -= 1

# 	return i

# global_b = Number()
# def constant_folding():
# 	a = Number()

# 	# a = 1 + 2

# 	# a += 1

# 	return a - 0

# 	# return a

# 	a = 3 + 4 + 5

# 	return a


# 	a = 6 * global_b - 7

	# global_b = 0
	# a = 1 - a

# global_a = Fixed16()
# global_b = Number()

# def simple_ifelse():
# 	a = Number()
# 	b = Number()

# 	if a > 0:
# 		b = 1

# 	else:
# 		b = 2

# 	return b


def while_loop():
	i = Number()
	i = 4

	# if i == 1:
		# return 2

	a = Number()

	while i > 0 and a == 2:
		i -= 1

		# a = 2 + i

		# a = 3 + 4

		# a = Number()
		# a = 2 + i + 4
		# # a = 4

		# while a > 0:
		# 	a -= 1
		# 	if a == 0:
		# 		i -= 2
		# 	else:
		# 		i = 1

		# if i == 1:
			# i = 2

		# else:
			# i += 3

		# while a == 2:
		# 	a += 1
		# 	i -= 3
			# i = a + 2 + 3

		# i += 1

	return a

# def simple_array():
# 	a = Number(count=4)
# 	# pass

# 	b = Number()

# 	# a = b[0] + a

# 	# a[0][1].h = b
# 	global_a += 1

# 	global_b = 3


# 	global_a = 1 + 2

# 	global_a = a + b

# 	return 2
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
	# 	return 2

	# return b + 4


# def func():
# 	b = Number() # defines b0

# 	# return 0

# # 	b += 1 # uses b0 and defines b1

# 	if b > 0: # uses B1
# # 		b = b + 4 # defines b2

# 		return b

# 	else:
# # # 		# b = 3 # defines b3
# 		b += 3
# # # # 		# pass

# # # 		if b > 2:
# # # 			b = 0

# 	return b # uses b
# # 	# need to define b4 and use b4 here

# def func():
# 	b = Number()
# 	i = Number()
# 	i = 4

# 	while i > 0:
# 		i -= 1

# 		b += i

# 		if i < 2:
# 			i = 3
		

# 	if b == 0:
# 		b = 2

# 	return b



# # def func(b: Number):
# def func():
# 	b = Number() 
# 	temp_a = Number()

# 	b += 1

# 	# b = 1.0 + 2
# 	# temp_b = Number()

# 	if temp_a > 0:
# 		# temp_b = Fixed16()
# 		# temp_b = 1.0 + b
# 		b = temp_a + 3
		
# 	else:
# 		if temp_a > 0:
# 			# temp_a += 1
# 			b = 1

# 		# temp_b = Fixed16()
		
# 		# temp_a += 2
# 		# b += 3

# 	# temp_b += 1
		
# 	return b


# ********************************
# Func code:
# ********************************
# 3	temp_a = Number()
# 			Var(temp_a_v1, i32) = Const(0, i32)
# 5	b += 1
# 			Var(%0, i32) = Var(b_v0, i32) add Const(1, i32)
# 			Var(b_v1, i32) = Var(%0, i32)
# 7	if temp_a > 0:
# 			Var(%1, i32) = Var(temp_a_v1, i32) gt Const(0, i32)
# 			Var(%2, i32) = Var(%1, i32)
# 			BR Z Var(%2, i32) -> if.else.0 (Line 0)
# LABEL if.then.0
# 8	b = temp_a + 3
# 			Var(%3, i32) = Var(temp_a_v1, i32) add Const(3, i32)
# 			Var(b_v2, i32) = Var(%3, i32)
# 			JMP -> if.end.0 (Line 7)
# LABEL if.else.0
# LABEL if.end.0
# 			Var(b_v3, i32) = PHI(b)[func.2]
# 26	return b
# 			RET Var(b_v3, i32)

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

# 	| * BLOCK START 1
# 	| * Predecessors: [0]
# 	| * Successors: [3]
# 	| * In: b_v1. temp_a_v1
# 	| * Out: b_v2
# 	| LABEL if.then.0
# 	| # 8 b = temp_a + 3
# 	| Var(%3, i32) = Var(temp_a_v1, i32) add Const(3, i32)
# 	| Var(b_v2, i32) = Var(%3, i32)
# 	| JMP -> if.end.0 # jumps to if.end.0 (only one target)

# 	| * BLOCK START 2
# 	| * Predecessors: [0]
# 	| * Successors: [3]
# 	| * In: 
# 	| * Out: 
# 	| LABEL if.else.0

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

# 	| * BLOCK START
# 	| * In: b_v1. temp_a_v1
# 	| * Out: b_v2
# 	| LABEL if.then.0
# 	| # 8 b = temp_a + 3
# 	| Var(%3, i32) = Var(temp_a_v1, i32) add Const(3, i32)
# 	| Var(b_v2, i32) = Var(%3, i32)
# 	| Var(b_v3, i32) = Var(b_v2, i32)
# 	| JMP -> if.end.0

# 	| * BLOCK START
# 	| * In: 
# 	| * Out: 
# 	| LABEL if.else.0
# 	| Var(b_v3, i32) = Var(b_v1, i32)

# | * BLOCK START
# | * In: b_v1. b_v2, temp_a_v1
# | * Out: b_v3, temp_a_v1
# | LABEL if.end.0
# | # 26 return b
# | RET Var(b_v3, i32)








# def func(b: Number):
# 	temp_a = Number()

# 	b += 1

# 	if temp_a > 0:
# 		b = temp_a + 3
# 		# temp_a = 1
		
# 	else:
# 		# b += 2
# 		# temp_a += 1
# 		pass
# 		# if temp_a == 2:
# 			# b = 4 + b


# 	# if temp_a > 5:
# 	# 	b = 3 + b
		
# 	# else:
# 	# 	# b = 1 + 2
# 	# 	temp_a = 5

# 	return b


# def func2(b: Number):
# 	temp_a = Number() # temp_a_v1

# 	b += 1  # b_v1 = b_v0 + 1

# 	temp_a = b + 2 # temp_a_v2 = b_v1 + 2

# 	if temp_a > 0: # temp_v2
# 		b = temp_a + 3 # b_v2 = temp_a_v2 + 3

# 		if b < 0: # b_v2
# 			b = 1 # b_v3
# 			b = 2 # b_v4

# 			temp_a = 6 # temp_a_v3

# 			b = b + temp_a # b_v5 = b_v4 + temp_a_v3
# 			# should reduce to 2 + 6

# 		# elif b == 1:
# 		# 	b = 2

# 		else: # equivalent to elif
# 			if b == 1: # b_v2
# 				b = 2 # b_v6

# 			# phi(b_v6)

# 		# phi(b_v5, b_v6)

# 	else:
# 		b = 4 + b

# 	temp_a = b

# 	return temp_a



# global_a = Number()
# global_b = Number()

# def func1():
#     # global_a += 1
#     global_a = global_a + 1
    

# def func2(b: Number):
# 	temp_a = Number()

# 	b += 2
	
# 	temp_a = global_a + 2 + b

# 	b = temp_a + 3

# 	temp_a = b

# 	return temp_a

# compile error here!


# def func3():
# 	for i in 4:
# 		func1()

# 		global_b = func2(i)


# 	if 1 == 1:
# 		global_b += 1

# 	else:
# 		global_a += 1