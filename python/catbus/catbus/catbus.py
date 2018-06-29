# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
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

import threading
import socket
import time
import random
import sys
import os
from datetime import datetime, timedelta
from UserDict import DictMixin
import logging

from elysianfields import *
from sapphire.fields import get_field_for_type, get_type_id
from sapphire.common import Ribbon, MsgQueueEmptyException
import sapphire.common.util as util
from sapphire.query import query_dict
import select
import Queue
from pprint import pprint

from data_structures import *
from messages import *
from options import *
from database import *
from server import *
from client import *


import click

NAME_COLOR = 'cyan'
HOST_COLOR = 'magenta'
KEY_COLOR = 'green'
VAL_COLOR = 'white'



class CatbusService(Database):
    def __init__(self, data_port=None, visible=True, **kwargs):
        super(CatbusService, self).__init__(**kwargs)

        self._server = Server(data_port=data_port, database=self, visible=visible)

        self._data_port = self._server._data_port
        self.host = self._server._host

        self._callbacks = {}

        # self.add_item('ip', self.host[0], 'ipv4', readonly=True) # getting source IP is not reliable
        self.add_item('process_name', sys.argv[0], 'string64', readonly=True)
        self.add_item('process_id', os.getpid(), 'uint32', readonly=True)
        self.add_item('kv_test_key', os.getpid(), 'uint32')

    def _item_notify(self, key, value):
        try:
            self._server._publish(key)

            if key in self._callbacks:
                self._callbacks[key](key, value)

        except AttributeError:
            pass

    def _send_msg(self, msg, host):
        self._server.send_data_msg(msg, host)

    def send(self, source_key, dest_key, dest_query=[]):
        self._server.send(source_key, dest_key, dest_query)

    def receive(self, dest_key, source_key, source_query=[], callback=None):
        self._server.receive(dest_key, source_key, source_query, callback)

    def register(self, key, callback):
        self._callbacks[key] = callback

    def stop(self):
        self._server.stop()
        del self._server



def echo_name(name, host, left_align=True, nl=True):
    if left_align:
        name_s = '%-32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))

    else:
        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))

    click.echo(name_s, nl=nl)


@click.group()
@click.option('--query', default=None, multiple=True, help="Query for tag match. Type 'all' to retrieve all matches.")
@click.pass_context
def cli(ctx, query):
    """Catbus Key-Value Pub-Sub System CLI"""

    if ctx.invoked_subcommand != 'hash':
        if query == None:
            query = ('all')

        ctx.obj['QUERY'] = query

        client = Client()

        ctx.obj['CLIENT'] = client

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

    for node in matches.itervalues():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)
        location = client.get_key(META_TAG_LOC)

        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))

        location_s = click.style('%16s' % (location), fg=VAL_COLOR)

        tags = []
        for tag in META_TAGS:
            if tag in [META_TAG_NAME, META_TAG_LOC]:
                continue

            tags.append(client.get_key(tag))

        s = '%s %-32s %s %s %s %s %s %s' % \
            (name_s, location_s, tags[0], tags[1], tags[2], tags[3], tags[4], tags[5])

        click.echo(s)

@cli.command()
@click.pass_context
def server(ctx):
    """Run a test server"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']
    query = ctx.obj['QUERY']

    kv = CatbusService()
    
    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass



@cli.command()
@click.pass_context
def list(ctx):
    """List keys"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.itervalues():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)

        echo_name(name, host)

        keys = client.list_keys()

        for k in keys:
            val = client.get_key(k)

            s = '%42s %42s' % (click.style('%s' % (k), fg=KEY_COLOR), click.style('%s' % (val), fg=VAL_COLOR))

            click.echo(s)


@cli.command()
@click.pass_context
def ping(ctx):
    """Ping nodes"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']

    for node in matches.itervalues():
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

    for node in matches.itervalues():
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

    for node in matches.itervalues():
        host = node['host']

        client.connect(host)

        name = client.get_key(META_TAG_NAME)
        client.set_key(key, value)
        val = client.get_key(key)

        name_s = '%32s @ %20s:%5s' % (click.style('%s' % (name), fg=NAME_COLOR), click.style('%s' % (host[0]), fg=HOST_COLOR), click.style('%5d' % (host[1]), fg=HOST_COLOR))
        val_s = click.style('%s' % (val), fg=VAL_COLOR)

        click.echo('%s %s' % (name_s, val_s))

@cli.command()
@click.pass_context
@click.argument('key')
def listen(ctx, key):
    """Stream updates from a key"""
    client = ctx.obj['CLIENT']
    matches = ctx.obj['MATCHES']
    query = ctx.obj['QUERY']

    kv = CatbusService()
    kv[key] = 0

    def callback(key, value, query, timestamp):
        name = query[0]
        location = query[1]

        click.echo('%s %24s %12d %24s @ %24s' % (timestamp, key, value, name, location))

        # query_format = '%32s, %32s %32s, %32s'

        # query_1 = query_format % (query[0], query[1], query[2], query[3])
        # query_2 = query_format % (query[4], query[5], query[6], query[7])

        # click.echo('%s %32s %12d\n%s\n%s' % (timestamp, key, value, query_1, query_2))
        # print timestamp, key, value, query

        # click.echo('%s %32s %12d' % (timestamp, key, value))
        # click.echo('%s %12d' % (timestamp, value))
        # click.echo('%s %s' % (' ' * 48, query_1))
        # click.echo('%s %s' % (' ' * 48, query_2))

    kv.receive(key, key, query, callback)

    # kv.register(key, callback)

    try:
        while True:
            time.sleep(1.0)
            # print kv[key]

    except KeyboardInterrupt:
        pass


@cli.command()
@click.argument('key')
def hash(key):
    """Print hash for a key"""

    # convert to ASCII
    key = str(key)

    hashed_key = catbus_string_hash(key)

    click.echo('%d 0x%08x' % (hashed_key, hashed_key & 0xffffffff))

    from fnvhash import fnv1a_32
    hashed_key = fnv1a_32(key)

    click.echo('FNV1A: %d 0x%08x' % (hashed_key, hashed_key & 0xffffffff))


def main():
    cli(obj={})


if __name__ == '__main__':

    main()

    # from pprint import pprint

    # kv1 = CatbusService(data_port=11632, tags=['meow'])
    # kv1['woof'] = 0

    # # kv1.receive('woof', 'kv_test_key', ['sender'])
    # # kv1.receive('woof', 'wifi_uptime', ['catbus'])

    # try:
    #     while True:
    #         time.sleep(0.5)
    #         # print kv1['woof']
    #         kv1['woof'] += 1

    # except KeyboardInterrupt:
    #     pass




    #     