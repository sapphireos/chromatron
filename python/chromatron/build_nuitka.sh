#!/bin/sh

# python3 -m nuitka --plugin-enable=numpy --include-package=pygments --standalone fx.py
# python3 -m nuitka --include-package=pygments --standalone --show-modules --show-modules-output=nuitka.log fx.py


# python3 -m nuitka --include-package=pygments --nofollow-import-to=PyQT5 --nofollow-import-to=numpy --standalone --verbose --verbose-output=nuitka.log fx.py
python3 -m nuitka --include-package=pygments --nofollow-import-to=PyQT5 --nofollow-import-to=numpy --standalone --verbose --verbose-output=nuitka.log chromatron.py