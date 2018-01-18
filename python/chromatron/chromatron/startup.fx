
cursor = Number()

def init(): 
    pixels.val = 0.0
    pixels.sat = 0.0

def loop():
    pixels.val = 0.0
    pixels[cursor].val = 1.0

    cursor += 1

    if cursor >= pix_count():
        halt()

