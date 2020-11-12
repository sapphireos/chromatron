# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2020  Jeremy Billheimer
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
    name='sapphire',

    version='1.0.4',

    packages=['sapphire',
              'sapphire.common',
              'sapphire.devices',
              'sapphire.protocols',
              'sapphire.buildtools',
              ],

    package_data={'sapphire.buildtools': ['project_template/*']},

    license='GNU General Public License v3',

    description='Sapphire Tools',

    long_description=open('README.txt').read(),

    install_requires=[
        "appdirs==1.4.3",
        "requests==2.22.0",
        "setuptools >= 49.2.0",
        "pyserial == 3.4.0",
        "cmd2 == 0.6.9",
        "click == 7.1.2",
        "crcmod == 1.7",
        "pyparsing == 2.2.0",
        "intelhex == 2.1",
        "elysianfields >= 1.0.5",
        "fnvhash==0.1.0",
        "zeroconf==0.24.4",
        "colorlog==4.1.0",
    ],

    entry_points='''
        [console_scripts]
        sapphiremake=sapphire.buildtools.core:main
        sapphireconsole=sapphire.devices.sapphireconsole:main
    ''',
)
