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

from setuptools import setup

setup(
    name='chromatron',

    version='1.0.2',

    packages=['chromatron',
              'chromatron.midi',
              'chromatron.osc',
            ],
            
    py_modules=['chromatron'],

    package_data={'chromatron': ['*.fx']},

    scripts=[],

    license='GNU General Public License v3',

    description='',

    long_description=open('README.txt').read(),

    install_requires=[
        "catbus >= 1.0.2",
        "crcmod == 1.7",
        "click >= 6.6",
        "sapphire >= 1.0.2",
        "elysianfields >= 1.0",
    ],

    entry_points='''
        [console_scripts]
        chromatron=chromatron.chromatron:main
        chromatron_legacy=chromatron.chromatron_legacy:main
    ''',
)
