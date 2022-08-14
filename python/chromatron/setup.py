# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
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

    version='1.0.7',

    packages=['chromatron',
              'chromatron.midi',
              'chromatron.osc',
              'catbus',
              'elysianfields',
              'sapphire',
              'sapphire.common',
              'sapphire.devices',
              'sapphire.protocols',
              'sapphire.buildtools',

            ],
            
    py_modules=['chromatron'],

    package_data={
        'chromatron': ['*.fx'],
        'sapphire.buildtools': ['settings.json', 'linker.x', 'project_template/*'],
        },

    scripts=[],

    license='GNU General Public License v3',

    description='',

    long_description=open('README.txt').read(),

    install_requires=[
        "crcmod == 1.7",
        "click == 7.1.2",
        "appdirs==1.4.3",
        "requests==2.22.0",
        # "setuptools >= 50.3.2",
        "pyserial == 3.4.0",
        "cmd2 == 0.6.9",
        "click == 7.1.2",
        "crcmod == 1.7",
        "intelhex == 2.1",
        "fnvhash==0.1.0",
        "zeroconf==0.24.4",
        "colorlog==4.1.0",
        "colored-traceback==0.3.0",
        "python-logging-loki==0.3.1",
        "prometheus-client==0.8.0",
        "paho-mqtt==1.5.1",
        "ifaddr==0.1.6",
        "filelock==3.0.12",
        "influxdb==5.3.1",
    ],

    entry_points='''
        [console_scripts]
        chromatron=chromatron.__main__:main
        sapphiremake=sapphire.buildtools.core:main
        sapphireconsole=sapphire.devices.sapphireconsole:main
        catbus=catbus.__main__:main
        catbus_directory=catbus.directory:main
    ''',
)
