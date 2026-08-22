
def startup():
    pixels.sat = 1.0
    pixels.val = 0.2
    pixels.hue = 0.0
    delay(1000) 

    pixels.hue = 0.333
    delay(1000) 

    pixels.hue = 0.667
    delay(1000) 

    pixels.sat = 0.0
    delay(1000) 
    
    pixels.v_fade = 100
    pixels.val = 0.0

    delay(1000)

    pixels.sat = 1.0
    pixels.v_fade = 500
    pixels.hs_fade = 50
    start_thread(main)


send('gfx_enable', 'swipe_state', ['quadrant'], 100)

MIN_DISTANCE = Number()
MAX_DISTANCE = Number()

MIN_DIMMER = Number()
MAX_DIMMER = Number()

MIN_SWIPE_DISTANCE = Number()

def init():
    MIN_DISTANCE = 100
    MAX_DISTANCE = 300

    MIN_DIMMER = 5000
    MAX_DIMMER = 20000

    MIN_SWIPE_DISTANCE = 50

    pixels.val = 0.0
    pixels.sat = 0.0
    pixels.hue = 0.0
    pixels.hs_fade = 100

    # prox_setting = 4000
    # dimmer_setting = 4000

    db.ssd1306_dimmer = 255

    start_thread(startup)
    # start_thread(main)


raw_distance = Number()
filtered_distance = Number()[8]
filtered_distance_index = Number()
distance = Number(publish=True)
control_distance = Number(publish=True)

on_pulse_width = Number()
swipe = Number(publish=True)
swipe_timeout = Number()
swipe_state = Number(publish=True)

lcd_triggered_counter = Number()


def one_second():
    while True:
        delay(1000)

        if lcd_triggered_counter > 0:
            lcd_triggered_counter -=1


def swipe_graphic():
    pixels.v_fade = 500

    if swipe_state == True:
        pixels.sat = 0.0

    else:
        pixels.sat = 1.0

    pixels.val = 0.5
    pixels.hue = 0.0

    delay(1000)

    pixels.v_fade = 1000
    pixels.val = 0.0


def set_dimmer_level_from_als():
    db.ssd1306_dimmer = ( db.veml7700_als - MIN_DIMMER ) * 255 / ( MAX_DIMMER - MIN_DIMMER )


def main():  

    start_thread(one_second)

    while True:
        raw_distance = db.vl53l0x_distance

        filtered_distance[filtered_distance_index] = raw_distance
        filtered_distance_index += 1

        distance = avg(filtered_distance)

        if distance > MAX_DISTANCE:
            distance = MIN_DISTANCE

        elif distance < MIN_DISTANCE:
            distance = MIN_DISTANCE

        control_distance = ( distance - MIN_DISTANCE ) * 65535 / ( MAX_DISTANCE - MIN_DISTANCE )

        if control_distance > 0:
            lcd_triggered_counter = 5        


        if raw_distance > MAX_DISTANCE:
            raw_distance = 0

        elif raw_distance < MIN_SWIPE_DISTANCE:
            raw_distance = 0        

        if raw_distance > 0:
            if on_pulse_width < 200:
                on_pulse_width += 1
                # db.ssd1306_debug = on_pulse_width

        else:
            # if on_pulse_width > 0:
                # db.ssd1306_debug = on_pulse_width

            on_pulse_width = 0


        if swipe_timeout > 0:
            swipe_timeout -= 1

            if swipe_timeout == 0:
                swipe = False


        if on_pulse_width > 5:
            if swipe == False:
                swipe = True

                start_thread(swipe_graphic)

                if swipe_state == True:
                    swipe_state = False

                else:
                    swipe_state = True

            swipe_timeout = 20

        db.ssd1306_debug = swipe_state

        # db.ssd1306_debug = raw_distance
        # db.ssd1306_debug = control_distance * 100 / 65535

        

        if db.veml7700_als < MIN_DIMMER and control_distance > 0:
            set_dimmer_level_from_als()

        elif db.veml7700_als > MAX_DIMMER:
            db.ssd1306_dimmer = 255

        elif lcd_triggered_counter > 0:
            set_dimmer_level_from_als()

        elif db.veml7700_als < MIN_DIMMER:
            db.ssd1306_dimmer = 0

        else:
            set_dimmer_level_from_als()

        delay(25)

