
def func(_a, _b):
    if _a > _b + 1 + 2:
        return 0

    return _a + _b


def init():
    a = Number()
    
    if a > 1 + 2:
        a += 1

    if a > 3 + 4:
        a -= 1
    
    a = func(1, 2)

    if a > 3 + 4 + func(1, 2):
        a = 2

def loop():
    pass