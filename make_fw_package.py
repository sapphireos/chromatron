
import os
import shutil
import zipfile

def clean():
    for f in ['chromatron_fw.zip']:
        try:
            os.remove(f)
        except OSError:
            pass


def build_wifi():
    cwd = os.getcwd()

    os.chdir('../src/chromatron_wifi')

    os.system('python make_esp_firmware.py')

    shutil.copy('chromatron_wifi_fw.zip', cwd)

    os.chdir(cwd)


def build_main():
    os.system('python build.py')

def make_package_zip():
    zf = zipfile.ZipFile('chromatron_fw_package.zip', 'w')
    zf.write('chromatron_wifi_fw.zip')
    zf.write('chromatron_main_fw.zip')
    zf.close()


if __name__ == '__main__':
    clean()
    build_main()
    build_wifi()
    make_package_zip()
