Status LED 
==========

Chromatron has an RGB status LED mounted near the middle of the board.  The colors and patterns all mean different things:



Power up
--------

Solid yellow
~~~~~~~~~~~~

This means the bootloader is running.  It will flash briefly on a normal start up, and will light for a few seconds after a firmware update.


Solid green
~~~~~~~~~~~~

The operating system is booting up.



Normal operation
----------------


Flashing green
~~~~~~~~~~~~~~

The operating system is running, and Wifi is not connected.


Flashing blue
~~~~~~~~~~~~~

Wifi is connected.


Flashing purple
~~~~~~~~~~~~~~~

Wifi is running as an access point.

.. note::
    On firmware v1.0, this mode will flash blue/white.



Errors
------

Flashing red
~~~~~~~~~~~~

The operating system has entered safe mode.  Please post on the support group if this happens, as this usually indicates the operating system has crashed and we may have a bug.



