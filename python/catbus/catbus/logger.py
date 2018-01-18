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

import time
from datetime import datetime, timedelta
import sqlite3
from client import Client
from data_structures import META_TAGS, catbus_string_hash, query_tags
import sapphire.common.util as util
from fnvhash import fnv1a_64
from sapphire.common import Ribbon, MsgQueueEmptyException


class Recorder(Ribbon):
    def initialize(self, database="database.db"):
        self.database = database

        self.db_conn = sqlite3.connect(self.database, check_same_thread=False)
        self.db_cursor = self.db_conn.cursor()
        
    def loop(self):
        try:
            msgs = self.recv_all_msgs(timeout=1.0)

        except MsgQueueEmptyException:
            msgs = []

        if len(msgs) > 0:
            self.db_cursor.executemany('''INSERT INTO data VALUES (?,?,?,?)''', (msgs))
            self.db_conn.commit()


class Logger(object):
    def __init__(self, database="database.db"):
        super(Logger, self).__init__()

        self.database = database
        self.db_conn = sqlite3.connect(self.database)
        self.db_cursor = self.db_conn.cursor()

        self.db_cursor.execute('''CREATE TABLE IF NOT EXISTS tags (tag_hash, name, location, tag2, tag3, tag4, tag5, tag6, tag7)''')
        self.db_cursor.execute('''CREATE TABLE IF NOT EXISTS data (tag_hash, key, value, timestamp)''')
        self.db_cursor.execute('''CREATE TABLE IF NOT EXISTS keys (key, hash)''')
        self.db_conn.commit()

        self.tags = {}
        for meta in self.db_cursor.execute('''SELECT * from tags''').fetchall():
            self.tags[meta[0]] = meta[1:]

        self.keys = {}
        for data in self.db_cursor.execute('''SELECT * from keys''').fetchall():
            self.keys[data[1]] = data[0]

        self._recorder = Recorder(database=self.database)

    def _hash_tags(self, tags):
        hashes = []
        for tag in tags:
            assert isinstance(tag, basestring)
            hashes.append((fnv1a_64(tag)))

        return sum(hashes) % ((2**63) - 1)

    def _insert_tags(self, tags):
        while len(tags) != len(META_TAGS):
            tags.append('')

        hashed_tags = self._hash_tags(tags)

        if hashed_tags not in self.tags:
            self.tags[hashed_tags] = tags

            tags.insert(0, hashed_tags)
            
            self.db_cursor.execute('''INSERT INTO tags VALUES (?,?,?,?,?,?,?,?,?)''', tags)
            self.db_conn.commit()

        return hashed_tags

    def _insert_key(self, key):
        hashed_key = catbus_string_hash(key)

        if hashed_key not in self.keys:
            self.keys[hashed_key] = key

            self.db_cursor.execute('''INSERT INTO keys VALUES (?,?)''', (key, hashed_key))
            self.db_conn.commit()

        return hashed_key

    def write(self, key, value, tags, timestamp=None):
        if timestamp is None:
            timestamp = util.now()

        hashed_tags = self._insert_tags(tags)
        hashed_key = self._insert_key(key)

        # convert to microseconds
        microseconds = util.datetime_to_microseconds(timestamp)
        
        self._recorder.post_msg((hashed_tags, hashed_key, value, microseconds))

    def query_tags(self, tags):
        results = []
        for hashed_tags, dest_tags in self.tags.iteritems():
            if query_tags(tags, dest_tags):
                results.append(hashed_tags)

        return results

    def query(self, key, tags, time_start, time_end):
        ms_start = util.datetime_to_microseconds(time_start)
        ms_end = util.datetime_to_microseconds(time_end)

        hashed_key = catbus_string_hash(key)

        results = {}

        for tag in self.query_tags(tags):
            self.db_cursor.execute('''SELECT * from data WHERE timestamp >= ? AND timestamp <= ? AND key == ? AND tag_hash == ?''', (ms_start, ms_end, hashed_key, tag))
            
            results[self.tags[tag]] = {}

            lookup_tag = self.tags[tag]

            for data in self.db_cursor.fetchall():
                key = self.keys[data[1]]
                value = data[2]
                timestamp = util.microseconds_to_datetime(data[3])

                if key not in results[lookup_tag]:
                    results[lookup_tag][key] = []

                results[lookup_tag][key].append((value, timestamp))

        return results


if __name__ == '__main__':

    l = Logger()

    # count = 10
    # start = time.time()

    # for i in xrange(count):
    #     l.write('test_key', i, ['test', 'meow', 'jeremy'])
    #     l.write('stuff', i, ['test', 'meow', 'jeremy'])
    #     l.write('things', i, ['meow'])

    # elapsed = time.time() - start

    # print ''
    # print elapsed
    # print count / elapsed

    # time.sleep(2.0)

    print l.query("things", [], util.now() - timedelta(seconds=6000), util.now())

    # print l.query_tags(['meow'])
    # print l.query_tags(['test', 'meow'])
    # print l.query_tags(['test'])
    # print l.query_tags(['woof'])

