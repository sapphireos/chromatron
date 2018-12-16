

a = Number(publish=True)

def init():
    temp = Number()    
    temp = 90

    if temp < 10:
        a = 0
        
    elif temp >= 95:
        a = 1
        
    else:
        a = temp + 1


# current_hue = Number()

# def init():
#     # charging()
    
#     # batt status
#     batt_soc = Number()    
#     batt_soc = 90

#     if batt_soc < 10:
#         current_hue = 0
        
#     elif batt_soc >= 95:
#         current_hue = 43690
        
#     else:
#         current_hue = ( batt_soc * 21845 ) / 100
#         