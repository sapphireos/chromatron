global_a = Number()
global_b = Number()

def func1():
    global_a += 1
    

def func2(b: Number):
	temp_a = Number()
	temp_a = global_a + 2 + b

	return temp_a

def func3():
	for i in 4:
		func1()

		global_b = func2(i)

