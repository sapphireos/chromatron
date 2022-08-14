# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>
#

"""

Catbus - Real-time Key-Value Messaging That's Not Built to Scale


Serverless
Low latency (UDP messaging)


Things that don't scale:

Message sized limited to 548 bytes
32 byte keys
Designed to run well on a system with 8K of RAM
No IPv6 support
32 bit hash for keys

"""

import socket
import time
import sys
import os

from sapphire.protocols import services
from sapphire.common import catbus_string_hash, run_all, synchronized
from sapphire.common.util import setup_basic_logging

from .database import *
from .server import *
from .client import *
from .link import *


import click

NAME_COLOR = 'cyan'
HOST_COLOR = 'magenta'
KEY_COLOR = 'green'
VAL_COLOR = 'white'



class CatbusService(Database):
    def __init__(self, data_port=None, visible=True, **kwargs):
        super(CatbusService, self).__init__(**kwargs)

        self._server = Server(data_port=data_port, database=self, visible=visible)
        self._service_manager = services.ServiceManager()
        self._link_manager = LinkManager(database=self, service_manager=self._service_manager)
        
        self._data_port = self._server._port

        self._callbacks = {}

        self.default_callback = None

        self._server._default_callback = self._default_callback

        self.add_item('process_name', sys.argv[0], 'string64', readonly=True)
        self.add_item('process_id', os.getpid(), 'uint32', readonly=True)
        self.add_item('kv_test_key', os.getpid(), 'uint32')
        self.add_item('kv_test_array', [0,1,2,3], 'uint32')

    def _default_callback(self, key, value, query, timestamp):    
        if self.default_callback != None:
            self.default_callback(key, value, query, timestamp)

    def _item_notify(self, key, value):
        try:
            if key in self._callbacks:
                self._callbacks[key](key, value)

        except AttributeError:
            pass

    # @property
    # def _link_manager(self):
    #     # automatically create link manager if it is accessed.
    #     # this way if the link manager isn't used, resources aren't created for it.
    #     with self._lock:
    #         if self.__link_manager is None:
    #             self.__link_manager = LinkManager(database=self, service_manager=self._service_manager)

    #     return self.__link_manager

    # @property
    # def _service_manager(self):
    #     # automatically create service manager if it is accessed.
    #     # this way if the service manager isn't used, resources aren't created for it.
    #     with self._lock:
    #         if self.__service_manager is None:
    #             self.__service_manager = services.ServiceManager()

    #     return self.__service_manager

    @synchronized
    def register(self, key, callback):
        self._callbacks[key] = callback

    def register_announce_handler(self, handler):
        self._server.register_announce_handler(handler)

    def register_shutdown_handler(self, handler):
        self._server.register_shutdown_handler(handler)

    def start_timer(self, *args, **kwargs):
        self._server.start_timer(*args, **kwargs)

    def join_team(self, *args, **kwargs):
        return self._service_manager.join_team(*args, **kwargs)

    def offer(self, *args, **kwargs):
        return self._service_manager.offer(*args, **kwargs)

    def listen(self, *args, **kwargs):
        return self._service_manager.listen(*args, **kwargs)

    def send(self, *args, **kwargs):
        return self._link_manager.send(*args, **kwargs)

    def receive(self, *args, **kwargs):
        return self._link_manager.receive(*args, **kwargs)

    def subscribe(self, *args, **kwargs):
        return self._link_manager.subscribe(*args, **kwargs)

    def publish(self, *args, **kwargs):
        return self._link_manager.publish(*args, **kwargs)

    def stop(self):
        self._server.stop()
        self._link_manager.stop()
     
    def join(self):
        self._server.join()
        self._link_manager.join()


def echo_name(name, host, left_align=True, nl=True):
    if left_align:
        name_s = '%-32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))

    else:
        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))

    click.echo(name_s, nl=nl)


@click.group()
@click.option('--query', '-q', default=None, multiple=True, help="Query for tag match. Type 'all' to retrieve all matches.")
@click.option('--verbose', '-v', default=False, help="Verbose logging")
@click.pass_context
def cli(ctx, query, verbose):
    """Catbus Key-Value System CLI"""

    client = Client()
    ctx.obj['CLIENT'] = client

    verbose = False
    ctx.obj['VERBOSE'] = verbose
    if verbose:
        setup_basic_logging()

    if ctx.invoked_subcommand not in ['hash', 'directory']:
        if query == None:
            query = ('all')

        ctx.obj['QUERY'] = query

        matches = client.discover(*query)

        ctx.obj['MATCHES'] = matches


