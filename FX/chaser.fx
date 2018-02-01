# chaser.fx

# declare a global variable for current hue
current_hue = Number()

# declare another variable for the cursor
cursor = Number()

# init - runs once when script is loaded
def init():
    # set pixels to full colors (maximum saturation)
    pixels.sat = 1.0

    # set pixels default hue to red
    pixels.hue = 0.0

    # set to off
    pixels.val = 0.0

    # set color fades to 500 milliseconds
    pixels.hs_fade = 500

    # override frame rate setting
    db.gfx_frame_rate = 200

# runs periodically, frame rate is configurable
def loop():
    # turn off the previous pixel with a slow fade
    pixels[cursor - 1].v_fade = 1000
    pixels[cursor - 1].val = 0.0

    # turn on pixels at cursor, with a 200 ms fade
    pixels[cursor].v_fade = 100
    pixels[cursor].val = 1.0

    # change color of pixel
    pixels[cursor].hue = current_hue

    # increment cursor
    cursor += 1

    # adjust color
    current_hue += 0.005

