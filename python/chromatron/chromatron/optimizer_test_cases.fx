
def loop_invariant_code_motion_basic():
	i = Number()
	i = 4

	a = Number()

	while i > 0:
		i -= 1

		a = 2 + 3

	return a

def loop_invariant_code_motion_ifelse():
	i = Number()
	i = 4

	a = Number()

	while i > 0:
		i -= 1

		if i == 10:
			a = 2 + 3


	# a should be 0 here, unless i inits to greater than 11

	return a


def loop_invariant_code_motion_ifbreak():
	i = Number()
	i = 4

	a = Number()

	while i > 0:
		i -= 1

		if i == 10:
			break

		a = 2 + 3

	# a should be 5 here unless i inits to 11

	return a



def loop_invariant_code_motion_induction():
	i = Number()
	i = 4

	while i > 0:
		i = 2

	return i
