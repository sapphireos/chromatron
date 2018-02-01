Chromatron
==========

`Chromatron Wifi Pixel Controller <http://chromatron.io>`_


Support
-------

`Chromatron Google Group <https://groups.google.com/forum/#!forum/chromatron>`_


Build Instructions
------------------

Install Python tools:

.. note::
    The build system only works with Python 2.7 (for now).


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
