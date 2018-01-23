
a = Number(publish=True)

def init():
    db.kv_test_key = 123
    a = 2
    a += db.kv_test_key + 1