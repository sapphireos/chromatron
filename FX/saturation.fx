
# init - runs once when script is loaded
def init():
    # set pixels to full colors (maximum saturation)
    pixels.sat = 1.0

    # set to maximum brightness
    pixels.val = 1.0

    # set hue on all pixels
    pixels.hue = 0.667

    # declare a local variable
    a = Number()

    # loop over all pixels in array
    for i in pixels.count:
        pixels[i].sat = a
        
        # shift color for next pixel.
        # this will distribute the rainbow pattern
        # across the entire array.
        a += 1.0 / (pixels.count - 1)
