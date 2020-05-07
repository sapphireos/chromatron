import os
import hashlib
import zipfile
from datetime import datetime
import json

projects = [
    'sapphire',
    'hal_stm32f7',
    # 'stm_test',
    'lib_chromatron2',
    'lib_gfx',
    'chromatron2',
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
        os.system('sapphiremake -p %s -t stm32f7_debug' % (proj))


if __name__ == '__main__':
    cwd = os.getcwd()

    clean()
    build()

    os.chdir(cwd)
