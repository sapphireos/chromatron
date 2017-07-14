Key-value System, Meta Tags, and Queries
========================================

Chromatron's operating system is built around an embedded key-value database. A basic key-value database is simply a mapping between a string and some value, and some interface to read and write those values.

The key-value database is central to how Chromatron works. Almost all of its configuration data (things like which Wifi network to connect to, dimmer settings, etc) and much of its run time information (such as Wifi signal strength, input voltage, etc) is stored there.  Most of the time when you're changing a setting on Chromatron through the Python API or command line tools, you're accessing the database.

The database has eight items which are special, called "meta tags". Each of these tags is a string, up to 32 characters long. By convention, the first tag is the device name and the second tag is the device location. The other six have no special meaning. These meta tags are used to find and identify devices in a way which is more descriptive than just using a single name, IP address, or some other device ID number.

A query is a listing of up to 8 strings that are compared against the meta tags. They work as a keyword search, so the order of strings in the meta tags or the query doesn't matter. For instance, if you query for two tags, 'King Arthur' and 'Camelot', you'll retrieve any devices that have both of those strings in their meta tags. The positional order does not matter. This makes it easy to compose multiple overlapping groups of devices.

