import os
import hashlib
import zipfile
from datetime import datetime
import json

projects = [
    'hal_arm',
    'hal_stm32h7',
    'sapphire',
    "lib_armloader",
    "loader_stm32h7"
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
        os.system('sapphiremake -p %s -t stm32h7_boot --loader' % (proj))


if __name__ == '__main__':
    cwd = os.getcwd()

    clean()
    build()

    os.chdir(cwd)
