#!/usr/bin/env python3
# <license>
#
#     This file is part of the Chromatron project.
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

# Replicates sapphiremake's keyvalue define generation (see
# sapphire/buildtools/core.py, scan_file_for_kv): the firmware compiler
# receives -D__KV__<name>=<hash> for every __KV__<name> token it finds in
# the project sources. The host test harness must pass the same defines or
# files that use __KV__ constants (memory.c, keyvalue.c, ...) will not
# compile.
#
# The hash is FNV-1a 32 of the bare name - the same function as
# hash_u32_string() in src/sapphireos/hash.c (kv_hashes.json).
#
# usage: gen_kv_defs.py <repo_root> <source.c> [<source.h> ...]
# Prints one -D__KV__<name>=<hash> per line.

import glob
import os
import sys

KV_PREFIX = '__KV__'
KV_CHARS = set('abcdefghijklmnopqrstuvwxyz'
               'ABCDEFGHIJKLMNOPQRSTUVWXYZ'
               '0123456789_')


def fnv1a32(s):
    h = 0x811c9dc5
    for b in s.encode():
        h = ((h ^ b) * 16777619) & 0xffffffff
    return h


def scan(path, defs):
    try:
        data = open(path, encoding='utf-8').read()
    except UnicodeDecodeError:
        data = open(path, encoding='latin1').read()

    i = data.find(KV_PREFIX)
    while i >= 0:
        j = i + len(KV_PREFIX)
        while j < len(data) and data[j] in KV_CHARS:
            j += 1
        name = data[i + len(KV_PREFIX):j]
        if name:
            defs[KV_PREFIX + name] = fnv1a32(name)
        i = data.find(KV_PREFIX, i + 1)


def main():
    repo = sys.argv[1]
    defs = {}

    for f in sys.argv[2:]:
        p = os.path.join(repo, f)
        if os.path.exists(p):
            scan(p, defs)

    # headers in the OS library closure also carry __KV__ tokens
    for h in glob.glob(repo + '/src/sapphireos/*.h') + \
             glob.glob(repo + '/src/lib_gfx/*.h'):
        scan(h, defs)

    for k in sorted(defs):
        print('-D%s=%d' % (k, defs[k]))


if __name__ == '__main__':
    main()
