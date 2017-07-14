FX Script Guide
===============

FX Script is a scripting language for creating graphics on LED pixels (such as WS2801, APA102, Neopixels, etc).  It is a simple, procedural language that uses Python syntax and runs on a custom designed virtual machine.

The language is designed for simplicity.  This makes it easy to learn and easy to run on resource constrained hardware (low power microcontrollers with less than 10 KB of RAM).  The virtual machine has a built in graphics system and optimizes common operations for pixel graphics to allow it to run as fast as possible.

The memory constraints and speed optimizations mean the language must be limited in many respects.  Object oriented programming, advanced data structures, exception handling, strings, floating point math, and even recursion are not supported.  The goal is to enable rapid development of pixel graphics for small to moderately sized projects (generally 300 pixels or less). If you need full 3D graphics across 20,000 pixels, FX Script is probably not the tool for you.  However, if you want to do live code updates on more modest projects, FX Script can dramatically improve productivity.


Installation
------------

The FX Script compiler is included with the chromatron Python package, which can be installed via pip:

.. code:: bash

    $ pip install chromatron


Usage
-----

The compiler is invoked via the command line:

.. code:: bash

    $ chromatron compile script_name.fx

By convention, source files use a .fx extension and the compiled binary uses .fxb.


Compile and load a script to a device:

.. code:: bash

    $ chromatron vm load script_name.fx


Live mode:
This will set up the command to recompile and reload the script file every time it is changed.

.. code:: bash

    $ chromatron vm load script_name.fx --live


See :doc:`cli` for more information on how to use the command line interface.



Example
-------

We'll start with an annotated example of a rolling rainbow pattern. Afterwards, we'll explain what everything does. This is the FX Script equivalent of "hello world".


.. code-block:: python

    # rainbow.fx

    # this script generates a rolling rainbow pattern

    # declare a global variable for current hue
    current_hue = Number()

    # init - runs once when script is loaded
    def init():
        # set pixels to full colors (maximum saturation)
        pixels.sat = 1.0

        # set to maximum brightness
        pixels.val = 1.0


    # runs periodically, frame rate is configurable
    def loop():
        # increment the base hue so the rainbow pattern
        # shifts across the pixels as we go from one frame
        # to the next.
        current_hue += 0.005

        # declare a local variable
        a = Number()
        a = current_hue

        # loop over all pixels in array
        for i in pixels.count:
            pixels[i].hue = a
            
            # shift color for next pixel.
            # this will distribute the rainbow pattern
            # across the entire array.
            a += 1.0 / pixels.count


Basics
------

FX Script uses Python as the front end language, so the best way to learn FX is to learn some basic Python first, and then come back here.

There are a number of differences from Python:


Program Structure
^^^^^^^^^^^^^^^^^

Each program requires an init() function. init() executes once when the program is loaded.

Most programs will also need a loop() function. loop() is called repeatedly by the virtual machine. The loop frame rate is configurable. See :ref:`frame-rate-reference`.

.. code:: python

    def init():
        # setup code goes here

        pass

    def loop():
        # called repeatedly to generate each frame of your graphics

        pass


Variables
^^^^^^^^^

There is only one basic data type, Number, which is a signed 32 bit integer. Number ranges from \-2,147,483,648 to 2,147,483,647.

Variables must be declared:
    
.. code:: python

    my_variable = Number()

All variables will initialize to 0.


For Loops
^^^^^^^^^

For loops do not uses the range/xrange functions:

.. code:: python
    
    # correct
    for count in my_variable:
        # iterate my_variable number of times.
        # count will start at 0 and increment with each iteration.
        pass

    # wrong
    for count in xrange(my_variable):
        pass


Arrays
^^^^^^

Instead of Python's lists, FX uses fixed length arrays.

.. code:: python

    my_array = Array(4) # declare an array of 4 Numbers

    # you can do this:
    for i in my_array.count:
        temp = Number()
        temp = my_array[i]

    # you cannot do things like this:
    for i in my_array:
        temp = Number()
        temp = i

Floats
^^^^^^

Floating point numbers are a shortcut to represent integer values in the graphics system. They do *not* behave like normal floating point numbers in Python.  Remember - the underlying virtual machine only understands integers.

All internal graphics parameters are represented as 16 bit integers (0 to 65535). However, it is often simpler to represent these values as a floating point number between 0.0 and 1.0. Thus, in FX Script the number 0.5 represents the integer 32768. You can use these special floats in expressions, such as 0.1 + 0.1, but be aware that something like 0.5 * 0.5 may not do what you expect.  Instead of yielding 16384 (0.25), you will actually get 32768 * 32768 = 1,073,741,824.  It is generally best to avoid complex math with the floating point representation.


