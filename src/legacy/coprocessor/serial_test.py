import sys
import os
import time

import serial

try:
    import serial.tools.list_ports
except ImportError:
    pass


try:
    port_name = sys.argv[1]

except IndexError:
    for port in serial.tools.list_ports.comports():
        try:
            print "%s %s %04x %04x" % (port.name, port.product, port.vid, port.pid)

        except TypeError:
            pass

    sys.exit(-1)

baudrate = 115200

try:
    baudrate = int(sys.argv[2])

except IndexError:
    pass

port = serial.Serial(port_name, baudrate=baudrate)


try:
    # port.timeout = 1.0
    # port.write("meow!")

    # print port.read(5)

    buf = ''
    i = 0
    count = 0

    while True:
        time.sleep(0.001)
        port.write("meow %d\n" % (i))
        i += 1

        char = port.read(1)
        count += 1
        # sys.stdout.write(char)

        if char in ['\r', '\n']:
            # check if buffer is empty
            if len(buf) == 0:
                continue
                
            print buf
            buf = ''

        else:
            buf += char


except KeyboardInterrupt:
    pass


port.close()
