

with open('../../legacy/coprocessor/firmware.bin', 'r') as f:
	data = f.read()

with open('coproc_firmware.txt', 'w+') as f:
	# f.write('static const uint8_t wifi_firmware[] = {\n')

	for i in data:
		f.write(hex(i) + ',\n')

	# f.write('};\n')
