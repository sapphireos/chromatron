

def init():
	pixels.hue = 0.0
	pixels.sat = 1.0
	pixels.val = 0.0
	pixels.v_fade = 100

	start_thread('main')

def main():
	cursor = Number()
	
	while True:
		
		pixels[cursor].v_fade = 1000
		pixels[cursor].val = 0.0

		cursor += 1

		pixels[cursor].v_fade = 100
		pixels[cursor].val = 0.3

		delay(200)
