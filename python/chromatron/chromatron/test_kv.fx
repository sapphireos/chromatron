
a = Number(publish=True)

def init():
    pass

def loop():
    # a = db.kv_test_key

    a += 1

    # db.kv_test_key = a

    db.kv_test_key += 1
    # db.kv_test_key = db.pix_count