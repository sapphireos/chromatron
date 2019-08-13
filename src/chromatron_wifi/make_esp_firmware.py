
import os
import hashlib
import zipfile
from datetime import datetime
import json
import shutil

from sapphire.buildtools import core
from chromatron import CHROMATRON_WIFI_FWID


def build():
    b = core.Builder(os.path.join(os.getcwd(), 'src'))
    hashes = b.create_kv_hashes()

    with open('__kv_hashes.h', 'w+') as f:
        f.write("/* AUTO GENERATED FILE! DO NOT EDIT! */\n")
        f.write("#ifndef __ASSEMBLER__\n")
        f.write("#ifndef __KV_HASHES_H__\n")
        f.write("#define __KV_HASHES_H__\n")

        f.write("#include <stdint.h>\n")

        for k, v in hashes.iteritems():
            f.write("#define %s (uint32_t)%s\n" % (k, v))

        f.write("#endif\n")
        f.write("#endif\n")

    # this doesn't work, because we need to cast the hashes to uint32_t.
    # if we don't, the xtensa compiler will sometimes glitch on some values and
    # screw up a function call.
    # we can't send them through the -D option, because the command 
    # can't parse the () 

    # defines = []
    # for k, v in hashes.iteritems():
    #     defines.append("-D%s=%s " % (k, v))

    # flags = ''.join(defines)
    # os.environ['PLATFORMIO_BUILD_FLAGS'] = flags

    os.environ['PLATFORMIO_BUILD_FLAGS'] = "-include __kv_hashes.h"

    os.system('platformio run')

def create_firmware_image():
    try:
        with open('.pioenvs/esp12e/firmware.bin', 'rb') as f:
            data = f.read()

    except IOError:
        with open('.pio/build/esp12e/firmware.bin', 'rb') as f:
            data = f.read()

    data_bytes = [ord(c) for c in data]

    # verify first byte (quick sanity check)
    if data_bytes[0] != 0xE9:
        print "Invalid firmware image!"
        return

    # we override bytes 2 and 3 in the ESP8266 image
    data_bytes[2] = 0
    data_bytes[3] = 0

    # convert back to string
    data = ''.join(map(chr, data_bytes))

    # need to pad to sector length
    padding_len = 4096 - (len(data) % 4096)
    data += (chr(0xff) * padding_len)

    md5 = hashlib.md5(data)
    data += md5.digest()
    sha256 = hashlib.sha256(data)

    with open('wifi_firmware.bin', 'wb') as f:
        f.write(data)

    return md5, sha256

def update_build_number():
    try:
        f = open('buildnumber.txt', 'r+')
        try:
            build_number = int(f.read())

            build_number += 1

            f.truncate(0)
            f.seek(0)
            f.write("%d" % (build_number))

        except:
            raise

        finally:
            f.close()

    except IOError:
        f = open('buildnumber.txt', 'w')
        build_number = "0"

        f.write(build_number)
        f.close()

def read_version():
    with open('src/version.h', 'r') as f:
        lines = f.readlines()

        major = None
        minor = None
        patch = None

        for line in lines:
            if line.startswith('#define VERSION_MAJOR'):
                line = line.replace('#define VERSION_MAJOR', '')
                major = int(line)

            elif line.startswith('#define VERSION_MINOR'):
                line = line.replace('#define VERSION_MINOR', '')
                minor = int(line)

            elif line.startswith('#define VERSION_PATCH'):
                line = line.replace('#define VERSION_PATCH', '')
                patch = int(line)


        return '%d.%d.%d' % (major, minor, patch)


def make_manifest(version, md5, sha256):
    now = datetime.utcnow()

    data = {
        'name': 'Chromatron ESP8266 firmware',
        'timestamp': now.isoformat(),
        'md5': md5.hexdigest(),
        'sha256': sha256.hexdigest(),
        'fwid': CHROMATRON_WIFI_FWID,
        'version': version
    }

    with open('manifest.txt', 'w+') as f:
        f.write(json.dumps(data))

def make_firmware_zip(version):
    zf = zipfile.ZipFile('chromatron_wifi_fw.zip', 'w')
    zf.write('manifest.txt')
    zf.write('wifi_firmware.bin')
    zf.close()


def clean():
    for f in ['wifi_firmware.bin', 'manifest.txt', 'chromatron_wifi_fw.zip']:
        try:
            os.remove(f)
        except OSError:
            pass


if __name__ == '__main__':
    # update_build_number()
    clean()
    build()
    version = read_version()
    md5, sha256 = create_firmware_image()

    print version
    print md5.hexdigest()
    print sha256.hexdigest()

    make_manifest(version, md5, sha256)

    make_firmware_zip(version)

    # copy firmware zip
    package_dir = core.get_build_package_dir()
    shutil.copy('chromatron_wifi_fw.zip', package_dir)


    shutil.copy('wifi_firmware.bin', '../chromatron')

