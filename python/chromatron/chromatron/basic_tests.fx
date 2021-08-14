

def arith():
	assert 1 + 2 == 3
	assert 3 - 2 == 1
	assert 3 * 2 == 6
	assert 8 / 4 == 2
	assert 8 % 5 == 3

def constant_folding():
	assert 1 + 2 - 3 * 10 / 5 == -3

NAMED_CONST = 4
def named_const():
	assert NAMED_CONST == 4
	assert NAMED_CONST + 1 == 5

	a = Number()

	a = NAMED_CONST * 3
	assert a == 12


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
  