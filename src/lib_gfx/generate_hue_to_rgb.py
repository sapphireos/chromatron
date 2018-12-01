

rgb = []

for h in xrange(65536):
    temp_r = 0
    temp_g = 0
    temp_b = 0

    if h <= 21845:
    
        temp_r = ( 21845 - h ) * 3
        temp_g = 65535 - temp_r
    
    elif h <= 43690:
    
        temp_g = ( 43690 - h ) * 3
        temp_b = 65535 - temp_g
    
    else:
    
        temp_b = ( 65535 - h ) * 3
        temp_r = 65535 - temp_b
    
    temp_r >>= 8
    temp_g >>= 8
    temp_b >>= 8

    rgb.append((temp_r, temp_g, temp_b))

with open('hue_table.txt', 'w+') as f:
    f.write('static const uint8_t hue_table[65536][3] = {\n')
    for r, g, b in rgb:
        f.write('\t{%d, %d, %d},\n' % (r, g, b))

    f.write('};')

