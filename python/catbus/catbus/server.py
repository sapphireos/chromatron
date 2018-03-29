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


import logging
import time
import threading
import select
from client import Client

from messages import *

from sapphire.common import Ribbon, MsgQueueEmptyException


class Link(object):
    def __init__(self,
                 source=False,
                 source_key=None,
                 source_hash=None,
                 dest_key=None,
                 dest_hash=None,
                 query=None,
                 tags=None,
                 callback=None):

        self.source = source

        self.source_key = source_key
        if source_key:
            self.source_hash = catbus_string_hash(source_key)
        else:
            self.source_hash = source_hash

        self.dest_key = dest_key
        if dest_key:
            self.dest_hash = catbus_string_hash(dest_key)
        else:
            self.dest_hash = dest_hash

        self.query = query
        if self.query != None:
            self.tags = [catbus_string_hash(a) for a in query]

        else:
            self.tags = tags

        self.callback = callback

    def __str__(self):
        s = "Link: "

        s += '-'

        if self.source:
            s += 'S '

        else:
            s += '- '

        s += 'src:%12d ' % (self.source_hash)
        s += 'dst:%12d ' % (self.dest_hash)
        s += 'query: %s' % (str(self.tags))


        return s

    def to_dict(self):
        d = {
            'source': self.source,
            'source_key': self.source_key,
            'source_hash': self.source_hash,
            'dest_key': self.dest_key,
            'dest_hash': self.dest_hash,
            'query': self.query,
            'tags': self.tags,
        }

        return d

    def from_dict(self, d):
        self.source = d['source']
        self.source_key = d['source_key']
        self.source_hash = d['source_hash']
        self.dest_key = d['dest_key']
        self.dest_hash = d['dest_hash']
        self.query = d['query']
        self.tags = d['tags']

        return self

    def __eq__(self, other):
        if self.source != other.source:
            return False

        if self.source_hash != other.source_hash:
            return False

        if self.dest_hash != other.dest_hash:
            return False

        if self.tags and other.tags:
            if not query_compare(self.tags, other.tags):
                return False


        return True


class Publisher(Ribbon):
    def initialize(self, server=None):
        self._server = server
        self.link_data_transmissions = {}
        self.name = "Publisher"

    def loop(self):
        timeout = None

        if len(self.link_data_transmissions) > 0:
            timeout = 0.0

        try:
            msgs = self.recv_all_msgs(timeout=timeout)

        except MsgQueueEmptyException:
            msgs = []

        data_msgs = []

        # get data messages, and go ahead and send anything that isn't Link Data.
        for msg in msgs:
            if isinstance(msg[0], LinkDataMsg):
                data_msgs.append(msg)

            else:
                self._server._send_data_msg(msg[0], msg[1])

        # sort messages by host and target key
        for msg, host in data_msgs:
            if host not in self.link_data_transmissions:
                self.link_data_transmissions[host] = {}

            self.link_data_transmissions[host][msg.dest_hash] = msg

        # print self.link_data_transmissions
        for host in self.link_data_transmissions.keys():
            msgs = self.link_data_transmissions[host]

            # get a random message for this host and send it
            target = random.choice(msgs.keys())
            msg = msgs[target]

            self._server._send_data_msg(msg, host)

            # don't forget to remove message!
            del msgs[target]

            # check if there are any messages left
            if len(msgs) == 0:
                del self.link_data_transmissions[host]

        self.wait(0.02)


