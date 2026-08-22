
# Jess & Ben's has 63 LEDs on the vase
# vase = PixelArray(0, 63, reverse=False)
# top = PixelArray(63, 12, reverse=False)

# the others all have 55
# no idea how that happened...
vase = PixelArray(0, 55, reverse=False)
top = PixelArray(55, 12, reverse=False)


button_down = Number()
program = Number()

current_hue = Fixed16()
cursor = Number()
cursor2 = Number()
target = Number()
direction = Number()
delay_count = Number()

is_charging = Number()


def prog0_init():
    pixels.val = 0.0
    pixels.sat = 0.9
    pixels.hue = 0.0
    pixels.v_fade = 1000
    pixels.hs_fade = 1000

    db.gfx_frame_rate = 20

    vase.val = 1.0
    top.val = 1.0

    a = Fixed16()
    a = 0.0

    for i in top.count:
        top[i].hue = a

        a += 0.3 / top.count

    a = 0.0 
    for i in vase.count:
        vase[i].hue = a

        a += 0.3 / vase.count

def prog0_loop():
    top.hue += 0.002
    vase.hue += 0.002


def prog1_init():
    pixels.val = 0.0
    pixels.sat = 1.0
    pixels.hue = 0.0
    pixels.v_fade = 500
    pixels.hs_fade = 100

    cursor = 0
    current_hue = rand()

    db.gfx_frame_rate = 40


def prog1_loop():
    for i in pixels.count:
        if pixels[i].is_v_fading == False:
            pixels[i].val -= 0.4

    pixels[cursor].val = 1.0
    pixels[cursor].hue = current_hue

    cursor += 1

    if cursor % pixels.count == 0:
        current_hue = rand(current_hue - 0.1, current_hue + 0.11)

def prog2_init():
    pixels.val = 0.0
    pixels.sat = 1.0
    pixels.hue = rand()
    pixels.v_fade = 500
    pixels.hs_fade = 10000

    db.gfx_frame_rate = 40


def prog2_loop():
    pixels.val -= 0.02


    pixels[rand()].val = 1.0

    if rand() < 100:
        pixels.hue = rand()


def prog3_init():
    pixels.val = 1.0
    pixels.sat = 1.0
    current_hue = rand()
    pixels.hue = current_hue
    pixels.v_fade = 500
    pixels.hs_fade = 100

    db.gfx_frame_rate = 40

    cursor = rand(0, vase.count)
    direction = rand(0, 2)
    target = cursor

    current_hue = rand()

    delay_count = 0

def prog3_loop():
    if delay_count > 0:
        delay_count -= 1
        return

    pixels[cursor].val = 1.0
    pixels[cursor].hue = current_hue

    if direction == 0:
        cursor += 1
    else:
        cursor -= 1

    if cursor < 0:
        cursor = pixels.count - 1

    if (cursor % pixels.count) == (target % pixels.count):
        cursor = rand(0, vase.count)
        direction = rand(0, 2)
        target = cursor
        current_hue = rand()

        delay_count = rand(0, 100)



def prog4_init():
    pixels.val = 1.0
    pixels.sat = 1.0
    pixels.hue = 0.0
    pixels.v_fade = 500
    pixels.hs_fade = 250

    db.gfx_frame_rate = 150

    target = 0
    cursor2 = 0
    current_hue = rand()

    cursor = rand(0,5)

def prog4_loop():
    vase.sat += 0.1
    vase[cursor].sat = 0.0
    vase[cursor].hue = current_hue

    cursor += 5
    if cursor > vase.count:
        cursor = rand(0, 5)

    if cursor2 < top.count:
        top[cursor2].hue = current_hue

    cursor2 += 1

    if cursor2 > target:
        cursor2 = 0
        target = rand(40, 200)
        current_hue = rand(current_hue - 0.2, current_hue + 0.25)



def prog5_init():
    pixels.val = 1.0
    pixels.sat = 0.3
    pixels.hue = 0.0
    pixels.v_fade = 800
    pixels.hs_fade = 250

    db.gfx_frame_rate = 200

def prog5_loop():
    pixels.hue += 0.01

    pixels.val += 0.1

    pixels[rand()].val = 0.0


