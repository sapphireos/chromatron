
def init():
    pixels.val = 0.5
    pixels.sat = 1.0

def loop():
    pixels.hue = ( ( db.wifi_rssi + 80 ) * 25000 ) / 50