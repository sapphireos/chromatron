import time
from chromatron import *

MODE = 'hsv'
# MODE = 'rgb'

if __name__ == '__main__':

    try:
        print "Discovering..."

        # query for devices
        devices = DeviceGroup('motion2')

        print "Found %d devices" % (len(devices))

        while True:

            # iterate through devices
            for d in devices.itervalues():

                if MODE == 'hsv':
                    # set brightness
                    d.val = 0.25

                    # set saturated colors
                    d.sat = 1.0

                    # set hue
                    for i in xrange(len(d.hue)):
                        d.hue[i] += (0.001 * (i + 1))

                elif MODE == 'rgb':
                    d.r = 0.0
                    d.g = 0.1
                    d.b = 0.1

            # send updated pixel data to devices
            devices.update_pixels(mode=MODE)

            time.sleep(0.02)

    except KeyboardInterrupt:
        pass