class Server(Ribbon):
    def initialize(self, data_port=None, database=None, visible=True):
        self._database = database

        self.name = '%s.%s' % (self._database[META_TAG_NAME], 'server')

        self.visible = visible

        self.__announce_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__announce_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__announce_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__announce_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self.__announce_sock.setblocking(0)
        self.__announce_sock.bind(('', CATBUS_DISCOVERY_PORT))

        self.__data_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__data_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__data_sock.setblocking(0)

        if data_port:
            self.__data_sock.bind(('', data_port))

        else:
            self.__data_sock.bind(('', 0))

        self._data_port = self.__data_sock.getsockname()[1]
        self._host = self.__data_sock.getsockname()

        logging.info("Data server: %d" % (self._data_port))

        self._inputs = [self.__announce_sock, self.__data_sock]

        self._links = []
        self._send_list = []
        self._receive_cache = {}
        self._sequences = {}
        self._hash_lookup = {}

        self._database.add_item('uptime', 0, 'uint32', readonly=True)

        self._msg_handlers = {
            ErrorMsg: self._handle_error,
            DiscoverMsg: self._handle_discover,
            AnnounceMsg: self._handle_announce,
            LookupHashMsg: self._handle_lookup_hash,
            ResolvedHashMsg: self._handle_resolved_hash,
            GetKeyMetaMsg: self._handle_get_key_meta,
            GetKeysMsg: self._handle_get_keys,
            SetKeysMsg: self._handle_set_keys,
            LinkMsg: self._handle_link,
            LinkDataMsg: self._handle_link_data,
        }

        self._last_announce = time.time() - 10.0

        self._publisher = Publisher(server=self)

        self._origin_id = self._database['device_id']

        self.__lock = threading.Lock()

    def send(self, source_key=None, dest_key=None, dest_query=[]):
        link = Link(source=True,
                    source_key=source_key,
                    dest_key=dest_key,
                    query=dest_query)

        with self.__lock:
            if link not in self._links:
                self._links.append(link)

    def receive(self, dest_key=None, source_key=None, source_query=[], callback=None):
        link = Link(source=False,
                    source_key=source_key,
                    dest_key=dest_key,
                    query=source_query,
                    callback=callback)

        try:
            self._database.add_item(dest_key, 0, data_type='int32')

        except KeyError: # key already exists
            pass

        with self.__lock:
            if link not in self._links:
                self._links.append(link)

    def resolve_hash(self, hashed_key, host=None):
        if hashed_key == 0:
            return ''

        with self.__lock:
            try:
                return self._hash_lookup[hashed_key]

            except KeyError:
                if host:
                    c = Client()
                    c.connect(host)

                    key = c.lookup_hash(hashed_key)[0]

                    if len(key) == 0:
                        raise KeyError(hashed_key)

                    self._hash_lookup[hashed_key] = key

                    return key

                else:
                    raise

    def _publish(self, key, _lock=True, _inc_sequence=True):
        if _lock:
            self.__lock.acquire()

        if isinstance(key, basestring):
            key_hash = catbus_string_hash(key)

        else:
            key_hash = key

        # check for source links
        try:
            # check if we have the source key
            if key_hash not in self._database:
                return

            if key_hash not in self._sequences:
                self._sequences[key_hash] = random.randint(0, 65535)

            if _inc_sequence:
                self._sequences[key_hash] += 1
                self._sequences[key_hash] %= 65535


            links = [link for link in self._links if
                    link.source and
                    link.source_hash == key_hash]

            # setup timestamp
            seconds, fraction = util.datetime_to_ntp(util.now())
            ntp_timestamp = NTPTimestampField(seconds=seconds, fraction=fraction)

            source_query = self._database.get_query()

            # check send list
            senders = [a for a in self._send_list if a['source_hash'] == key_hash]

            for sender in senders:
                msg = LinkDataMsg(
                        flags=CATBUS_MSG_DATA_FLAG_TIME_SYNC,
                        ntp_timestamp=ntp_timestamp,
                        source_query=source_query,
                        source_hash=key_hash,
                        dest_hash=sender['dest_hash'],
                        data=self._database[key],
                        sequence=self._sequences[key_hash])

                self._publisher.post_msg((msg, sender['host']))

        finally:
            if _lock:
                self.__lock.release()

    def _send_data_msg(self, msg, host):
        msg.header.origin_id = self._origin_id
        s = self.__data_sock
        s.sendto(serialize(msg), host)

    def _send_announce_msg(self, msg, host=('<broadcast>', CATBUS_DISCOVERY_PORT)):
        msg.header.origin_id = self._origin_id
        s = self.__announce_sock
        s.sendto(serialize(msg), host)

    def _send_announce(self, host=('<broadcast>', CATBUS_DISCOVERY_PORT), discovery_id=None):
        msg = AnnounceMsg(
                data_port=self._data_port,
                query=self._database.get_query())

        if discovery_id:
            msg.header.transaction_id = discovery_id

        self._send_announce_msg(msg, host)

    def _handle_error(self, msg, host):
        print msg

    def _handle_discover(self, msg, host):
        if self.visible:
            if (self._database.query(*msg.query) or (msg.flags & CATBUS_DISC_FLAG_QUERY_ALL)):
                self._send_announce(host=host, discovery_id=msg.header.transaction_id)

    def _handle_announce(self, msg, host):
        pass

    def _handle_lookup_hash(self, msg, host):
        resolved_hashes = []
        for requested_hash in msg.hashes:
            if requested_hash in self._database:
                resolved_hashes.append(CatbusStringField(self._database.lookup_hash(requested_hash)))

        return ResolvedHashMsg(keys=resolved_hashes)

    def _handle_resolved_hash(self, msg, host):
        pass
    
    def _handle_get_key_meta(self, msg, host):
        keys = sorted(self._database.keys())
        key_count = len(keys)
        index = msg.page * CATBUS_MAX_KEY_META
        page_count = (key_count / CATBUS_MAX_KEY_META ) + 1
        item_count = key_count - index

        if item_count < 0:
            raise GenericProtocolException

        if item_count > CATBUS_MAX_KEY_META:
            item_count = CATBUS_MAX_KEY_META
    
        meta = []
        for i in xrange(item_count):
            key = keys[i + index]
            item = self._database.get_item(key)
            meta.append(item.meta)

        reply_msg = KeyMetaMsg(page=msg.page, page_count=page_count, item_count=item_count, meta=meta)

        return reply_msg

    def _handle_get_keys(self, msg, host):
        if msg.count == 0:
            raise GenericProtocolException

        items = []

        for hashed_key in msg.hashes:
            items.append(self._database.get_item(hashed_key))

        reply_msg = KeyDataMsg(data=items)

        return reply_msg

    def _handle_set_keys(self, msg, host):
        reply_items = []

        for item in msg.data:
            try:
                db_item = self._database.get_item(item.meta.hash)

            except KeyError:
                raise GenericProtocolException

            # check types
            if db_item.meta.type != item.meta.type:
                raise GenericProtocolException

            # check read only flag
            if db_item.meta.flags & CATBUS_FLAGS_READ_ONLY:
                raise GenericProtocolException

            # set data
            self._database[item.meta.hash] = item.value

            # retrieve updated copy of item
            reply_items.append(self._database.get_item(item.meta.hash))

        reply_msg = KeyDataMsg(data=reply_items)

        return reply_msg

    def _handle_link(self, msg, host):
        # check flags
        if (msg.flags & CATBUS_MSG_LINK_FLAGS_DEST) == 0:
            # check query
            if not self._database.query(*msg.query):
                return
        
        # check link type
        

        # source link
        if msg.flags & CATBUS_MSG_LINK_FLAG_SOURCE:
            # check if we have dest key
            if msg.dest_hash not in self._database:
                return

            # change link flags and echo message back to sender
            reply_msg = LinkMsg(flags=CATBUS_MSG_LINK_FLAGS_DEST,
                                source_hash=msg.source_hash,
                                dest_hash=msg.dest_hash,
                                query=msg.query)
            reply_msg.header.transaction_id = msg.header.transaction_id

            return reply_msg

        # receiver link
        else:
            # check if we have the source key
            if msg.source_hash not in self._database:
                return

            # remove current entries for this host
            self._send_list = [a for a in self._send_list if 
                                (a['host'] != host) and
                                (a['dest_hash'] != msg.dest_hash) and
                                (a['source_hash'] != msg.source_hash)]

            # add to sender list
            entry = {'host': host, 
                     'dest_hash': msg.dest_hash, 
                     'source_hash': msg.source_hash,
                     'ttl': 32.0}

            self._send_list.append(entry)

    def _handle_link_data(self, msg, host):
        # setup timestamp
        if (msg.flags & CATBUS_MSG_LINK_FLAGS_DEST) == 0:
            timestamp = util.now()
        else:
            timestamp = util.ntp_to_datetime(msg.ntp_timestamp.seconds, msg.ntp_timestamp.fraction)

        try:
            item = self._database.get_item(msg.dest_hash)

        except KeyError:
            return

        # check read only flag
        if item.meta.flags & CATBUS_FLAGS_READ_ONLY:
            return

        # check current receive cache
        try:
            if self._receive_cache[msg.dest_hash][host]['sequence'] != msg.sequence:
                # set data
                self._database[msg.dest_hash] = msg.data

        except KeyError:
            # set data
            self._database[msg.dest_hash] = msg.data

        with self.__lock:
            if msg.dest_hash not in self._receive_cache:
                self._receive_cache[msg.dest_hash] = {}

            self._receive_cache[msg.dest_hash][host] = {
                'ttl': 32,
                'data': msg.data,
                'sequence': msg.sequence
            }

        # get matching links
        with self.__lock:
            links = [l for l in self._links if l.source_hash == msg.source_hash and query_tags(l.tags, msg.source_query)]

        for link in links:
            if link.callback:
                source_query = []
                for hashed_key in msg.source_query:
                    source_query.append(self.resolve_hash(hashed_key, host))

                link.callback(link.source_key, msg.data, source_query, timestamp)


    def _process_msg(self, msg, host):
        response = self._msg_handlers[type(msg)](msg, host)

        return response

    def loop(self):
        try:
            readable, writable, exceptional = select.select(self._inputs, [], [], 1.0)

            for s in readable:
                try:
                    data, host = s.recvfrom(1024)

                    msg = deserialize(data)
                    response = None

                    # filter our own messages
                    try:
                        if msg.header.origin_id == self._origin_id:
                            continue

                        if host[1] == self._data_port:
                            continue

                    except KeyError:
                        pass

                    response = self._process_msg(msg, host)

                    if response:
                        response.header.transaction_id = msg.header.transaction_id
                        self._send_data_msg(response, host)

                except UnknownMessageException as e:
                    pass

                except Exception as e:
                    logging.exception(e)


        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)


        if time.time() - self._last_announce > 4.0:
            self._last_announce = time.time()

            self._database['uptime'] += 4

            if self.visible:
                self._send_announce()

            # broadcast links
            with self.__lock:
                for link in self._links:
                    link_flags = 0

                    if link.source:
                        link_flags = CATBUS_MSG_LINK_FLAG_SOURCE
                    
                    query = CatbusQuery()
                    query._value = link.tags

                    msg = LinkMsg(
                            msg_flags=0,
                            flags=link_flags,
                            source_hash=link.source_hash,
                            dest_hash=link.dest_hash,
                            query=query)
                    
                    self._send_data_msg(msg, ('<broadcast>', CATBUS_DISCOVERY_PORT))

                # prune senders
                for a in self._send_list:
                    a['ttl'] -= 4.0

                self._send_list = [a for a in self._send_list if a['ttl'] >= 0.0]

                # auto publish all source data
                for send in self._send_list:
                    self._publish(send['source_hash'], _lock=False, _inc_sequence=False)

                # print util.now()
                # for link in self._links:
                #     print link

                for i in self._receive_cache.keys():
                    for j in self._receive_cache[i].keys():
                        self._receive_cache[i][j]['ttl'] -= 4

                        if self._receive_cache[i][j]['ttl'] <= 0:
                            del self._receive_cache[i][j]

                    if len(self._receive_cache[i]) == 0:
                        del self._receive_cache[i]

                # pprint(self._receive_cache)


