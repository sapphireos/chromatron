


# Shard



lux_control_program_selector = Number(publish=True)
lux_control_current_program = Number(publish=True)
lux_control_countdown = Number(publish=True)

def program_selector():
    lux_control_current_program = -1

    while True:
        delay(1000)

        if lux_control_program_selector == 1:
            db.vm_prog = "markers.fxb"
        
        elif lux_control_program_selector == 2:
            db.vm_prog = "shimmer.fxb"

        elif lux_control_program_selector == 3:
            db.vm_prog = "beam.fxb" 

        elif lux_control_program_selector == 4:
            db.vm_prog = "shard.fxb"

        else:
            db.vm_prog = "markers_low_power.fxb"
        
        # reset program
        db.vm_reset = True

        lux_control_current_program = lux_control_program_selector

        while lux_control_current_program == lux_control_program_selector:
            delay(100)

def timer():
    while True:
        delay(1000)

        if lux_control_countdown > 0:
            lux_control_countdown -= 1


def main_sequence():
    lux_control_countdown = 3600 * 8 # countdown in hours

    lux_control_program_selector = 2

    enable()

    delay(300000)

    lux_control_program_selector = 3

    while True:
        delay(5000)

        if lux_control_countdown <= 0:
            lux_control_program_selector = 1 # countdown ended, reset sequence
        
        elif db.batt_soc < 80:
            lux_control_program_selector = 4


def enable():
    db.gfx_enable = True
    db.vm_run = True

def disable():
    stop_thread(main_sequence)

    db.gfx_enable = False
    db.vm_run = False



def lux():
    db.gfx_sub_dimmer = 65535

    while True:

        delay(10000)


        # check battery level

        # too low, shut down
        if db.batt_soc < 25:
            lux_control_program_selector = -3

            disable()

            # wait until ALS comes back up for a new charge cycle
            while db.veml7700_filtered_als < 100000:
                delay(10000)

                disable()


        # low battery
        elif db.batt_soc < 40:
            disable()

            lux_control_program_selector = -2

            enable()

        # check if light level is low enough to trigger:

        # daybreak
        elif db.veml7700_filtered_als > 25000:
            disable()

            lux_control_program_selector = -1

            pixels.v_fade = 2000
            disable()

            
        # main sequence already running, this will skip the twilight checks.
        # should avoid a false change to twilight from a stray light.
        # though just letting it happen could be cool too.
        elif thread_running(main_sequence):
            continue
            
        # darkness
        elif db.veml7700_filtered_als < 5000: # millilux!
            if not thread_running(main_sequence):
                start_thread(main_sequence)

        # twilight
        elif db.veml7700_filtered_als < 10000: # millilux!

            lux_control_program_selector = 1

            enable()

    

def init():
    disable()

    start_thread(lux)
    start_thread(program_selector)
    start_thread(timer)

