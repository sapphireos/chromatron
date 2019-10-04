
# a = Number(publish=True)
# b = Fixed16(publish=True)
# c = Fixed16(publish=True)
d = Fixed16(publish=True)
# e = Fixed16(publish=True)
# f = Fixed16(publish=True)
# g = Fixed16(publish=True)
# h = Fixed16(publish=True)
# i = Fixed16(publish=True)

ary = Array(4)
# ary1 = Array(4, type=Fixed16)

def init():
    # pixels.hue = 123
    # db.kv_test_key = pixels[0].hue
    # a = db.kv_test_key

    # pixels[0].val = db.kv_test_key
    # b = pixels[0].val

    # ary[0] = db.kv_test_key
    # c = ary[0]

    ary[1] = 456
    db.kv_test_key = ary[1]
    d = db.kv_test_key

    # ary1[1] = pixels[0].val
    # e = ary1[1]

    # ary1[2] = 0.123
    # pixels[0].val = ary1[2]
    # f = pixels[0].val

    # pixels[1].val = pixels[0].val
    # g = pixels[1].val

    # pixels[0].val = 0.333
    # db.kv_test_array[0] = pixels[0].val
    # h = db.kv_test_array[0]

    # db.kv_test_array[0] = 456
    # pixels[0].val = db.kv_test_array[0]
    # i = pixels[0].val


# prox_setting = Number()
# dimmer_setting = Number(publish=True)


# def startup():
#     pixels.sat = 1.0
#     pixels.val = 1.0
#     pixels.hue = 0.0
#     delay(1000) 

#     pixels.hue = 0.333
#     delay(1000) 

#     pixels.hue = 0.667
#     delay(1000) 

#     pixels.sat = 0.0
#     delay(1000) 
    
#     pixels.v_fade = 100
#     pixels.val = 0.0

#     delay(1000)

#     pixels.sat = 1.0
#     pixels.v_fade = 500
#     pixels.hs_fade = 50
#     start_thread('main')

# def init():
#     pixels.val = 0.0
#     pixels.sat = 0.0
#     pixels.hue = 0.0
#     pixels.hs_fade = 500

#     prox_setting = 4000
#     dimmer_setting = 4000

#     start_thread('startup')
    
# def main():  
#     while True:
#         if db.sensors_distance < 300 and db.sensors_distance_hover >= 8:
#             pixels.sat = 1.0
#             pixels.val = 1.0

#             prox_setting = (((db.sensors_distance - 130) * 65535) / (300 - 130))

#             if prox_setting < 4000:
#                 prox_setting = 4000

#             elif prox_setting > 65535:
#                 prox_setting = 65535

#             pixels.hue = 0.667 + (((db.sensors_distance - 130) * 0.333) / (300 - 130))

#             dimmer_setting = prox_setting


#         if db.sensors_distance_hover == 0 and pixels.is_v_fading == False:
#         # pixels.val = 0.0

#         delay(20)

# p1 = PixelArray()

# def init():
#     a = Number()
#     a = pixels.size_x

#     b = Number()
    # b = p1.is_v_fading

# i = Number()

# def init():
#     while i < 0:
#         for x in pixels.size_x:
#             pass

#     for x in pixels.size_x:
#         pass



# # a = String(publish=True)
# b = String(publish=True)
# c = String(32, publish=True)

# def init():
#     # a = "test"
#     s = String('test2')
#     b = s
#     # c = a
#     b = c


# my_str = String("rainbow.fxb")
# s2 = String(publish=True)
# s3 = String(publish=True)

# def init():
#     # s = String()
#     s_meow = String('meow')

#     # s = my_str
#     # s2 = my_str
#     # pass

#     # s2 = "meow"
#     s2 = my_str
#     s3 = "meow"