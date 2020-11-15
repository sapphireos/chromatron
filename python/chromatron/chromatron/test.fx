# honeycomb.fx


array1 = PixelArray(0, 4, reverse=False)
array2 = PixelArray(4, 4, reverse=True)


# declare a global variable for current hue
current_hue = Fixed16()

# init - runs once when script is loaded
def init():
    # set pixels to full colors (maximum saturation)
    pixels.sat = 1.0

    # set to maximum brightness
    pixels.val = 1.0

    bright()


# runs periodically, frame rate is configurable
def loop():
    # increment the base hue so the rainbow pattern
    # shifts across the pixels as we go from one frame
    # to the next.
    current_hue += 0.003

    # declare a local variable
    a = Fixed16()
    a = current_hue

    # loop over all pixels in array
    for i in pixels.count:
        array1[i].hue = a
        array2[i].hue = a
        
        # shift color for next pixel.
        # this will distribute the rainbow pattern
        # across the entire array.
        a += 0.3 / array1.count


@schedule(hours=8, minutes=0, seconds=0)
def bright():
    db.gfx_sub_dimmer = 65535

@schedule(hours=0, minutes=0, seconds=0)
def dim():
    db.gfx_sub_dimmer = 16384

