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
        print(port)

    sys.exit(-1)

baudrate = 115200

try:
    baudrate = int(sys.argv[2])

except IndexError:
    pass

port = serial.Serial(port_name, baudrate=baudrate)

start = None

try:
    buf = ''

    while True:
        try:
            char = port.read(1).decode('utf-8')[0]

            if char in ['\r', '\n']:
                # check if buffer is empty
                if len(buf) == 0:
                    continue

                now = time.time()
                if start == None:
                    elapsed = 0.0

                else:
                    elapsed = now - start                   
                
                start = now
                print("%5d" % (elapsed * 1000), buf)
                buf = ''

            else:
                buf += char

        except UnicodeDecodeError:
            pass


except KeyboardInterrupt:
    pass


port.close()
