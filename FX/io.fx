def init():
    pixels.val = 0.0
    pixels.sat = 0.0
    pixels.hue = 0.0

    # IO config:
    # 0 = input (high impedance)
    # 1 = input with pull up
    # 2 = input with pull down
    # 3 = output
    # 4 = output open drain
    # 5 = analog input

    db.io_cfg_gpio_0 = 2
    db.io_cfg_xck_1 = 2
    db.io_cfg_txd_2 = 2
    db.io_cfg_rxd_3 = 2
    db.io_cfg_adc0_4 = 2
    db.io_cfg_adc1_5 = 2
    db.io_cfg_dac0_6 = 2
    db.io_cfg_dac1_7 = 2

def loop():
    if db.io_val_gpio_0 == 0:
        pixels[0].val = 0.0

    else:
        pixels[0].val = 1.0

    if db.io_val_xck_1 == 0:
        pixels[1].val = 0.0

    else:
        pixels[1].val = 1.0

    if db.io_val_txd_2 == 0:
        pixels[2].val = 0.0

    else:
        pixels[2].val = 1.0

    if db.io_val_rxd_3 == 0:
        pixels[3].val = 0.0

    else:
        pixels[3].val = 1.0

    if db.io_val_adc0_4 == 0:
        pixels[4].val = 0.0

    else:
        pixels[4].val = 1.0

    if db.io_val_adc1_5 == 0:
        pixels[5].val = 0.0

    else:
        pixels[5].val = 1.0

    if db.io_val_dac0_6 == 0:
        pixels[6].val = 0.0

    else:
        pixels[6].val = 1.0

    if db.io_val_dac1_7 == 0:
        pixels[7].val = 0.0

    else:
        pixels[7].val = 1.0

