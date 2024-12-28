#!/bin/bash

# python3 -m nuitka --plugin-enable=numpy --include-package=pygments --standalone fx.py
# python3 -m nuitka --include-package=pygments --standalone --show-modules --show-modules-output=nuitka.log fx.py
# python3 -m nuitka --include-package=pygments --nofollow-import-to=PyQT5 --nofollow-import-to=numpy --standalone --verbose --verbose-output=nuitka.log fx.py





# build chromatron tool
python3 -m nuitka --output-dir=bin/chromatron --standalone --onefile --verbose --verbose-output=nuitka.log chromatron.py
# install
# ln -s $(pwd)/bin/chromatron/chromatron.dist/chromatron.bin ~/.local/bin/chromatron
cp $(pwd)/bin/chromatron/chromatron.bin ~/.local/bin/chromatron

# # build catbus tool
python3 -m nuitka --output-dir=bin/catbus --standalone --onefile --verbose --verbose-output=nuitka.log catbus.py
# # install
# ln -s $(pwd)/bin/catbus/catbus.dist/catbus.bin ~/.local/bin/catbus
cp $(pwd)/bin/catbus/catbus.bin ~/.local/bin/catbus

# build fx tool
python3 -m nuitka --output-dir=bin/fx --standalone --onefile --verbose --verbose-output=nuitka.log fx.py
# # install
# ln -s $(pwd)/bin/fx/fx.dist/fx.bin ~/.local/bin/fx
cp $(pwd)/bin/fx/fx.bin ~/.local/bin/fx

# # build sapphiremake tool
python3 -m nuitka --output-dir=bin/sapphiremake --include-package-data=sapphire.buildtools --standalone --onefile --verbose --verbose-output=nuitka.log sapphiremake.py
# # install
# ln -s $(pwd)/bin/sapphiremake/sapphiremake.dist/sapphiremake.bin ~/.local/bin/sapphiremake
cp $(pwd)/bin/sapphiremake/sapphiremake.bin ~/.local/bin/sapphiremake

# # build sapphireconsole tool
python3 -m nuitka --output-dir=bin/sapphireconsole --standalone --onefile --verbose --verbose-output=nuitka.log sapphireconsole.py
# # install
# ln -s $(pwd)/bin/sapphireconsole/sapphireconsole.dist/sapphireconsole.bin ~/.local/bin/sapphireconsole
cp $(pwd)/bin/sapphireconsole/sapphireconsole.bin ~/.local/bin/sapphireconsole
