a = Number(publish=True)
b = Number(publish=True)

def init():
    db.kv_test_key = 123
    
    a = db.kv_test_key + 1
    b = db.kv_test_key + db.kv_test_key
