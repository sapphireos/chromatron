top = PixelArray(0, 48)
bottom = PixelArray(48, 48, reverse=True, mirror=top)

hue = Fixed16()
cursor = Number()

def init():
    pixels.val = 0.0
    pixels.sat = 1.0
    
    hue = 0.667
    pixels.hue = hue

    pixels.v_fade = 10
    pixels.hs_fade = 100

    db.gfx_frame_rate = 30

    start_thread(warp)

def warp():
    while True:
        delay(40)

        pixels[cursor].v_fade = 3000
        pixels[cursor].val = 0.0

        cursor += 1

        if cursor >= top.count:
            cursor = 0
            hue = rand()

        if cursor == pixels.size_x:
            delay(90)

        pixels[cursor].v_fade = 10
        pixels[cursor].val = 1.0
        pixels[cursor].hue = hue

def loop():
    pass