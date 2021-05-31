

def func2(b: Number):
	temp_a = Number()

	b += 1

	temp_a = b + 2

	if temp_a > 0:
		b = temp_a + 3

	else:
		b = 4 + b

	temp_a = b

	return temp_a



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