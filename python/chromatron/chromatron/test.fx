# chaser.fx

# declare a global variable for current hue
current_hue = Fixed16()

# declare another variable for the cursor
cursor = Number()

offset = Number()

frame = Number()

side1 = PixelArray(0, 800)
side1_0 = PixelArray(0, 200, reverse=True)
side1_1 = PixelArray(200, 200)
side1_2 = PixelArray(400, 200, reverse=True)
side1_3 = PixelArray(600, 200)

side2 = PixelArray(800, 800, reverse=False)
side2_0 = PixelArray(0, 200, reverse=True)
side2_1 = PixelArray(200, 200)
side2_2 = PixelArray(400, 200, reverse=True)
side2_3 = PixelArray(600, 200)

side3 = PixelArray(1600, 800)
side3_0 = PixelArray(0, 200, reverse=True)
side3_1 = PixelArray(200, 200)
side3_2 = PixelArray(400, 200, reverse=True)
side3_3 = PixelArray(600, 200)

side4 = PixelArray(2400, 800, reverse=False)
side4_0 = PixelArray(0, 200, reverse=True)
side4_1 = PixelArray(200, 200)
side4_2 = PixelArray(400, 200, reverse=True)
side4_3 = PixelArray(600, 200)

keystone = PixelArray(3200, 284)

pinnacle = PixelArray(3484, 1)
# pinnacle = PixelArray(0, 1)

sat_thingy = Fixed16()
increment = Number()

state = Number()

# init - runs once when script is loaded
def init():
    # set pixels to full colors (maximum saturation)
    pixels.sat = 1.0

    pixels.hue = rand()

    pixels.val = 1.0

    pixels.hs_fade = 100

    pixels.v_fade = 100

    # override frame rate setting
    db.gfx_frame_rate = 10

    # side1.hue = 0.0
    # side2.hue = 0.333
    # side3.hue = 0.667
    # side4.hue = 0.8
    # side4.sat = 0.0

    # keystone.hue = 0.5

    offset = 30

    pinnacle.val = 1.0
    # pinnacle.hue = 0.667
    # pinnacle.sat = 1.0

    sat_thingy = 0.03

    increment = 1

    state = 0
    # pixels.val = 0.0
    # keystone.val = 1.0

    cursor = 0
    frame = 0

# runs periodically, frame rate is configurable
def loop():
    frame += 1

    if state == 0:
        for i in increment:
            side1.sat += sat_thingy
            side2.sat += sat_thingy
            side3.sat += sat_thingy
            side4.sat += sat_thingy

            side1_0[cursor].sat = 0.0
            side1_1[cursor].sat = 0.0
            side1_2[cursor].sat = 0.0
            side1_3[cursor].sat = 0.0

            side2_0[cursor].sat = 0.0
            side2_1[cursor].sat = 0.0
            side2_2[cursor].sat = 0.0
            side2_3[cursor].sat = 0.0

            side3_0[cursor].sat = 0.0
            side3_1[cursor].sat = 0.0
            side3_2[cursor].sat = 0.0
            side3_3[cursor].sat = 0.0

            side4_0[cursor].sat = 0.0
            side4_1[cursor].sat = 0.0
            side4_2[cursor].sat = 0.0
            side4_3[cursor].sat = 0.0

            pinnacle.hue += 0.002

            keystone[cursor].hue = current_hue

            side1[cursor].hue = current_hue
            side2[cursor].hue = current_hue
            side3[cursor].hue = current_hue
            side4[cursor].hue = current_hue

            # increment cursor
            cursor += 1

        # adjust color
        current_hue += 0.0051

        if frame % 1200 == 0:
            increment += 1

            if increment > 6:
                increment = 0

                pixels.sat = 0.0

                frame = 0
                state = 1


    elif state == 1:
        if frame > 600:
            state = 2
            frame = 0
            cursor = 0

            # pixels.val = 0.0
            

    elif state == 2:
        side1_0[cursor].val = 0.0
        side1_1[cursor].val = 0.0
        side1_2[cursor].val = 0.0
        side1_3[cursor].val = 0.0

        side2_0[cursor].val = 0.0
        side2_1[cursor].val = 0.0
        side2_2[cursor].val = 0.0
        side2_3[cursor].val = 0.0

        side3_0[cursor].val = 0.0
        side3_1[cursor].val = 0.0
        side3_2[cursor].val = 0.0
        side3_3[cursor].val = 0.0

        side4_0[cursor].val = 0.0
        side4_1[cursor].val = 0.0
        side4_2[cursor].val = 0.0
        side4_3[cursor].val = 0.0

        cursor += 1

        if cursor > 300:
            state = 3
            frame = 0


    elif state == 3:
        pixels.v_fade = 4000
        pixels.val = 0.0
        
        if frame > 300:
            state = 4
            frame = 0
            cursor = 0

    elif state == 4:
        pixels[rand()].val = 1.0
        pixels[rand()].val = 0.0
        pinnacle.val = 1.0
        
        if frame > 1000:
            pixels.sat += 0.001

        if frame > 3000:
        # if frame > 400:
            state = 5
            frame = 0
            cursor = 0
            pixels.val = 0.0
            pinnacle.sat = 1.0


    elif state == 5:
        pinnacle.val += 0.01
        pinnacle.hue += 0.0001

        if frame > 2000:
        # if frame > 500:
            state = 6
            frame = 0
            cursor = 0
            # pinnacle.val = 0.0
            # keystone[frame].val = 1.0

    elif state == 6:
        if frame > 1000:
            state = 7
            frame = 0
            cursor = 0

    elif state == 7:
        side1[cursor].val = 1.0
        side2[cursor].val = 1.0
        side3[cursor].val = 1.0
        side4[cursor].val = 1.0

        cursor += 1

        if cursor > 800:
            state = 8
            frame = 0
            cursor = 0

    elif state == 8:
        if frame > 1000:
            state = 99
            frame = 0
            cursor = 0

    elif state == 99:
        init()





# def startup():
#     return 1

# def shutdown():
#     return 2


# def init():
#     if thread_running('startup') or thread_running('shutdown'):
#        return

