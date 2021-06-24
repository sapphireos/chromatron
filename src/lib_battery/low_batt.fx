

def init():
	pixels.hue = 0.0
	pixels.sat = 1.0
	pixels.val = 0.0
	pixels.v_fade = 1000

	start_thread('main')

def main():
	while True:
		
		pixels.val = 0.0

		delay(2000)

		pixels.val = 0.3

		delay(2000)