Graphics System
---------------

HSV Colorspace
^^^^^^^^^^^^^^

All graphics in FX Script use the HSV color space. Although the LEDs themselves (along with our eyes) use RGB, HSV is much simpler to design for specific colors. If you're not familiar with HSV, the `Wikipedia article is a good place to start <https://en.wikipedia.org/wiki/HSL_and_HSV>`_.  However, we'll explain the basics right here.

HSV is shorthand for hue, saturation, value. You could translate it as color, whiteness, brightness.

Hue
"""

Hue is color.  It works as a circle, with the colors of the rainbow spaced out along the edge.  0.0 is the top of the circle, and represents the color red.  1/3 (0.333) is green, 2/3 (0.667) is blue.  Every other color is a blend between these values:

===== ======  ====================
hue   color   16 bit integer value
===== ======  ====================
0.000 red     0
0.167 yellow  10944
0.333 green   21823
0.500 teal    32767
0.667 blue    43711
0.833 purple  54590
1.000 red     0
===== ======  ====================

Note that 1.0 is the same as 0.0, hue wraps around as a circle.  Thus, the value 1.5 would be the same as 0.5, etc.  As you'll see in the rainbow demo, this wraparound behavior is incredibly useful in doing continuous, smooth color shifts.

Sat
"""

Saturation (sat for shorthand) is the whiteness value.  1.0 is full saturation, meaning you get all of the colors of the rainbow.  0.0 is no saturation, which is full white.  The pastel colors are created by selecting a hue, and then adjusting the saturation.


Val
"""

Value (val for shorthand) is brightness.


Pixel Array
^^^^^^^^^^^

This is where the fun begins. The pixel array is how we get HSV values into the pixel strip itself.

There is a master pixel array called pixels.  You can set HSV values on individual pixels like so:

.. code:: python
    
    pixels[0].hue = 0.5 # set pixel 0 to teal
    pixels[0].val = 1.0 # set pixel 0 to max brightness

    pixels[1].val = 0.0 # set pixel 1 to off


If you wanted to turn on all of the pixels in the array, you could do something like this:

.. code:: python

    for i in pixels.count:
        pixels[i].val = 1.0 # set pixel to max brightness


However, this is a pretty common operation, so FX Script has a better way of doing the same thing:


.. code:: python

    pixels.val = 1.0 # set all pixels to max brightness


Since pixels was not indexed, FX knows that you want to write to the entire array. While it's nice that this saves some typing and makes your code look a bit cleaner, the other benefit is that the virtual machine can skip doing a for loop in the VM's bytecode, and instead run an optimized instruction with the loop implemented in C. This means that the second method is much, much faster than the first one.

This technique is not limited to just assignment. You can adjust parameters mathematically as well:

.. code:: python

    pixels.val += 0.1 # increment all pixels' brightness by 0.1


This works with +, -, \*, /, and % operators.

What happens when the pixel val exceeds 1.0? FX understands what hue, sat, and val all mean - it knows they are special. 

In the case of sat and val, FX will automatically limit to a range between 0.0 and 1.0. This means if you attempt to increment val beyond 1.0, it will just stop at 1.0. The same occurs if you try to decrease below 0.0. It knows you can't have a negative brightness, and it also knows you probably don't want to jump from 0.0 to 1.0 when you're using this syntax.

For hue, FX will allow the parameter to wrap around. If hue is 0.99 and you increment by 0.1, hue will end up at 0.09 (0.99 + 0.1 = 1.09, so we wrap to 0.09). This is how doing a rainbow color effect works, you can just increment hue continuously and it will wrap around.  No need to bounds check (unless you want to intentionally limit the color range!).

There are some limitations of course.  For instance, you cannot do an array read:

.. code:: python
    
    if pixels.val == 0.5: # this is not valid
        pass

    temp = Array()
    temp = pixels.val # this is also not valid

The Array() declaration is also wrong because it does not specify the length. The point is, this type of operation is not supported.

This means that you cannot do something like this:

.. code:: python

    pixels.val = pixels.hue # sorry, FX doesn't work this way.

If for some reason you really needed to do something like that, you can always write your own loop.


API
"""

.. automodule:: fx_api
.. autoclass:: PixelArray
    :members:

.. autoclass:: Pixel
    :members:


Dimmers and Faders 
^^^^^^^^^^^^^^^^^^

The graphics system has a number of dimmers and faders built in.

Dimmers
"""""""