def prog6_init():
    pixels.val = 1.0
    pixels.sat = 1.0
    pixels.v_fade = 20
    pixels.hs_fade = 20

    db.gfx_frame_rate = 100

    current_hue = rand()
    pixels.hue = current_hue

def prog6_loop():
    current_hue += 0.005

    a = Fixed16()
    a = current_hue

    for i in pixels.count:
        pixels[i].hue = a
        
        a += 1.0 / pixels.count


def set_program(new_prog):
    if new_prog > 6:
        new_prog = 0

    if new_prog == program:
        return

    program = new_prog

    if program == 0:
        prog0_init()
    elif program == 1:
        prog1_init()
    elif program == 2:
        prog2_init()
    elif program == 3:
        prog3_init()
    elif program == 4:
        prog4_init()
    elif program == 5:
        prog5_init()
    elif program == 6:
        prog6_init()


def charging():
    top[cursor].val = 0.8
    top.val -= 0.15
    
    # batt status
    batt_soc = Number()    
    batt_soc = db.batt_soc
    batt_soc_f16 = Fixed16()
    batt_soc_f16 = batt_soc # workaround for broken type conversion

    current = Number()
    current = db.batt_charge_current

    if batt_soc_f16 < 10:
        current_hue = 0.0
        
    elif current <= 100:
        current_hue = 0.667
        
    else:
        current_hue = ( batt_soc_f16 * 0.333 ) / 100

    top[cursor].hue = current_hue

    cursor += 1

def startup():
    delay(1000)
    # batt status
    batt_soc = Number()    
    batt_soc = db.batt_soc

    if batt_soc < 10:
        top.hue = 0.0
        
        for i in 4:
            top.val = 1.0
            delay(500)
            top.val = 0.0
            delay(500)

    else:
        top.hue = ( batt_soc * 0.333 ) / 100
        
        for i in top.count:
            top[i].val = 1.0
            delay(100)


    delay(2000)
            
    set_program(0)


def shutdown():
    pixels.v_fade = 1000
    pixels.hs_fade = 500
    pixels.sat = 0.0
    pixels.val = 0.0

    pixels.v_fade = 100

    delay(200)

    for i in pixels.count:
        pixels.val -= 0.01
        pixels[(pixels.count - 1) - i].val = 0.5

        delay(20)

    pixels.v_fade = 1000
    pixels.val = 0.0
    delay(10000)

    halt()


def init():

    # set subdimmer
    db.gfx_sub_dimmer = 32000

    # set up potentiometer
    # db.io_cfg_adc0_4 = 5

    # set up button, input w/ pullup
    # db.io_cfg_dac0_6 = 1

    pixels.val = 0.0
    pixels.sat = 1.0
    pixels.hue = 0.0

    program = -1

    db.gfx_frame_rate = 100
    start_thread(startup)


def loop():
    if thread_running(startup) or thread_running(shutdown):
        return

    # check for shutdown
    elif db.ui_status < 0:
        start_thread(shutdown)
        return

    if is_charging:
        if db.batt_external_power == 0:
            is_charging = 0
            
            temp = Number()
            temp = program
            program = -1

            set_program(temp)

            return

        charging()
        
        return

    else:
        if db.batt_external_power != 0:
            db.gfx_frame_rate = 300
        
            pixels.v_fade = 500
            pixels.sat = 1.0
            vase.val = 0.0

            is_charging = 1

            return

    # dimmer_input = Number()
    # dimmer_input = (1400 - db.io_val_adc0_4) + 600

    # db.gfx_sub_dimmer = (65535 / (1400+ 600)) * dimmer_input
    
    # if db.io_val_dac0_6 == 0 and button_down == 0:
    if db.batt_button_state != 0 and button_down == 0:
        button_down = 1

        set_program(program + 1)


    # elif db.io_val_dac0_6 != 0:
    elif db.batt_button_state == 0:
        button_down = 0

    if program == 0:
        prog0_loop()
    elif program == 1:
        prog1_loop()
    elif program == 2:
        prog2_loop()
    elif program == 3:
        prog3_loop()
    elif program == 4:
        prog4_loop()
    elif program == 5:
        prog5_loop()
    elif program == 6:
        prog6_loop()

