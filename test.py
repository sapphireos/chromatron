import serial
import time

s = serial.Serial('/dev/tty.usbserial-FT9LKADW', baudrate=115200)

for i in xrange(16000):
	data = "Meow" * 16

	print s.write(data)


time.sleep(1.0)

s.close()
