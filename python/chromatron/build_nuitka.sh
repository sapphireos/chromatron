#!/bin/bash

# python3 -m nuitka --plugin-enable=numpy --include-package=pygments --standalone fx.py
# python3 -m nuitka --include-package=pygments --standalone --show-modules --show-modules-output=nuitka.log fx.py
# python3 -m nuitka --include-package=pygments --nofollow-import-to=PyQT5 --nofollow-import-to=numpy --standalone --verbose --verbose-output=nuitka.log fx.py





# build chromatron tool
# python3 -m nuitka --include-package=pygments --nofollow-import-to=PyQT5 --nofollow-import-to=numpy --standalone --onefile --verbose --verbose-output=nuitka.log chromatron.py
# install
# ln -s $(pwd)/chromatron.dist/chromatron.bin ~/.local/bin/chromatron


# build sapphiremake tool
python3 -m nuitka --include-package=pygments --nofollow-import-to=PyQT5 --nofollow-import-to=numpy --standalone --onefile --verbose --verbose-output=nuitka.log sapphiremake.py
# install
# ln -s $(pwd)/chromatron.dist/chromatron.bin ~/.local/bin/chromatron
