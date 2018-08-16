# this script generates rolling rainbow pattern

# declare a global variable for current hue
current_hue = Fixed16()

# init - runs once when script is loaded
def init():
    # set pixels to full colors (maximum saturation)
    pixels.sat = 1.0

    # set to maximum brightness
    pixels.val = 1.0


# runs periodically, frame rate is configurable
def loop():
    # increment the base hue so the rainbow pattern
    # shifts across the pixels
    current_hue += 0.005

    # declare a local variable
    a = Fixed16()
    a = current_hue

    b = Fixed16()
    b = 1.0 / pixels.count

    # loop over all pixels in array
    for i in pixels.count:
        pixels[i].hue = a
        
        # shift color for next iteration
        a += b
