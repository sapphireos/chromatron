import os
import hashlib
import zipfile
from datetime import datetime
import json

projects = [
    'hal_arm',
    'sapphire',
    'hal_stm32h7',
    'lib_chromatron',
    'lib_battery',
    'lib_veml7700',
    'lib_rfm95w',
    'lib_ssd1306',
    'lib_gfx',
    'chromatron',
]

def clean():
    for f in ['firmware.bin', 'manifest.txt', 'chromatron_main_fw.zip']:
        try:
            os.remove(f)
        except OSError:
            pass

    for proj in projects:
        os.system('sapphiremake -p %s -c' % (proj))


def build():
    for proj in projects:
        os.system('sapphiremake -p %s -t stm32h7' % (proj))
        # os.system('sapphiremake -p %s -t stm32h7_debug' % (proj))
        # os.system('sapphiremake -p %s -t stm32h7_noboot' % (proj))


if __name__ == '__main__':
    cwd = os.getcwd()

    clean()
    build()

    os.chdir(cwd)
