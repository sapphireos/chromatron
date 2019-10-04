a = String(publish=True)
b = String(publish=True)
c = String(publish=True)
d = String("test3", publish=True)
e = String(publish=True)

def init():
    a = "test"
    s = String('test2')
    b = s
    c = a
    e = d



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