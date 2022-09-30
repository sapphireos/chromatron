@schedule(hours=6, minutes=30, seconds=0)
def turn_on():
    db.gfx_sub_dimmer = 16384

