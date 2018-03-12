Chromatron
==========

`Chromatron Wifi Pixel Controller <http://chromatron.io>`_

Chromatron is an open source Wifi pixel controller. It is a complete hardware and software toolkit designed specifically for making projects with LED pixel strips.

Supported LED types:

- APA102
- WS2801
- WS2811/WS2812/WS2812b
- SK6812 (RGB and RGBW)
- Analog/PWM (RGB only - post an issue if you need RGBW support)
- `Adafruit Pixie <https://www.adafruit.com/product/2741>`_


Documentation
-------------

`chromatron.readthedocs.io <https://chromatron.readthedocs.io>`_


Support
-------

`Chromatron Google Group <https://groups.google.com/forum/#!forum/chromatron>`_


Build Instructions
-------------------

Install Python tools:


The build system only works with Python 2.7 (for now).  Additionally, building on Windows is also broken (for now).  Official releases are built on Mac OS X.  Builds on Ubuntu should also work.


.. code:: bash

    $ pip install chromatron

You will also need PlatformIO (which is needed to build the wifi firmware):

.. code:: bash

    $ pip install platformio


Make sure you are in the root folder of the repository for the remaining steps.


Install compiler toolchain:

.. code:: bash

    $ sapphiremake --install_build_tools


Scan for firmware projects.  This searchs the folder structure for all of the projects so the build tools can find everything.

.. code:: bash

    $ sapphiremake --discover


Build the bootloader project:

.. code:: bash

    $ sapphiremake -p loader_xmega128a4u


Build everything else:

.. code:: bash

    $ python make_fw_package.py



License Information
-------------------

The software contents of this repository are released under the GNU General Public License v3.

See http://www.gnu.org/licenses/ for more information.
