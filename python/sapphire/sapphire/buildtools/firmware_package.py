# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>

import os
import zipfile
import hashlib
import json
import requests
import shutil
from appdirs import *
from sapphire.common import util

PUBLISHED_AT_FILENAME = 'published_at.txt'

class FirmwarePackageDirNotFound(Exception):
    pass

def data_dir():
    return user_data_dir("Chromatron", "SapphireOpenSystems")

def firmware_package_dir(release=None):
    package_dir = os.path.join(data_dir(), 'firmware_packages')

    if release:
        return os.path.join(package_dir, release)

    else:
        return package_dir

def open_manifest(dir):
    with open('manifest.txt', 'r') as f:
        return json.loads(f.read())

def get_firmware(release=None, include_firmware_image=False, sort_fwid=False):
    cwd = os.getcwd()

    try:
        os.chdir(firmware_package_dir(release=release))

    except OSError:
        raise FirmwarePackageDirNotFound(firmware_package_dir())

    firmwares = {}

    # scan firmware zips
    for filename in os.listdir(os.getcwd()):
        filepath = os.path.join(os.getcwd(), filename)
        
        try:

            filedir, ext = os.path.splitext(filename)
                
            if ext != '.zip':
                continue

            try:
                os.mkdir(filedir)

            except OSError:
                pass

            zf = zipfile.ZipFile(filename)
            zf.extractall(filedir)
            zf.close()

            os.chdir(filedir)

            # open the manifest
            manifest = open_manifest(os.getcwd())

            key = filedir
            if sort_fwid:
                key = str(manifest['fwid']).replace('-', '')

            firmwares[key] = {'manifest': manifest,
                              'short_name': filedir}

            if include_firmware_image:
                # load binary
                try:
                    with open('firmware.bin', 'rb') as f:
                        image = f.read()

                except IOError:
                    with open('wifi_firmware.bin', 'rb') as f:
                        image = f.read()

                firmwares[key]['image'] = {'binary': image}

                # verify firmware
                fw_sha256 = hashlib.sha256(image).hexdigest()
                valid = (fw_sha256 == firmwares[key]['manifest']['sha256'])
                firmwares[key]['image']['valid'] = valid

                if not valid:
                    # if not valid, delete the binary image, so
                    # we can't accidentally load it
                    del firmwares[key]['image']['binary']


            os.chdir('..')

            # clean up directory
            shutil.rmtree(filedir, True)


        except IOError:
            pass

        except zipfile.BadZipfile:
            pass


    # change back
    os.chdir(cwd)

    return firmwares

def get_releases(use_date_for_key=False):
    cwd = os.getcwd()
    releases = {}

    try:
        for filename in os.listdir(firmware_package_dir()):    
            os.chdir(firmware_package_dir())

            if not os.path.isdir(filename):
                continue

            os.chdir(filename)
            with open(PUBLISHED_AT_FILENAME, 'r') as f:
                # published_at = util.iso_to_datetime(f.read())
                published_at = f.read()

            if use_date_for_key:
                releases[published_at] = filename

            else:
                releases[filename] = published_at

    except OSError:
        # firmware dir does not exist
        return {}

    # change back
    os.chdir(cwd)

    return releases


def get_most_recent_release():
    releases = get_releases(use_date_for_key=True)
    
    most_recent = sorted(releases.keys())[-1]

    return releases[most_recent]

def update_releases():
    # make sure the package dir exists
    try:
        os.makedirs(firmware_package_dir())

    except OSError:
        pass
    
    # get local releases
    local_releases = get_releases()

    # get releases
    r = requests.get('https://api.github.com/repos/sapphireos/chromatron/releases')

    if r.status_code != 200:
        return []

    # sort releases
    releases = [a for a in reversed(sorted(r.json(), key=lambda a: a['published_at']))]

    new_releases = []

    for release in releases:
        # check if we already have this release
        if release['tag_name'] in local_releases:
            continue
        
        # we don't have it, let's download it
        for asset in [a for a in release['assets'] if a['content_type'] == 'application/zip']:
            # print asset['name'], asset['url'], asset['size']

            # get zip file
            headers = {'Accept': 'application/octet-stream'}
            r = requests.get(asset['url'], headers=headers)

            filename = os.path.join(firmware_package_dir(), asset['name'])
            with open(filename, 'w') as f:            
                f.write(r.content)

            # unzip it
            filedir = os.path.join(firmware_package_dir(), release['tag_name'])

            zf = zipfile.ZipFile(filename)
            zf.extractall(filedir)
            # for z in zf.infolist():
            #     print z.filename
            zf.close()

            # I *really* hate Mac sometimes.

            macosx_dir = os.path.join(filedir, "__MACOSX")
            try:
                shutil.rmtree(macosx_dir)
            except OSError:
                pass

        new_releases.append(release['tag_name'])

    return new_releases

if __name__ == "__main__":
    from pprint import pprint

    # pprint(get_firmware())
    pprint(get_releases(use_date_for_key=True))
    # pprint(get_most_recent_release())
    # update_releases()




