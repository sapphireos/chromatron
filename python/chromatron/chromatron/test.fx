
def func(b: Number):
	temp_a = Number()

	b += 1

	if temp_a > 0:
		b = temp_a + 3
		# temp_a = 1
		
	else:
		# b += 2
		# temp_a += 1
		pass
		# if temp_a == 2:
			# b = 4 + b


	# if temp_a > 5:
	# 	b = 3 + b
		
	# else:
	# 	# b = 1 + 2
	# 	temp_a = 5

	return b


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