

import os
import os.path
import sys


license_text = """
    This file is part of the Sapphire Operating System.

    Copyright (C) 2013-2020  Jeremy Billheimer


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

        for filename in files:
            if filename in skip_files:
                print "skipping", filename
                continue

            name, ext = os.path.splitext(filename)

            if ext not in file_extensions:
                continue

            filepath = os.path.join(root, filename)
            with open(filepath, 'r+') as f:
                print filepath,

                license_start = None
                license_end = None

                # search lines
                lines = f.readlines()

                i = 0
                for line in lines:
                    if line.find(LICENSE_START) >= 0:
                        license_start = i

                    if line.find(LICENSE_END) >= 0:
                        license_end = i

                    if license_start and license_end:
                        break

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
                    print "Updating old license"

                else:
                    print "license header not found - skipping"
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
