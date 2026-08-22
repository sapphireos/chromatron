
trigger_level = Number()

state = Number()
# 0 = idle
# 1 = PRE: bottom entry
# 2 = bottom entry
# 3 = PRE: top entry
# 4 = top entry
# 5 = PRE: bottom exit
# 6 = bottom exit
# 7 = PRE: top exit
# 8 = top exit

timeout_setting = Number()
dwell_setting = Number()
timeout = Number()


def init():
    pixels.val = 0.0
    pixels.hue = 0.0
    pixels.sat = 1.0
 
    trigger_level = 400
    timeout_setting = 10
    dwell_setting = 200

    start_thread(sparkle)
    
def entry_bottom():
    stop_thread(sparkle)
    cursor = Number()

    pixels.sat = 1.0
    pixels.hue = rand()
    pixels.v_fade = 10

    while cursor < pixels.count - 1:
        pixels[cursor].val = 1.0

        pixels[cursor].hs_fade = 0
        pixels[cursor].sat = 0.0

        cursor += 1

        if cursor % 2 == 0:
            delay(15)

        pixels.sat += 0.1

    pixels.sat = 1.0
    
def entry_top():
    stop_thread(sparkle)
    cursor = Number()
    cursor = pixels.count - 1

    pixels.sat = 1.0
    pixels.hue = rand()
    pixels.v_fade = 10

    while cursor > 0:
        pixels[cursor].val = 1.0

        pixels[cursor].hs_fade = 0
        pixels[cursor].sat = 0.0

        cursor -= 1

        if cursor % 2 == 0:
            delay(15)

        pixels.sat += 0.1

    pixels.sat = 1.0
    
def exit_bottom():
    state = 255

def exit_top():
    state = 255

def loop():
    # return
    # IDLE
    if state == 0:
        pixels.hs_fade = 2000

        timeout = timeout_setting
        # pixels.val = 0.0
        # pixels.sat = 1.0

        # entry from bottom
        if db.sensors_distance_0 > trigger_level and db.sensors_distance_1 < trigger_level:
            state = 1

        # entry from top
        elif db.sensors_distance_3 > trigger_level and db.sensors_distance_2 < trigger_level:   
            state = 3

        # exit from bottom
        elif db.sensors_distance_1 > trigger_level and db.sensors_distance_0 < trigger_level:
            state = 5

        # exit from top
        elif db.sensors_distance_2 > trigger_level and db.sensors_distance_3 < trigger_level:   
            state = 7

        
    # PRE bottom entry
    elif state == 1:
        
        timeout -= 1
        if timeout <= 0:
            state = 255
            return

        if db.sensors_distance_1 > trigger_level:
            state = 2
            start_thread(entry_bottom)
            timeout = dwell_setting

    # BOTTOM entry
    elif state == 2:
        timeout -= 1
        if timeout <= 0:
            state = 255
            return

    # PRE top entry
    elif state == 3:
        
        timeout -= 1
        if timeout <= 0:
            state = 255
            return

        if db.sensors_distance_2 > trigger_level:
            state = 4
            start_thread(entry_top)
            timeout = dwell_setting

    # TOP entry
    elif state == 4:
        timeout -= 1
        if timeout <= 0:
            state = 255
            return

    # PRE bottom exit
    elif state == 5:
        timeout -= 1
        if timeout <= 0:
            state = 255
            return

        if db.sensors_distance_0 > trigger_level:
            state = 6
            start_thread(exit_bottom)

    # BOTTOM exit
    elif state == 6:
        pass

    # PRE top exit
    elif state == 7:
        timeout -= 1
        if timeout <= 0:
            state = 255
            return

        if db.sensors_distance_3 > trigger_level:
            state = 8
            start_thread(exit_top)

    # BOTTOM exit
    elif state == 8:
        pass

    else:
        pixels.v_fade = 2000
        pixels.val = 0.0
        state = 0

        if not thread_running(sparkle):
            start_thread(sparkle)

def sparkle():
    cursor = Number()

    while True:
        delay(100)

        pixels.val -= 0.025
        temp = Number()
        temp = noise(cursor)
        pixels[temp].v_fade = 1000
        pixels[temp].val = 1.0
        cursor += 4

        pixels.hue -= 0.002
