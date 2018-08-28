import os
import hashlib
import zipfile
from datetime import datetime
import json

projects = [
    'sapphire',
    'hal_xmega128a4u',
    'lib_chromatron',
    # 'lib_fixmath',
    'chromatron',
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
        os.system('sapphiremake -p %s -t chromatron' % (proj))


if __name__ == '__main__':
    cwd = os.getcwd()

    clean()
    build()

    os.chdir(cwd)