There are two global dimmers, called master and sub. Each affects the brightness of all pixels. Additionally, each pixel's val will further modulate the brightness for that pixel. The dimmer values multiply to achieve the final dimming level for each pixel:

pixel_brightness = master * sub * pixel val

For instance, if master is 0.5, sub is also 0.5, and the pixel val is 1.0, the actual dimmed output will be 0.25. If the master, sub, and the pixel val are all 0.5, that pixel will be at 0.125.

The master and sub dimmers are currently only accessible over the network via the CLI and Python APIs. There is no interface from within FX Script to override them.

Check the current dimmer levels:

.. code:: bash

    $ chromatron dimmer show


Set master dimmer to 0.5:

.. code:: bash

    $ chromatron dimmer master 0.5


Set sub dimmer to 0.3:

.. code:: bash

    $ chromatron dimmer sub 0.3


See :doc:`cli` for more information on how to use the command line interface.


Faders
""""""

Each pixel has two time based faders. One fader is shared between hue and sat, the other is used for val. This allows FX to produce smooth timed fades automatically.

The fader system runs every 20 ms, regardless of the frame rate of the FX VM.


.. code:: python
    
    # assumes pixels start fully off

    pixels.v_fade = 1000 # set val fader to 1000 ms fade
    pixels.val = 1.0 # set all pixels to max brightness

This code will instruct each pixel to fade up to max brightness over the course of one second. Note that each time you set val it will recompute the fader (thus resetting the one second timer from wherever val is at the time). If the frame rate is faster than once per second, you can ensure a one second fade by doing something like this:

.. code:: python
    
    # assumes pixels start fully off

    # only start the value fade if the pixels are not currently fading.
    if pixels.is_fading == 0:
        pixels.v_fade = 1000 # set val fader to 1000 ms fade
        pixels.val = 1.0 # set all pixels to max brightness

The hue/sat fader is hs_fade, and operates the same way.



.. _frame-rate-reference:

Frame Rate
^^^^^^^^^^

The frame rate (rate at which the loop() function is called) is adjustable via the Python API and CLI. There is currently no interface within FX to adjust it.

Check the current frame rate with:

.. code:: bash

    $ chromatron keys get gfx_frame_rate


Set the frame rate with:

.. code:: bash

    $ chromatron keys set gfx_frame_rate 100

This will set the frame rate to 100 ms, or 10 times per second. The range is adjustable from 10 ms to 65535 ms.


See :doc:`cli` for more information on how to use the command line interface.



Performance Notes
-----------------

The FX VM is designed to be as fast as possible, but the fact is that a VM incurs some overhead that would normally be done by hardware in machine code. The processor has a tremendous amount of work to do: process the FX script itself, run faders over 300 LEDs with 3 information channels each 50 times per second (this is 45,000 channel updates per second), do 15,000 HSV to RGB conversions per second, and also manage the Wifi connection.

You can check the performance of your script with the following command:

.. code:: bash

    $ chromatron keys get vm_loop_time

This will return the number of microseconds your loop function takes to execute.

You can also check the timing of the fader system:

.. code:: bash

    $ chromatron keys get vm_fade_time


For reference, with 300 pixels and running the default rainbow script, the faders will run in 800 to 900 microseconds and the script loop itself around 1,000 microseconds.

If your script is running slow, there are a few things you can do to help.

Try lowering the frame rate.  If you are using a high frame rate to achieve smooth fades, consider that the fader system is already trying to do this for you
.

Make sure you use array operations where possible, they are much faster than looping in the script.

Be aware that the compiler only does *very* basic optimizations. For instance, it will optimize expressions that only involve constants:

.. code:: python

    a = Number()
    a = 1 + 2 + 3 

The compiler will evaluate this expression and just assign 6 to variable a.

However, it currently does not do more sophisticated optimizations:

.. code:: python
    
    a = Number()

    for i in pixels.count:
        pixels[i].hue = a

        a += 1.0 / pixels.count

In this case, the 1.0 / pixels.count is computed *every* iteration. A faster way is this:

.. code:: python
    
    a = Number()
    b = Number()
    b = 1.0 / pixels.count

    for i in pixels.count:
        pixels[i].hue = a

        a += b

Just that change in the rainbow script saves almost 400 microseconds.


Finally, note that memory is also constrained. The FX compiler will impose a limit of 128 variables (which includes all of the elements in arrays). The .fxb file format is a somewhat self-contained VM image, so you can use the file size as a reference for how much memory (code and data) your script uses.  The rainbow demo is less than 200 bytes. The VM itself has 4096 bytes allocated, however, a script that actually uses that much memory is unlikely to perform well at high pixel counts.
