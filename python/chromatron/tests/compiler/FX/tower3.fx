
delay_min = Number()
delay_max = Number()


def start_threads():
	start_thread(layer0)
	start_thread(layer1)
	start_thread(layer2)
	start_thread(layer3)

def stop_threads():
	stop_thread(layer0)
	stop_thread(layer1)
	stop_thread(layer2)
	stop_thread(layer3)

def init():
	delay_min = 4000
	delay_max = 16000

	pixels.v_fade = 500
	pixels.hs_fade = 500

	pixels.val = 1.0
	pixels.hue = 0.0
	pixels.sat = 1.0

	start_thread(flush)

def flush():
	pixels.hue = rand()

	delay(2000)

	while True:
		start_threads()

		delay(rand(60000,180000))
		
		stop_threads()

		pixels.hs_fade = 5000
		pixels.hue = rand()
		pixels.hs_fade = 500

		delay(8000)

def layer0():	
	y_offset = Number()
	y_offset = 0
	hue = Fixed16()

	while True:
		hue = rand()
		for i in 64:
			pixels[rand()][y_offset].hue = hue

			delay(100)

		for x in pixels.size_x:
			pixels[x][y_offset].hue = hue

		delay(rand(delay_min, delay_max))

def layer1():	
	y_offset = Number()
	y_offset = 1
	hue = Fixed16()

	while True:
		hue = rand()
		for i in 64:
			pixels[rand()][y_offset].hue = hue

			delay(100)

		for x in pixels.size_x:
			pixels[x][y_offset].hue = hue

		delay(rand(delay_min, delay_max))

def layer2():	
	y_offset = Number()
	y_offset = 2
	hue = Fixed16()

	while True:
		hue = rand()
		for i in 64:
			pixels[rand()][y_offset].hue = hue

			delay(100)

		for x in pixels.size_x:
			pixels[x][y_offset].hue = hue

		delay(rand(delay_min, delay_max))

def layer3():	
	y_offset = Number()
	y_offset = 3
	hue = Fixed16()

	while True:
		hue = rand()
		for i in 64:
			pixels[rand()][y_offset].hue = hue

			delay(100)

		for x in pixels.size_x:
			pixels[x][y_offset].hue = hue

		delay(rand(delay_min, delay_max))
