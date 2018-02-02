
Getting Started
===============


Resources for beginners
-----------------------

If this is your first time doing an LED project, you can do no better than Adafruit's excellent getting started guides:

`Adafruit NeoPixel Überguide <https://learn.adafruit.com/adafruit-neopixel-uberguide>`_

`Adafruit DotStar LEDs <https://learn.adafruit.com/adafruit-dotstar-leds>`_

While you're there, please consider picking up your first set of LEDs from them.  You can absolutely find LEDs cheaper elsewhere on the Internet, but Adafruit helps enable projects like Chromatron to exist by the sheer volume of information, guides, and enthusiasm they create.  The Maker community thrives on the good will of its members.


Where to get help
-----------------

Stuck? Something broken? Just want to show off your project?

`This is the place for all of that <https://groups.google.com/forum/#!forum/chromatron>`_.


Want to file a bug report (or request a feature)?  You can do that on the `Github page <https://github.com/sapphireos/chromatron/issues>`_.  Please only file an issue for actual bugs (or features) - if you just need help, the Google Group is a better place for that.



Things you will need
--------------------

- Chromatron
- LEDs
- A suitable power supply
- A micro USB cable
- A precision sized flathead screwdriver (1/16 inch or 1.6 mm)
- A multimeter (not technically required, but since you're working with electricity, you should really have one)


Install tools
-------------

First, you're going to need Python 2.7, which you can get `here <https://www.python.org/downloads/>`_.  You won't need to use Python directly, but the command line tools need it to run.

Next, install Chromatron's tools:

.. code:: bash

    $ pip install chromatron


You can check that it installed by checking the version:

.. code:: bash

    $ chromatron --version




Basic setup
-----------

First, we're going to get Chromatron on your Wifi network.

Plug in your Chromatron via USB.  The on-board LED will go through a few states as the operating system boots up.  See :doc:`status_led` to learn what the LED colors mean.

Let's see if we can talk to the Chromatron:

.. code:: bash

    $ chromatron --host usb discover

This should come up with some information about your Chromatron.

Next, let's set up the Wifi:

.. code:: bash

    $ chromatron --host usb wifi setup

.. note::
    Chromatron only supports 2.4GHz networks.


Next, let's give it a name:

.. code:: bash

    $ chromatron --host usb meta setup

.. note::
    The meta setup will also ask for a location and a set of tags.  These are optional, but useful for assigning Chromatrons to groups. Read more about meta tags here: :doc:`keyvalue`.


Find your Chromatron on the network:

.. code:: bash

    $ chromatron discover

If you have multiple Chromatrons, you can also query for specific ones:

.. code:: bash

    $ chromatron -q meow discover

This will find any Chromatrons named, tagged, or in location 'meow'.

.. note::
    Meta tags must be 32 characters or less, encoded in ASCII (no Unicode support for now), and must not contain spaces.



LED setup
---------

Now for the fun part: connecting your LED strip.  Hardware connections are shown here: :doc:`hardware`.

.. warning::
    Unplug the USB cable before connecting an LED strip!


.. caution::
    Powering via USB:
    Chromatron can power LEDs via the USB port.  However, USB ports can only power a small number of LEDs (8 to 10 is fairly safe - unless you are using high power LEDs like the Pixie).  If you connect a full 300 LED strip and plug in the USB, you might damage your USB port or Chromatron, and that would be sad.


Got your LEDs connected?  Double check your wiring before you power up!  Everything ok? Let's move on:

.. code:: bash

    $ chromatron -q meow pixels setup


This will walk you through the pixel setup, and also get the RGB order configured.


Dimmer Controls
---------------

You can control the dimmers via the command line:

.. code:: bash

    $ chromatron -q meow dimmer master 1.0


Will set to full brightness.

See :ref:`dimmer-reference` for more detailed information about how the dimmer controls work.



FX Script
---------

Example scripts are in the FX folder in the Github repository.  You can download it here if you are not using git: `https://github.com/sapphireos/chromatron/archive/master.zip <https://github.com/sapphireos/chromatron/archive/master.zip>`_


The LED setup loads the rainbow demo.  You can load different scripts:


.. code:: bash

    $ chromatron -q meow vm load script_name.fx


.. note::
    The chaser.fx example will not work properly on firmware v1.0, due to a slight bug in the virtual machine (it runs the loop twice per iteration, instead of only once).  This will be fixed in the next release. The update procedure is documented here: :doc:`firmware_update`


If you're ready for more, FX script has its own guide: :doc:`fx_script_guide`


