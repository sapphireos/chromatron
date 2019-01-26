@schedule(hours=23, minutes=0, seconds=0)
def turn_half():
    db.gfx_sub_dimmer = 32768


@schedule(hours=0, minutes=0, seconds=0)
def turn_off():
    db.gfx_sub_dimmer = 0

@schedule(hours=8, minutes=0, seconds=0)
def turn_on():
    db.gfx_sub_dimmer = 65535

