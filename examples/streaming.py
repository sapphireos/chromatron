import time
from chromatron import *


if __name__ == '__main__':

    try:
        print "Discovering..."

        # query for devices
        devices = DeviceGroup('test')

        print "Found %d devices" % (len(devices))

        while True:

            # iterate through devices
            for d in devices.itervalues():

                # set brightness
                d.val = 0.25

                # set saturated colors
                d.sat = 1.0

                # set hue
                for i in xrange(len(d.hue)):
                    d.hue[i] += (0.0001 * i)


            # send updated pixel data to devices
            devices.update_pixels()

            time.sleep(0.02)

    except KeyboardInterrupt:
        pass
