
def init():
    pass

def loop():
    pixels.sat = 0.0
    pixels.val = 0.0
    pixels[0].val = 1.0
    pixels[pixels.count - 1].val = 1.0
