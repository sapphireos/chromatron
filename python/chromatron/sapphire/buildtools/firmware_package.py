# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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
import shutil
import requests
import uuid
from datetime import datetime
from appdirs import *
from sapphire.common import util

PUBLISHED_AT_FILENAME = 'published_at.txt'
MANIFEST_FILENAME = 'manifest.json'
OLD_MANIFEST_FILENAME = 'manifest.txt'

class FirmwarePackageDirNotFound(Exception):
    pass

class BadFirmwareHash(Exception):
    pass

class FirmwarePackageNotFound(Exception):
    pass

class NotAFirmwarePackage(Exception):
    pass

def data_dir():
    return user_data_dir("Chromatron")

PACKAGE_DIR = os.path.join(data_dir(), 'firmware_packages')

def get_build_package_dir():
    return firmware_package_dir('build')

def firmware_package_dir(release=None):
    if release:
        return os.path.join(PACKAGE_DIR, release)

    else:
        return PACKAGE_DIR

def open_manifest(dir):
    with open(MANIFEST_FILENAME, 'r') as f:
        return json.loads(f.read())

def get_release(release='build', sort_fwid=False):
    cwd = os.getcwd()

    try:
        os.chdir(firmware_package_dir(release=release))

    except OSError:
        raise FirmwarePackageDirNotFound(firmware_package_dir())

    firmwares = {}

    # scan firmware zips
    for filename in os.listdir(os.getcwd()):
        filepath = os.path.join(os.getcwd(), filename)
        
        # try:

        filedir, ext = os.path.splitext(filename)
            
        if ext != '.zip':
            continue

        fw = FirmwarePackage(filepath=filename)

        firmwares[fw.name] = fw

    # change back
    os.chdir(cwd)

    return firmwares

def list_releases(use_date_for_key=False):
    cwd = os.getcwd()
    releases = {}

    try:
        for filename in os.listdir(firmware_package_dir()):    
            os.chdir(firmware_package_dir())

            if not os.path.isdir(filename):
                continue

            os.chdir(filename)
            try:
                with open(PUBLISHED_AT_FILENAME, 'r') as f:
                    # published_at = util.iso_to_datetime(f.read())
                    published_at = f.read()

                # check if there are any zip files (there should be!)
                has_zip = False
                for f in os.listdir(os.getcwd()):
                    if os.path.splitext(f)[1] == '.zip':
                        has_zip = True
                        break

                # no zips at all? this is definitely not the folder we're looking for.
                if not has_zip:
                    raise IOError

                if use_date_for_key:
                    releases[published_at] = filename

                else:
                    releases[filename] = published_at

            except IOError:
                # the published_at file does not exist, possibly some other folder got in somehow.
                pass

    except OSError:
        # firmware dir does not exist
        return {}

    # change back
    os.chdir(cwd)

    return releases


def get_most_recent_release():
    releases = list_releases(use_date_for_key=True)

    most_recent = sorted(releases.keys())[-1]

    return releases[most_recent]

def update_releases():
    # make sure the package dir exists
    try:
        os.makedirs(firmware_package_dir())

    except OSError:
        pass
    
    # get local releases
    local_releases = list_releases()

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
            with open(filename, 'wb') as f: # must be in binary mode or Windows will break
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


def get_firmware_package(name_or_fwid, release='build'):
    release_dir = os.path.join(PACKAGE_DIR, release)

    # test if UUID or name
    name = None
    try:
        uuid.UUID(name_or_fwid.replace('-', ''))
        
        # this is an FWID

        # look for package
        for file in [a for a in os.listdir(release_dir) if a.endswith('.zip')]:
            try:
                fw = FirmwarePackage(filepath=os.path.join(release_dir, file))

            except NotAFirmwarePackage:
                continue

            # check if FWID matches
            if fw.FWID.replace('-', '') == name_or_fwid.replace('-', ''):
                return fw

        raise FirmwarePackageNotFound(name_or_fwid)

    except ValueError:
        name = name_or_fwid

        try:
            fw = FirmwarePackage(filepath=os.path.join(release_dir, name_or_fwid + '.zip'))

        except FileNotFoundError:
            raise FirmwarePackageNotFound(name_or_fwid)

    return fw

