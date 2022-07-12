
import os
import hashlib
import zipfile
from datetime import datetime
import json
import shutil

from sapphire.buildtools import core
from chromatron import CHROMATRON_ESP_UPGRADE_FWID


def make_manifest():
    now = datetime.utcnow()

    data = {
        'name': 'Chromatron ESP8266 3.0 UPGRADE',
        'timestamp': now.isoformat(),
        'fwid': CHROMATRON_ESP_UPGRADE_FWID,
        'version': '9.9.9'
    }

    with open('manifest.txt', 'w+') as f:
        f.write(json.dumps(data))

def make_firmware_zip():
    zf = zipfile.ZipFile('chromatron_esp_upgrade_fw.zip', 'w')
    zf.write('manifest.txt')
    zf.write('wifi_firmware.bin')
    zf.write('firmware.bin')
    zf.close()


if __name__ == '__main__':
    print("Make ESP upgrade package...")

    print("Build coprocessor firmware...")
    os.system('./build_coprocessor.sh')
    
    print("Build ESP8266 upgrade firmware...")
    os.system('./build_esp8266_upgrade.sh')

    shutil.copy('src/legacy/esp8266_upgrade_1mb/wifi_firmware.bin', '.')
    shutil.copy('src/legacy/coprocessor/firmware.bin', '.')

    make_manifest()

    make_firmware_zip()

    # copy firmware zip
    package_dir = core.get_build_package_dir()
    shutil.copy('chromatron_esp_upgrade_fw.zip', package_dir)

    # remove component firmware packages, they will just make things confusing
    os.remove(os.path.join(package_dir, 'esp8266_upgrade_1mb.zip'))
    os.remove(os.path.join(package_dir, 'coprocessor.zip'))

    print("Output dir: %s" % (package_dir))