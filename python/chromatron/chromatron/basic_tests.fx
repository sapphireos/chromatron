

def arith():
	assert 1 + 2 == 3
	assert 3 - 2 == 1
	assert 3 * 2 == 6
	assert 8 / 4 == 2
	assert 8 % 5 == 3


NAMED_CONST = 4
def named_const():
	assert NAMED_CONST == 4
	assert NAMED_CONST + 1 == 5

	a = Number()

	a = NAMED_CONST * 3
	assert a == 12