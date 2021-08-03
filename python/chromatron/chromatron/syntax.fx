def empty_func():
    pass

def simple_func():
    return 0

def simple_expr():
    return 1 + 2

def simple_var():
    a = Number()

    return a

def simple_binop():
    a = Number()
    b = Number()

    b = 1

    a = b + 2

    return a

def simple_ifelse():
    a = Number()
    b = Number()

    if a > 0:
        b = 1

    else:
        b = 2

    return b

def two_ifelse():
    a = Number()
    b = Number()

    if a > 0:
        b = 1

    else:
        b = 2

    if b < 0:
        a = 1

    else:
        b = 2

    return b

def while_loop():
    i = Number()
    i = 4

    while i > 0:
        i -= 1

def double_while_loop():
    i = Number()
    i = 4

    while i > 0:
        i -= 1
        j = Number()

        j = 4

        while j > 0:
            j -= 1

def while_with_if_loop():
    i = Number()
    i = 4
    a = Number()
    b = Number()

    while i > 0:
        i -= 1

        if a < b:
            i = 0


def while_while_expr():
    a = Number()
    b = Number()

    while a > 0:
        while a > 0:
            b += 1

    return b

def another_while_if():
    a = Number()

    while a > 0:
        if True:
            pass

def if_if_expr():
    a = Number()
    
    if a > 0:
        if a > 0:
            a += 1

    return a

def while_if_expr():
    a = Number()
    
    while a > 0:
        if a > 0:
            a += 1

    return a

def if_while_expr():
    a = Number()
    b = Number()

    if a > 0:
        while a > 0:
            b += a

    return b
        
def while_if_while():
    a = Number()

    while a > 0:
        if a > 0:
            while a > 0:
                pass
        

def while_if_while_expr():
    a = Number()
    b = Number()

    while a > 0:
        if a > 0:
            while a > 0:
                b += a

    return b
        

# global_a = Number()
# def global_var():
#   global_a += 1