class FirmwarePackage(object):
    def __init__(self, name=None, filepath=None):
        self.FWID = None
        self.name = name

        self.filename = f"{self.name}.zip"
        self.filepath = filepath

        # create empty manifest
        self.manifest = {
            'timestamp': None,
            'FWID': self.FWID,
            'name': self.name,
            'targets': {}
        }

        self.images = {}

        if self.filepath != None:
            self.load()

    def __str__(self):
        return f"FirmwarePackage:{self.name} FWID:{self.FWID}"

    def get_version_for_target(self, target):
        return self.manifest['targets'][target]['firmware.bin']['version']

    def load(self):
        with zipfile.ZipFile(self.filepath) as myzip:
            if MANIFEST_FILENAME not in myzip.namelist():
                # this is not a firmware package
                raise NotAFirmwarePackage
                            
            with myzip.open(MANIFEST_FILENAME) as myfile:
                self.manifest = json.loads(myfile.read())

            self.name = self.manifest['name']
            self.FWID = self.manifest['FWID'].replace('-', '')

            for target in self.manifest['targets']:
                self.images[target] = {}

                for file in self.manifest['targets'][target]:
                    with myzip.open(f"{target}_{file}", 'r') as myfile:
                        data = myfile.read()
                        self.images[target][file] = data

                        # verify file
                        sha256 = hashlib.sha256(data).hexdigest()

                        if sha256 != self.manifest['targets'][target][file]['sha256']:
                            raise BadFirmwareHash(target, file)

    def add_image(self, filename, data, target, version):
        if data == None:
            with open(filename, 'rb') as f:
                data = f.read()

        sha256 = hashlib.sha256(data).hexdigest()

        if target not in self.manifest['targets']:
            self.manifest['targets'][target] = {}

        if filename not in self.manifest['targets'][target]:
            self.manifest['targets'][target][filename] = {}

        self.manifest['targets'][target][filename]['sha256'] = sha256
        self.manifest['targets'][target][filename]['length'] = len(data)
        self.manifest['targets'][target][filename]['version'] = version

        if target not in self.images:
            self.images[target] = {}

        self.images[target][filename] = data

    def save(self):
        self.filename = f"{self.name}.zip"
        assert self.filename != "None.zip"

        # we only save to the build package
        self.filepath = os.path.join(get_build_package_dir(), self.filename)

        # remove old file
        try:
            os.remove(self.filepath)
        except FileNotFoundError:
            pass

        # make sure package dir exists
        try:
            os.makedirs(get_build_package_dir())

        except FileExistsError:
            pass    

        self.manifest['name'] = self.name
        self.manifest['FWID'] = self.FWID
        self.manifest['timestamp'] = datetime.utcnow().isoformat()

        with zipfile.ZipFile(self.filepath, mode='w') as myzip:
            myzip.writestr(MANIFEST_FILENAME, json.dumps(self.manifest, indent=4, separators=(',', ': ')))

            for target in self.images:
                for filename in self.images[target]:
                    myzip.writestr(f"{target}_{filename}", self.images[target][filename])

        with open(os.path.join(get_build_package_dir(), PUBLISHED_AT_FILENAME), 'w') as f:
            f.write(util.now().isoformat())

# if __name__ == "__main__":
#     from pprint import pprint

#     fw = FirmwarePackage('4b2e4ce5-1f41-494e-8edd-d748c7c81dcb')

    # pprint(get_firmware())
    # pprint(list_releases(use_date_for_key=True))
    # pprint(get_most_recent_release())
    # update_releases()




