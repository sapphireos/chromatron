
receive('clock', 'sys_time', ['ir3'])
clock = Number(publish=True)


def init():
    pixels.sat = 0.0


def loop():
    pixels.v_fade = 500
    pixels.val = 0.0


    pixels[db.sys_time/100].v_fade = 100
    pixels[db.sys_time/100].val = 1.0

    # pixels.hue = 0.0
    # pixels.sat = 0.0
    # pixels.val = 0.0
    # pixels[0].val = 0.5

    # pixels[1][1].val = 0.5

    # pixels[1].sat = 1.0

    # pixels[pixels.count - 1].val = 1.0


