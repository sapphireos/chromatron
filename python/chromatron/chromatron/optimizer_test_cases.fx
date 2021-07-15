
def loop_invariant_code_motion_basic():
	i = Number()
	i = 4

	a = Number()

	while i > 0:
		i -= 1

		a = 2 + 3

	return a

def loop_invariant_code_motion2():
	i = Number()
	i = 4

	while i > 0:
		i -= 1

		if i == 1:
			i = 2

	return 0
