# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2020  Jeremy Billheimer
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

import socket
from .data_structures import *
from .messages import *
from .options import *
from sapphire.common import catbus_string_hash
from sapphire.common.broadcast import send_udp_broadcast
import time
import os
import json

from sapphire.buildtools import firmware_package
DATA_DIR_FILE_PATH = os.path.join(firmware_package.data_dir(), 'catbus_hashes.json')
DATA_DIR_FILE_PATH_ALT = 'catbus_hashes.json'

DISCOVERY_TIMEOUT = 1.0

import random


class Client(object):
    def __init__(self):
        self.__sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        self._connected_host = None

        self.read_window_size = 1
        self.write_window_size = 1

        self.nodes = {}
        self._meta = {}

        try:
            os.makedirs(firmware_package.data_dir())
        except FileExistsError:
            pass
        except PermissionError:
            pass
            
    @property
    def meta(self):
        if len(self._meta) == 0:
            self.get_meta()

        return self._meta

    def _exchange(self, msg, host=None, timeout=1.0, tries=5):
        self.__sock.settimeout(timeout)

        if host == None:
            host = self._connected_host

        i = 0
        while i < tries:
            i += 1

            try:
                # start = time.time()
                # print 'send', type(msg), len(msg.pack())
                self.__sock.sendto(msg.pack(), host)

                while True:
                    data, sender = self.__sock.recvfrom(4096)

                    reply_msg = deserialize(data)
                    # elapsed = time.time() - start
                    # print int(elapsed*1000), 'recv', type(reply_msg), len(data), '\n'

                    if reply_msg.header.transaction_id != msg.header.transaction_id:
                        # bad transaction IDs coming in, this doesn't count
                        # against our retries.
                        i -= 1
                        
                        continue

                    elif isinstance(reply_msg, ErrorMsg):
                        self.flush()
                        raise ProtocolErrorException(reply_msg.error_code, lookup_error_msg(reply_msg.error_code))

                    return reply_msg, sender

            except socket.error:
                pass

        raise NoResponseFromHost(msg.header.msg_type, host)

    def set_window(self, read, write):
        self.read_window_size = read
        self.write_window_size = write

    def flush(self):
        timeout = self.__sock.gettimeout()
        self.__sock.settimeout(0.1)

        while True:
            try:
                data, host = self.__sock.recvfrom(4096)
                
            except socket.timeout:
                break

        # reset timeout
        self.__sock.settimeout(timeout)

    def is_connected(self):
        return self._connected_host != None

    def connect(self, host):
        if isinstance(host, str):
            # if no port is specified, set default
            host = (host, CATBUS_DISCOVERY_PORT)

        self._connected_host = host

    def ping(self):
        msg = DiscoverMsg(flags=CATBUS_DISC_FLAG_QUERY_ALL)

        start = time.time()
        if self._exchange(msg) == None:
            raise NoResponseFromHost

        elapsed = time.time() - start

        return elapsed

    def lookup_hash(self, *args):
        # open cache file
        cache = {}
        try:
            try:
                with open(DATA_DIR_FILE_PATH, 'r') as f:
                    file_data = f.read()

            except PermissionError:
                with open(DATA_DIR_FILE_PATH_ALT, 'r') as f:
                    file_data = f.read()

            temp = json.loads(file_data)

            cache = {}
            # have to convert keys back to int because json only does string keys
            for k, v in temp.items():
                cache[int(k)] = v

        except ValueError as e:
            # if the cache file has an error, or if it is
            # being rewritten and has not fully synced to disk,
            # we get an error here.

            # just bypass and carryon with no cache
            print(e)
            # pass

        except IOError:
            pass

        resolved_keys = {}

        arg_list = []
        for arg in args:
            try:
                arg_list.extend(arg)

            except TypeError:
                arg_list.append(arg)

        # filter out any items in the cache
        for k, v in cache.items():
            if k in arg_list:
                resolved_keys[k] = v

        arg_list = [a for a in arg_list if a not in cache]

        chunks = [arg_list[x:x + CATBUS_MAX_HASH_LOOKUPS] for x in range(0, len(arg_list), CATBUS_MAX_HASH_LOOKUPS)]

        for chunk in chunks:
            hashes = []

            for key in chunk:
                if isinstance(key, str):
                    key = str(key)
                    hashed_key = catbus_string_hash(key)

                else:
                    hashed_key = key

                hashes.append(hashed_key)

            msg = LookupHashMsg(hashes=hashes)

            response, sender = self._exchange(msg)

            for i in range(len(response.keys)):
                key = response.keys[i]
                hashed_key = hashes[i]

                if len(key) == 0:
                    key = None

                resolved_keys[hashed_key] = key


        tags = None

        # check for any keys that didn't resolve
        for k, v in resolved_keys.items():
            if k == 0:
                continue

            if v == None:
                # try computing hash from meta tags
                if tags is None:
                    tags = {catbus_string_hash(v): v for v in self.get_tags().values() if len(v) > 0}
                
                try:
                    resolved_keys[k] = tags[k]

                except KeyError:
                    # can't find anything, just return hash itself
                    resolved_keys[k] = k

        changed = False
        # check if any keys were added
        for k in resolved_keys:
            if k not in cache:
                cache.update(resolved_keys)

                changed = True
                break

        if changed:
            # update cache file, if anything changed
            try:
                with open(DATA_DIR_FILE_PATH, 'w') as f:
                    f.write(json.dumps(cache))

                    # ensure file is committed to disk
                    f.flush()

            except (PermissionError, FileNotFoundError):
                with open(DATA_DIR_FILE_PATH_ALT, 'w') as f:
                    f.write(json.dumps(cache))

                    # ensure file is committed to disk
                    f.flush()

        return resolved_keys

    def get_directory(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            sock.connect(('localhost', CATBUS_DIRECTORY_PORT))

        except ConnectionRefusedError:
             return None

        data = sock.recv(4)
        length = struct.unpack('<L', data)[0]

        buf = bytes()

        while len(buf) < length:
            buf += sock.recv(1024)

        directory = json.loads(buf)

        # convert host from list to tuple for convience in socket functions
        for key, device in directory.items():
            directory[key]['host'] = tuple(device['host'])

        return directory

    def discover(self, *args, **kwargs):
        """Discover KV nodes on the network"""
        self.nodes = {}

        if len(args) > 0:
            tags = [catbus_string_hash(a) for a in args]
            query = CatbusQuery()
            query._value = tags
            msg = DiscoverMsg(query=query)

        else:
            tags = []
            msg = DiscoverMsg(flags=CATBUS_DISC_FLAG_QUERY_ALL)

        # try to contact directory server
        directory = self.get_directory()

        if directory == None:
            # note we're creating a new socket for discovery.
            # the reason to do this is we may get a stray response after
            # we've completed discovery, and that may interfere with
            # other communications (you receive an announce when you
            # were expecting a data message, etc).

            discover_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            discover_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

            discover_sock.settimeout(DISCOVERY_TIMEOUT)

            for i in range(3):
                send_udp_broadcast(discover_sock, CATBUS_DISCOVERY_PORT, msg.pack())

                start = time.time()

                while (time.time() - start) < DISCOVERY_TIMEOUT:
                    try:
                        data, sender = discover_sock.recvfrom(1024)

                        response = deserialize(data)
                        
                        if not isinstance(response, AnnounceMsg):
                            continue

                        if response.header.transaction_id == msg.header.transaction_id and \
                            query_tags(tags, response.query):

                            # add to node list
                            self.nodes[response.header.origin_id] = \
                                {'host': (sender[0], response.data_port),
                                 'tags': response.query}

                    except InvalidMessageException as e:
                        logging.exception(e)

                    except socket.timeout:
                        pass

            discover_sock.close()
        
        else:
            for origin_id, v in directory.items():
                if query_tags(tags, v['tags']):
                    self.nodes[origin_id] = \
                                    {'host': tuple(v['host']),
                                     'tags': v['tags']}

        return self.nodes

    def get_tags(self):
        return self.get_keys(META_TAGS)

    def get_meta(self):
        msg = GetKeyMetaMsg(page=0)

        responses = []

        # get first page
        response, sender = self._exchange(msg)
        responses.extend(response.meta)

        page = 0
        page_count = response.page_count

        # get additional pages
        while page_count > 1:
            page_count -= 1
            page += 1

            msg = GetKeyMetaMsg(page=page)

            response, sender = self._exchange(msg)
            responses.extend(response.meta)

        meta = {}

        keys = self.lookup_hash([a.hash for a in responses])
                
        for response in responses:
            if response.hash != 0:
                meta[keys[response.hash]] = response


        self._meta = meta

        return meta

    def get_all_keys(self):
        return self.get_keys(list(self.meta.keys()))

    def get_keys(self, *args, **kwargs):
        with_meta = False
        if 'with_meta' in kwargs and kwargs['with_meta']:
            with_meta = True

        key_list = []
        for arg in args:
            if isinstance(arg, str):
                key_list.append(arg)

            else:
                try:
                    key_list.extend(arg)

                except TypeError:
                    key_list.append(arg)

        # convert unicode to ascii
        key_list = [str(k) for k in key_list]

        # check if we have these keys
        # we're doing this because on older versions of catbus,
        # requesting a key that does not exist may return garbage.
        # key_list = [k for k in key_list if k in self.meta]
        # disabled this code path, this is legacy support - 
        # i don't think i have any more of these in the field.

        hashes = {}
        for key in key_list:
            hashes[catbus_string_hash(key)] = key

        answers = {}

        while len(hashes) > 0:
            request_list = list(hashes.keys())[:CATBUS_MAX_GET_KEY_ITEM_COUNT]

            msg = GetKeysMsg(hashes=request_list)

            try:
                response, sender = self._exchange(msg)
                
                if len(response.data) == 0:
                    raise KeyError

                for item in response.data:
                    key = hashes[item.meta.hash]

                    if with_meta:
                        answers[key] = item

                    else:
                        answers[key] = item.value

                    del hashes[item.meta.hash]

            except InvalidMessageException:
                print("Invalid message received")
                return answers

        return answers

    def get_key(self, key):
        try:
            return self.get_keys(key)[key]

        except KeyError:
            raise KeyError(key)

    def set_keys(self, **kwargs):
        key_data = kwargs

        # convert unicode to ascii AND
        # check if we have these keys
        # we're doing this because on older versions of catbus,
        # requesting a key that does not exist may return garbage.
        key_data = {str(k):v for k,v in key_data.items() if k in self.meta}

        # need to get meta data so we can pack
        meta = self.get_keys(list(key_data.keys()), with_meta=True)
        
        # create data items
        items = []
        batches = []
        for key in key_data:
            if isinstance(key_data[key], str):
                # we do not support unicode, and things break if leaks into the system
                key_data[key] = str(key_data[key])

            if key_data[key] == '__null__':
                key_data[key] = ''

            data = CatbusData(meta=meta[key].meta, value=key_data[key])

            items.append(data)

            batches.append(CatbusDataArray())
            
        # filter into batches
        for batch in batches:
            for item in items:
                if batch.size() + item.size() < CATBUS_MAX_DATA:
                    batch.append(item)                    

            for item in batch:
                items.remove(item)

        # filter out batches which are empty
        batches = [batch for batch in batches if len(batch) > 0]

        hashes = {}
        for key in key_data.keys():
            hashes[catbus_string_hash(key)] = key

        answers = {}

        # send each batch
        for batch in batches:
            msg = SetKeysMsg(data=batch)
            
            response, sender = self._exchange(msg)
            
            for item in response.data:
                answers[hashes[item.meta.hash]] = item.value

        return answers


    def set_key(self, key, value):
        return self.set_keys(**{key: value})[key]

    def check_file(self, filename):
        msg = FileCheckMsg(
                filename=filename)

        response, host = self._exchange(msg, timeout=4.0)

        if not isinstance(response, FileCheckResponseMsg):
            raise ProtocolErrorException

        return {'hash': response.hash, 'length': response.file_len}

    def delete_file(self, filename):
        msg = FileDeleteMsg(
                filename=filename)

        try:
            response, host = self._exchange(msg, timeout=4.0)

        except ProtocolErrorException as e:
            if e.error_code != CATBUS_ERROR_FILE_NOT_FOUND:
                raise


    def read_file(self, filename, progress=None):
        file_data = bytes()

        msg = FileOpenMsg(
                flags=CATBUS_MSG_FILE_FLAG_READ,
                filename=filename,
                offset=0,
                data_len=0)

        i = 3
        while i > 0:
            try:
                response, host = self._exchange(msg)

                if not isinstance(response, FileConfirmMsg):
                    raise ProtocolErrorException

                break

            except ProtocolErrorException as e:
                # if file system is busy, we'll try again
                if e.error_code == CATBUS_ERROR_FILESYSTEM_BUSY:
                    time.sleep(0.2)

                # check if file was not found
                elif e.error_code == CATBUS_ERROR_FILE_NOT_FOUND:
                    raise IOError("File %s not found" % (filename))

                else:
                    raise

            i -= 1

        if i == 0:
            raise ProtocolErrorException
                    
        session_id = response.session_id
        requested_offset = 0
        ack_offset = 0
        page_size = response.page_size

        self.__sock.settimeout(0.5)

        max_window = self.read_window_size
        current_window = max_window

        while True:
            while current_window > 0:
                msg = FileGetMsg(session_id=session_id, offset=requested_offset)
                self.__sock.sendto(msg.pack(), host)  

                requested_offset += page_size

                current_window -= 1

            try:
                data, sender = self.__sock.recvfrom(4096)
                data_msg = deserialize(data)

                if isinstance(data_msg, ErrorMsg):
                    self.flush()
                    raise ProtocolErrorException(data_msg.error_code, lookup_error_msg(data_msg.error_code))

                elif not isinstance(data_msg, FileDataMsg):
                    self.flush()
                    raise ProtocolErrorException("invalid message")

                if data_msg.session_id != session_id:
                    continue

                if data_msg.offset == ack_offset:
                    ack_offset += data_msg.len

                    file_data += data_msg.data

                    if progress:
                        progress(len(file_data))

                    # check if end of file
                    if data_msg.len == 0:
                        break

                    if max_window < self.read_window_size:
                        max_window += 1

                    if current_window < max_window:
                        current_window += 1

            except socket.error:
                requested_offset = ack_offset
                current_window = 1
                max_window = 1


        # close session
        msg = FileCloseMsg(session_id=session_id)
        self.__sock.sendto(msg.pack(), host)

        # f = open(filename, 'w')
        # f.write(file_data)
        # f.close()

        return file_data

    def write_file(self, filename, file_data=None, progress=None, delete_existing=True, use_percent=False):
        if file_data is None:
            f = open(filename, 'r')
            file_data = f.read()
            f.close()

        if delete_existing:
            # delete existing file first
            self.delete_file(filename)


        msg = FileOpenMsg(
                flags=CATBUS_MSG_FILE_FLAG_WRITE,
                filename=filename,
                offset=0,
                data_len=len(file_data))

        i = 3
        while i > 0:
            i -= 1
            try:
                response, host = self._exchange(msg)

                if not isinstance(response, FileConfirmMsg):
                    raise ProtocolErrorException

                break

            except ProtocolErrorException as e:
                # if file system is busy, we'll try again
                if e.error_code == CATBUS_ERROR_FILESYSTEM_BUSY:
                    time.sleep(0.2)

                else:
                    raise

        session_id = response.session_id
        offset = 0        
        page_size = response.page_size
        
        while offset < len(file_data):
            write_len = page_size

            # check for end of file
            data_remaining = len(file_data) - offset
            if write_len > data_remaining:
                write_len = data_remaining

            assert write_len > 0

            data = file_data[offset:offset + write_len]
            assert len(data) == write_len

            msg = FileDataMsg(session_id=session_id, offset=offset, len=len(data), data=data)

            reply_msg, host = self._exchange(msg, timeout=1.0)

            offset += write_len

            if not isinstance(reply_msg, FileGetMsg):
                raise ProtocolErrorException(CATBUS_ERROR_UNKNOWN_MSG, "invalid message: %s" % (str(reply_msg)))

            if reply_msg.session_id != session_id:
                raise InvalidMessageException("Invalid session ID")

            if reply_msg.offset != offset:
                raise InvalidMessageException("Invalid file offset")                

            if progress:
                if use_percent:
                    progress((offset / len(file_data)) *100.0)
                else:
                    progress(offset)

        # close session
        msg = FileCloseMsg(session_id=session_id)
        self.__sock.sendto(msg.pack(), host)

        return file_data

    def list_files(self):
        msg = FileListMsg(index=0)

        responses = []

        # get first page
        response, sender = self._exchange(msg)
        responses.extend(response.meta)

        index = response.next_index
        file_count = response.file_count

        while len(responses) < file_count:
            msg = FileListMsg(index=index)

            response, sender = self._exchange(msg)
            responses.extend(response.meta)

            index = response.next_index


        d = {}

        for response in responses:
            if response.filesize < 0:
                continue

            d[response.filename] = {'size': response.filesize, 'flags': response.flags, 'filename': response.filename}

        return d

    def get_links(self):
        index = 0

        exc = None

        links = []

        while True:
            msg = LinkGetMsg(index=index)
            index += 1

            try:
                response, host = self._exchange(msg)

            except ProtocolErrorException as e:
                exc = e
                break

            # check flags
            if response.flags & CATBUS_LINK_FLAGS_VALID == 0:
                continue

            if response.flags & CATBUS_MSG_LINK_FLAGS_SOURCE:
                source = True
            else:
                source = False

            # look up hashes    
            resolved_keys = self.lookup_hash(response.source_hash, response.dest_hash, response.tag, *response.query)

            link = {
                "source": source,
                "source_key": resolved_keys[response.source_hash],
                "dest_key": resolved_keys[response.dest_hash],
                "query": [resolved_keys[a] for a in response.query],
                "tag": resolved_keys[response.tag]
            }

            links.append(link)


        if exc.error_code != CATBUS_ERROR_LINK_EOF:
            raise exc

        return links

    def add_link(self, source, source_key, dest_key, query, tag):
        if source:
            flags = CATBUS_MSG_LINK_FLAGS_SOURCE

        else:
            flags = 0

        flags |= CATBUS_LINK_FLAGS_VALID

        msg = LinkAddMsg(
                flags=flags,
                source_key=source_key,
                dest_key=dest_key,
                query=query,
                tag=tag)

        response, host = self._exchange(msg)

    def delete_link(self, tag):
        msg = LinkDeleteMsg(tag=catbus_string_hash(tag))

        response, host = self._exchange(msg)


if __name__ == '__main__':

    import sys
    from pprint import pprint
    
    c = Client()

    c.discover()

    for node in c.discover().values():
        pprint(node)

    # c.connect(('10.0.0.119', 44632))
    # c.connect(('10.0.0.102', 44632))
    # pprint(c.get_links())

    # c.add_link(True, "wifi_rssi", "wifi_rssi", ["datalog"], "datalog")
    # c.add_link(True, "supply_voltage", "supply_voltage", ["datalog"], "datalog")

    # c.delete_link("test")

    # pprint(c.get_links())

    # print c.get_key('test_meow')

    # c.set_key('test_meow', '4')

    # print c.get_key('test_woof')
    # c.set_key('test_woof', ['4','5','6','7'])

    # print(c.get_key('fx_a'))





