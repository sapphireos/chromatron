
p1 = PixelArray(5, 9, reverse=True)

# declare a global variable for current hue
current_hue = Number()

# init - runs once when script is loaded
def init():
    pixels.sat = 1.0
    pixels.val = 1.0
    pixels.hue = 0.0

    # p1.sat = 1.0
    # p1.val = 1.0
    p1.hue = 0.333

    pass

# runs periodically, frame rate is configurable
def loop():

    cursor = Number()


    p1[cursor].hue = current_hue


    cursor += 1

    # increment the base hue so the rainbow pattern
    # shifts across the pixels
    if cursor >= p1.count:
        current_hue += 0.3
        cursor = 0



    # declare a local variable
    a = Number()
    a = 0.0
    # a = current_hue

    # p1.hue += 0.01

    # a = pixels.count

    # # loop over all pixels in array
    # for i in p1.count:
    # # for i in pix_count():
        # p1[i].hue = 0.667
    #     p1[i].hue = a
    #     # pixels[i].sat = 1.0
        
    #     a += 1.0 / pix_count()

    # # p1[a].hue = 0.6667

