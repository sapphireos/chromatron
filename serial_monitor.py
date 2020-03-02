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
        print port

    sys.exit(-1)

baudrate = 115200

try:
    baudrate = int(sys.argv[2])

except IndexError:
    pass

port = serial.Serial(port_name, baudrate=baudrate)

try:
    buf = ''

    while True:
        char = port.read(1)

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
