import os
import hashlib
import zipfile
from datetime import datetime
import json
import shutil

projects = [
    'sapphire',
    'hal_xmega128a4u',
    # 'lib_chromatron',
    # 'lib_gfx',
    # 'lib_fixmath',
    'chromatron_legacy',
    #'chromatron_recovery',
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
        os.system('sapphiremake -p %s -t chromatron_legacy' % (proj))


if __name__ == '__main__':
    cwd = os.getcwd()

    clean()

    shutil.copy('src/chromatron_wifi/wifi_firmware.bin', 'src/chromatron_legacy')

    build()

    os.chdir(cwd)
