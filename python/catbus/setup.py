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
    name='catbus',

    version='1.0.4',

    packages=['catbus'],

    scripts=[],

    package_data={},

    license='GNU General Public License v3',

    description='Catbus',

    long_description=open('README.txt').read(),

    install_requires=[
        "sapphire >= 1.0.4",
        "click == 7.1.1",
        "elysianfields >= 1.0.5",
        "fnvhash==0.1.0",
        "crcmod==1.7",
        "netifaces==0.10.9",
    ],

    entry_points='''
        [console_scripts]
        catbus=catbus.__main__:main
        catbus_directory=catbus.directory:main
    ''',
)
