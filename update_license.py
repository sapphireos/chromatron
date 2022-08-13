

import os
import os.path
import sys


license_text = """
    This file is part of the Sapphire Operating System.

    Copyright (C) 2013-2022  Jeremy Billheimer


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

file_extensions = [
    '.py',
    '.c',
    '.cpp',
    '.h'
]

skip_files = [
    'CuTest.c',
    'CuTest.h',
    'OSC.py',
    'update_license.py',
]

skip_folders = [
    'esp-idf',
]

ext_comments = {
    '.py': '#',
    '.c': '//',
    '.cpp': '//',
    '.h': '//',
}

LICENSE_START = '<license>'
LICENSE_END = '</license>'


def gen_license_text(ext):
    comment = ext_comments[ext]

    front = comment + ' ' + LICENSE_START + '\n'
    back = comment + ' ' + LICENSE_END + '\n'

    middle = ''
    for line in license_text.split('\n'):
        middle += comment + ' ' + line + '\n'

    return front + middle + back


if __name__ == "__main__":
    target_path = sys.argv[1]


    for root, dirs, files in os.walk(target_path):
        # print root, dirs, files

        skip_dir = False
        for skip in skip_folders:
            if skip in root:
                skip_dir = True
                break

        if skip_dir:
            print(f'Skipping dir {root}')
            continue


        for filename in files:
            if filename in skip_files:
                print(f"skipping {filename}")
                continue

            name, ext = os.path.splitext(filename)

            if ext not in file_extensions:
                continue

            filepath = os.path.join(root, filename)
            with open(filepath, 'r+') as f:

                license_start = -1
                license_end = -1

                # search lines
                try:
                    lines = f.readlines()

                except UnicodeDecodeError as e:
                    print(f"ERROR {filename}: {e}")
                    continue

                i = 0
                for line in lines:
                    if license_start < 0 and line.find(LICENSE_START) >= 0:
                        license_start = i

                    if license_end < 0 and line.find(LICENSE_END) >= 0:
                        license_end = i

                    i += 1

                # license found
                if license_start >= 0 and license_end >= 0:
                    # remove old license
                    front = ''.join(lines[:license_start])
                    back = ''.join(lines[license_end + 1:])

                    # add new license
                    license = gen_license_text(ext)

                    updated_lines = front + license + back

                    # print updated_lines
                    print(f"{filepath} Updating old license")

                else:
                    print(f"{filepath} license header not found - skipping")
                    continue
                    
                    # # add new license to top of file
                    # license = gen_license_text(ext)
                    
                    # updated_lines = license + ''.join(lines)
                    
                    # # print updated_lines
                    # print "Adding license"

                # now, rewrite the file
                f.truncate(0)
                f.seek(0)
                f.write(updated_lines)
