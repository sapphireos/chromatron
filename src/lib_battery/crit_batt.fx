

def init():
	pixels.hue = 0.0
	pixels.sat = 1.0
	pixels.val = 0.0
	pixels.v_fade = 1000

	start_thread('main')

def main():
	cursor = Number()
	
	while True:
		
		pixels.v_fade = 1000
		pixels.val = 0.0

		pixels[cursor].v_fade = 100
		pixels[cursor].val = 0.3

		cursor += 1

		delay(200)
