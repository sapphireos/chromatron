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
    name='sapphire',

    version='1.0.1',

    packages=['sapphire',
              'sapphire.fields',
              'sapphire.common',
              'sapphire.query',
              'sapphire.udpx',
              'sapphire.devices',
              'sapphire.buildtools',
              ],

    package_data={'sapphire.buildtools': ['settings.json', 'linker.x', 'project_template/*']},

    license='GNU General Public License v3',

    description='Sapphire Tools',

    long_description=open('README.txt').read(),

    install_requires=[
        "catbus",
        "appdirs==1.4.3",
        "requests==2.18.4",
        "setuptools == 34.2.0",
        "pyserial == 3.2.1",
        "bitstring == 3.1.5",
        "cmd2 == 0.6.9",
        "click >= 6.6",
        "crcmod == 1.7",
        "pyparsing == 2.2.0",
        "intelhex == 2.1",
        "pydispatcher == 2.0.5",
        "elysianfields >= 1.0",
        "fnvhash",
    ],

    entry_points='''
        [console_scripts]
        sapphiremake=sapphire.buildtools.core:main
        sapphireconsole=sapphire.devices.sapphireconsole:main
    ''',
)
