
max_temp = Number()
min_temp = Number()

def init():
    # set pixels to full colors (maximum saturation)
    pixels.sat = 1.0

    # set pixels default hue to red
    pixels.hue = 0.0

    pixels.val = 1.0

    pixels.v_fade = 1000
    pixels.hs_fade = 1000

    min_temp = 90
    max_temp = 120


def loop():
    
    # for y in pixels.size_y:
    #     for x in pixels.size_x:
    for y in 8:
        for x in 8:
            hue = Number()

            hue = ( ( db.amg_pixel_delta[x * 8 + (8 - y)] - min_temp ) * 65536 ) / ( max_temp - min_temp )

            pixels[x * 2][y * 2].hue = hue
            pixels[x * 2 + 1][y * 2].hue = hue
            pixels[x * 2][y * 2 + 1].hue = hue
            pixels[x * 2 + 1][y * 2 + 1].hue = hue


            val = Fixed16()

            if db.amg_pixel_delta[x * 8 + (8 - y)] <= 1:
                val = 0.0

            else:
                val = 1.0

            pixels[x * 2][y * 2].val = val
            pixels[x * 2 + 1][y * 2].val = val
            pixels[x * 2][y * 2 + 1].val = val
            pixels[x * 2 + 1][y * 2 + 1].val = val


            # pixels[x][y].hue = db.amg_data[y * pixels.size_y + x]

            # pixels[x][y].hue = ( ( db.amg_data[x * pixels.size_x + y] - 15000 ) * 65536 ) / ( 30000 - 15000 )

            # pixels[x][y].val = ( ( db.amg_data[x * pixels.size_x + y] - min_temp ) * 65536 ) / ( max_temp - min_temp )

            # pixels[x][y].hue = db.amg_data[y]

            # pixels[x][y].hue = temp

            # temp += 1000

            # db.kv_test_key = y * pixels.size_y + x

            # if y == 4:
                # db.kv_test_array[x] = db.amg_data[y * pixels.size_y + x]
                # db.kv_test_array[x] = y * pixels.size_y + x

    # for i in len(db.amg_data):
    #     pixels[i].hue = db.amg_data[i]

    # db.kv_test_key = db.amg_data[32]

    # db.kv_test_key = pixels.size_y