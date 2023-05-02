import sys
import os
import time

import serial

CP2102_VID = 0x10c4
CP2102_PID = 0xea60

def monitor(portname=None, baud=115200, reconnect=False):
    if portname is None:
        ports = list(serial.tools.list_ports.comports())

        for comport in ports:
            if comport.vid == CP2102_VID and comport.pid == CP2102_PID:
                portname = comport.device
                break
    
    assert portname is not None

    print(f"Connecting monitor on port {portname}")    

    try:
        while reconnect:
            while True:
                try:
                    port = serial.Serial(portname, baudrate=baud)
                    break

                except serial.serialutil.SerialException:
                    if not reconnect:
                        raise

                    time.sleep(0.1)


            start = None
        
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

                except serial.serialutil.SerialException:
                    if not reconnect:
                        raise

                    time.sleep(0.1)


    except KeyboardInterrupt:
        pass

    port.close()


def main():
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

    monitor(port_name, baudrate)


if __name__ == "__main__":
    main()


