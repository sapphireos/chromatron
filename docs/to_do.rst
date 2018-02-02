Documentation To-Do
===================

Chromatron is an extremely powerful system, and the documentation at present only covers the basics.

Here's some things that still need documentation.  Just because the docs aren't finished doesn't mean you can't use advanced features!  If you want to learn more about how something works but don't want to wait for me to finish writing, post a question in the `user group <https://groups.google.com/forum/#!forum/chromatron>`_.


- database linkage to FX script: you can link variables in FX script to the database, and then access them over the network.

- pixel arrays: you can create virtual pixel arrays in an FX script and access them independently.  This is useful if you want to control different sections of a single strip differently (for instance, if you made a light up cube, you could have a separate array defined for each side).

- Accessing the auxiliary IO connections to connect sensors, etc: this will require a firmware update (which is not yet released), but will allow your scripts to interact with the outside world.

- Using the Python API directly: you can integrate Chromatron's drivers into your own projects, stream raw graphics directly to the pixel array, etc.

- Building your own custom firmware: Not for the faint of heart, as Chromatron has about 20,000 lines of operating system that makes it work.  However, it is an extremely powerful system and can be used as a general purpose microcontroller platform.
