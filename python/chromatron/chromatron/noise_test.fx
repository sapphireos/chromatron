
a = Number()
current_hue = Number(publish=True)
scale = Number()

meow = Number(publish=True)

def init():
    # pass
    # pixels.v_fade = 1
    pixels.val = 1.0

    pixels.hs_fade = 200
    pixels.v_fade = 200

    # b = Number()
    # b = 10

    # for i in pix_count():
    #     pixels[i].hs_fade = b
    #     pixels[i].v_fade = b

    #     b += 1000

    # scale = 0.05
    scale = 0.005

    meow = 123


def loop():
    current_hue += 0.2
    # a += 0.02

    for i in pixels.count:
        
        n = Number()
        n = noise(a + i * scale)
        # sin(a)

        # pixels[i].hue = n

        pixels[i].sat = 1.0
        pixels[i].val = n

        # a += 1.0 / pix_count()
        a += 5

