
a = Number(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Number(publish=True)

ary = Array(4, type=Fixed16)
ary2 = Array(4)

def init():
    ary = 3.123

    a = ary[1]
    b = ary[1]

    ary2 = 3.123    
    c = ary2[1]

    ary2 += 3.123
    d = ary2[1]
