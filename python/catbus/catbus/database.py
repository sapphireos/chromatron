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


import threading
from UserDict import DictMixin
import random
import time
import logging


from data_structures import *


DATAFILE_EXT = '.catbusdb'


class Database(DictMixin, object):
    """KV Database that acts like a dict.  Thread-safe."""
    def __init__(self, name=None, location=None, tags=[], datafile='data'):
        super(Database, self).__init__()

        self._kv_items = {}
        self._hashes = {}
        self._lock = threading.RLock()
        self._datafile = datafile

        try:
            self._load_file()

        except IOError:
            pass

        if 'device_id' not in self:
            # set up random device id
            rng = random.SystemRandom()
            device_id = rng.randint(0, pow(2, 64) - 1)

            self.add_item('device_id', device_id, 'uint64', readonly=True, persist=True)

        for item in META_TAGS:
            if item not in self:
                self.add_item(item, '', 'string32', persist=True)

                if item == META_TAG_NAME:
                    # default name
                    self[META_TAG_NAME] = str(int(time.time() * 1000000))

        if name:
            self[META_TAG_NAME] = name

        if location:
            self[META_TAG_LOC] = location

        i = len(META_TAGS) - META_TAG_GROUP_COUNT
        for tag in tags:
            self[META_TAGS[i]] = tag
            i += 1

    def _load_file(self):
        with open(self._datafile + DATAFILE_EXT, 'rb') as f:
            array = CatbusDataArray().unpack(f.read())

            for item in array:  
                self._kv_items[item.key] = item.data
                self._hashes[catbus_string_hash(item.key)] = item.key

    def _persist(self):
        pass
        # items = [CatbusData(data=v, key=k) for k, v in self._kv_items.iteritems() if v.meta.flags & CATBUS_FLAGS_PERSIST]

        # array = CatbusDataArray()
        # array._value = items

        # with open(self._datafile + DATAFILE_EXT, 'wb+') as f:
        #     f.write(array.pack())

    def keys(self):
        """Return list of keys"""
        with self._lock:
            return self._kv_items.keys()

    def get_query(self):
        query = CatbusQuery()
        # this syntax could be cleaned up
        query._value = self.get_hashed_tags()
        return query

    def get_tags(self):
        return [self.get_item(a).value for a in META_TAGS]

    def get_hashed_tags(self):
        # return {catbus_string_hash(tag): tag for tag in self.get_tags()}
        return [catbus_string_hash(self.get_item(a).value) for a in META_TAGS]        

    def query(self, *args):
        tags = self.get_hashed_tags()

        return query_tags(args, tags)

    def infer_default_type(self, value):
        """Get default Sapphire type for value"""
        if isinstance(value, bool):
            return 'bool'

        elif isinstance(value, int):
            return 'int32'

        elif isinstance(value, float):
            return 'float'

        elif isinstance(value, long):
            return 'int64'

        elif isinstance(value, basestring):
            return 'string128'

        else:
            raise TypeError(value)

    def add_item(self, key, value, data_type=None, readonly=False, persist=False):
        """Add new KV item to database"""

        if len(key) > CatbusStringField()._length:
            raise ValueError("Key is too long")

        # get default type if one is not provided
        if data_type == None:
            data_type = self.infer_default_type(value)

        # set up flags
        flags = 0
        if readonly:
            flags |= CATBUS_FLAGS_READ_ONLY

        if persist:
            flags |= CATBUS_FLAGS_PERSIST

        with self._lock:
            # check if key is already in database
            if key in self._kv_items:
                raise KeyError('Key already present')

            # add new item
            hashed_key = catbus_string_hash(key)
            meta = CatbusMeta(hash=hashed_key, flags=flags, type=data_type)
            kv_item = CatbusData(meta=meta, value=value)
            self._kv_items[key] = kv_item
            self._hashes[hashed_key] = key

        # notify subclass
        self._item_notify(key, value)

        if persist:
            self._persist()

    def _get_item_no_lock(self, key):
        try:
            return self._kv_items[key]

        except KeyError:
            return self._kv_items[self._hashes[key]]

    def get_item(self, key):
        with self._lock:
            return self._get_item_no_lock(key)

    def lookup_hash(self, hashed_key):
        return self._hashes[hashed_key]

    # item set notification for subclass
    def _item_notify(self, key, value):
        pass

    def __getitem__(self, key):
        with self._lock:
            return self._get_item_no_lock(key).value

    def __setitem__(self, key, value):
        with self._lock:
            # get key if hash
            if key in self._hashes:
                key = self._hashes[key]

            found = key in self._kv_items

        # can set from key or hash
        if found:
            with self._lock:
                try:
                    self._kv_items[key].value = value

                except TypeError:
                    self._kv_items[key].value = value._value

                if self._kv_items[key].meta.flags & CATBUS_FLAGS_PERSIST:
                    self._persist()

            # publish
            self._item_notify(key, value)

        # adding a new item
        # this is a "quick and dirty" way to add keys.
        # you get default types and options only.
        # this requires an actual string key
        else:
            assert isinstance(key, basestring)
            self.add_item(key, value)

