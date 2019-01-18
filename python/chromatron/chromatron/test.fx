# a = Number(publish=True)
# b = Number(publish=True)
# c = Number(publish=True)
d = Number(publish=True)
# e = Number(publish=True)

def test(_a, _b):
    return _a + _b

def init():
    # a += 1 + 1
    # b -= a - 4

    # c += test(1, 2)
    d += test(1, 2) + 5
    # e += test(1, 2) + test(3, 4)
