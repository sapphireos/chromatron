a = Number(publish=True)
b = Fixed16(publish=True)
c = Fixed16(publish=True)
d = Fixed16(publish=True)
e = Fixed16(publish=True)
f = Fixed16(publish=True)
g = Fixed16(publish=True)
h = Fixed16(publish=True)
i = Fixed16(publish=True)

ary = Array(4)
ary1 = Array(4, type=Fixed16)

def init():
    # pixels.hue = 123
    # db.kv_test_key = pixels[0].hue
    # a = db.kv_test_key

    # pixels[0].val = db.kv_test_key
    # b = pixels[0].val

    # ary[0] = db.kv_test_key
    # c = ary[0]

    ary[1] = 456
    db.kv_test_key = ary[1]
    d = db.kv_test_key

    # ary1[1] = pixels[0].val
    # e = ary1[1]

    # ary1[2] = 0.123
    # pixels[0].val = ary1[2]
    # f = pixels[0].val

    # pixels[1].val = pixels[0].val
    # g = pixels[1].val

    # pixels[0].val = 0.333
    # db.kv_test_array[0] = pixels[0].val
    # h = db.kv_test_array[0]

    # db.kv_test_array[0] = 456
    # pixels[0].val = db.kv_test_array[0]
    # i = pixels[0].val



