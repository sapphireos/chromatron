

pixel1 = PixelArray(0, 8)
pixel2 = PixelArray(8, 8)

fx_send_var = Number(publish=True)
fx_vm_lib_test = Number(publish=True)

fx_thread1 = Number(publish=True)
fx_thread2 = Number(publish=True)

send('kv_test_key', 'fx_send_var', ['test'], tag='meow')


def thread1():
    while True:
        delay(1000)
        fx_thread1 += 1        

def thread2():
    while True:
        delay(2000)
        fx_thread2 += 1        

def init():
    pixels.sat = 1.0
    pixels.val = 1.0

    pixel1.hue = 0.333
    pixel2.hue = 0.667

    fx_send_var = 123

    fx_vm_lib_test = test_lib_call(1, 2)

    db.kv_test_key = 456

    start_thread(thread1)
    start_thread(thread2)


def loop():
    pixel1.hue += 0.001
    pixel2.hue += 0.001

