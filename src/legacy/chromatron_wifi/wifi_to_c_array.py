

with open('wifi_firmware.bin', 'r') as f:
	data = f.read()

with open('wifi_firmware.txt', 'w+') as f:
	# f.write('static const uint8_t wifi_firmware[] = {\n')

	for i in data:
		f.write(str(ord(i)) + ',\n')

	# f.write('};\n')