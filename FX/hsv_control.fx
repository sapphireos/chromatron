# hsv_control.fx

# declare variables and publish to network
hue = Fixed16(publish=True)
sat = Fixed16(publish=True)
val = Fixed16(publish=True)

# init - runs once when script is loaded
def init():
    # set startup defaults
    hue = 0.0
    sat = 1.0
    val = 1.0

# runs periodically, frame rate is configurable
def loop():
    # continually apply network variables to pixel array
    pixels.val = val
    pixels.sat = sat
    pixels.hue = hue
