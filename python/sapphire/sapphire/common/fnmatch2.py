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

import fnmatch

MAX_FILENAMES = 10000
MAX_PATTERNS = 10000

cache = {}

# @profile
def fnmatch2(filename, pattern):
    if filename in cache:
        if pattern in cache[filename]:
            return cache[filename][pattern]

        else:
            if len(cache[filename]) > MAX_PATTERNS:
                cache[filename].popitem()

    else:
        if len(cache) > MAX_FILENAMES:
            cache.popitem()

        cache[filename] = {}

    if fnmatch.fnmatch(filename, pattern):
        cache[filename][pattern] = True
        return True

    else:
        cache[filename][pattern] = False
        return False

# @profile
def filter2(names, pattern):
    if pattern == '*':
        return names

    return [n for n in names if fnmatch2(n, pattern)]


import time

# @profile
def test_fnmatch():
    start = time.time()
    for i in xrange(1000000):
        fnmatch.fnmatch('stuff', '*')
        fnmatch.fnmatch('things.stuff', '*.stuff')
        fnmatch.fnmatch('cat.meow', 'cat*.meow')

    elapsed = time.time() - start

    print elapsed * 1000.0


# @profile
def test_fnmatch2():
    start = time.time()
    for i in xrange(1000000):
        fnmatch2('stuff', '*')
        fnmatch2('things.stuff', '*.stuff')
        fnmatch2('cat.meow', 'cat*.meow')

    elapsed = time.time() - start

    print elapsed * 1000.0


def test_mem_usage():
    for i in xrange(1000):
        for j in xrange(100):
            fnmatch2('stuff.%d' % (i), '*%d' % (j))

    for i in xrange(1000):
        for j in xrange(100):
            fnmatch2('stuff.%d' % (i), '*%d' % (j))

# test_fnmatch()
# test_fnmatch2()

# test_mem_usage()

# print "DONE"
# time.sleep(10.0)