@cli.command()
@click.pass_context
def discover(ctx):
    """Discover Catbus nodes on the network"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    click.echo('Found %d nodes' % (len(matches)))

    s = 'Name                                        Location                Tags'
    click.echo(s)

    nodes = []

    for node in matches.values():
        host = node['host']

        client.connect(host)

        name, location, *tags = client.get_keys(META_TAGS).values()
        nodes.append((name, location, host, tags))

    for name, location, host, tags in sorted(nodes, key=lambda a: a[0]):
        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))

        location_s = click.style('%16s' % (location), fg=VAL_COLOR)

        s = '%s %-32s %s %s %s %s %s %s' % \
            (name_s, location_s, tags[0], tags[1], tags[2], tags[3], tags[4], tags[5])

        click.echo(s)

@cli.command()
@click.option('--name', '-n', default=None, help="Server name")
@click.option('--location', '-l', default=None, help="Server location")
@click.option('--tag', '-t', default=None, multiple=True, help="Query tags for server")
@click.pass_context
def server(ctx, name, location, tag):
    """Run a test server"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']
    
    kv = CatbusService(name=name, location=location, tags=tag)
    
    run_all()


@cli.command()
@click.pass_context
def list(ctx):
    """List keys"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.values():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)

        echo_name(name, host)

        keys = client.get_all_keys()

        for k, val in keys.items():
            s = '%42s %42s' % (click.style('%s' % (k), fg=KEY_COLOR), click.style('%s' % (val), fg=VAL_COLOR))

            click.echo(s)

@cli.command()
@click.pass_context
def meta(ctx):
    """Dump key meta data"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.values():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)

        echo_name(name, host)

        for k, val in client.meta.items():
            s = '%42s %42s' % (click.style('%s' % (k), fg=KEY_COLOR), click.style('%s' % (val), fg=VAL_COLOR))

            click.echo(s)


@cli.command()
@click.pass_context
def ping(ctx):
    """Ping nodes"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.values():
        host = node['host']

        client.connect(host)

        name = "unknown"

        try:
            start = time.time()
            name = client.get_key(META_TAG_NAME)
            elapsed = int((time.time() - start) * 1000)

            val_s = click.style('%3d ms' % (elapsed), fg=VAL_COLOR)

        except socket.timeout:
            val_s = click.style('No response', fg=VAL_COLOR)

        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))


        s = '%s %s' % (name_s, val_s)

        click.echo(s)


@cli.command()
@click.pass_context
@click.argument('key')
def get(ctx, key):
    """Get key"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.values():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)
        val = client.get_key(key)

        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))
        val_s = click.style('%s' % (val), fg=VAL_COLOR)

        click.echo('%s %s' % (name_s, val_s))


@cli.command()
@click.pass_context
@click.argument('key')
@click.argument('value')
def set(ctx, key, value):
    """Set key"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.values():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)
        client.set_key(key, value)
        val = client.get_key(key)

        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))
        val_s = click.style('%s' % (val), fg=VAL_COLOR)

        click.echo('%s %s' % (name_s, val_s))

# @cli.command()
# @click.pass_context
# @click.argument('key')
# def listen(ctx, key):
#     """Stream updates from a key"""
#     client = ctx.obj['CLIENT']
#     matches = ctx.obj['MATCHES']
#     query = ctx.obj['QUERY']

#     kv = CatbusService()
#     kv[key] = 0

#     def callback(key, value, query, timestamp):
#         name = query[0]
#         location = query[1]

#         click.echo('%s %24s %12d %24s @ %24s' % (timestamp, key, value, name, location))

#     kv.receive(key, key, query, callback)

#     try:
#         while True:
#             time.sleep(1.0)

#     except KeyboardInterrupt:
#         pass


@cli.command()
@click.argument('key')
def hash(key):
    """Print hash for a key"""

    hashed_key = catbus_string_hash(key)

    click.echo('%d 0x%08x' % (hashed_key, hashed_key & 0xffffffff))


@cli.command()
@click.pass_context
def directory(ctx):
    """Query directory server"""
    client = ctx.obj['CLIENT']

    d = client.get_directory()

    if d is None:
        print("Directory not running")
        return

    click.echo('Found %d nodes' % (len(d)))

    s = 'Name                                        TTL  Tags'
    click.echo(s)

    for node in sorted(d.values(), key=lambda node: node['name']):
        host = node['host']

        name = node['name']
        tags = node['query']

        name_s = '%32s @ %20s:%5s %3s' % \
            (click.style('%s' % (name), fg=NAME_COLOR), 
             click.style('%s' % (host[0]), fg=HOST_COLOR), 
             click.style('%5d' % (host[1]), fg=HOST_COLOR),
             click.style('%3d' % (node['ttl']), fg=KEY_COLOR))

        s = name_s

        for tag in tags:
            s += ' %s' % (tag)

        click.echo(s)

def main():
    cli(obj={})

    
