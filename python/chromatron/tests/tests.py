import unittest

from catbus import *
from catbus.database import Database
from sapphire.common import catbus_string_hash
from catbus.options import CATBUS_MAIN_PORT


class DatabaseTests(unittest.TestCase):
    def setUp(self):
        self.database = Database(name='test_name', location='test_location', tags=['test_tag'])

    def test_meta(self):
        self.assertEqual(self.database['meta_tag_name'], 'test_name')
        self.assertEqual(self.database['meta_tag_location'], 'test_location')

    def test_query(self):
        self.assertTrue(self.database.query(*[catbus_string_hash(a) for a in ('test_name',)]))
        self.assertTrue(self.database.query(*[catbus_string_hash(a) for a in ('test_location',)]))
        self.assertTrue(self.database.query(*[catbus_string_hash(a) for a in ('test_tag',)]))
        self.assertTrue(self.database.query(*[catbus_string_hash(a) for a in ('test_tag', 'test_name')]))
        self.assertFalse(self.database.query(*[catbus_string_hash(a) for a in ('test_tag', 'test_name2')]))

    def add_item(self):
        self.database['test_item'] = 123
        self.assertEqual(self.database['test_item'], 123)

class DiscoverTestBase(object):
    def test_discover(self):
        nodes = list(self.client.discover(self.CATBUS_TEST_TAG).values())
        self.assertEqual(len(nodes), 1)

@unittest.skip('')
class DiscoverTestsLocal(unittest.TestCase, DiscoverTestBase):
    CATBUS_TEST_TAG = '___CATBUS_UNIT_TEST____'

    def setUp(self):
        self.catbus = CatbusService(tags=[self.CATBUS_TEST_TAG])
        self.client = Client()

    def tearDown(self):
        self.catbus.stop()

@unittest.skip('')
class DiscoverTestsRemote(unittest.TestCase, DiscoverTestBase):
    CATBUS_TEST_TAG = '__catbus_remote_test__'

    def setUp(self):
        self.client = Client()


class ProtocolTestBase(object):
    def test_ping(self):
        self.assertTrue(self.client.ping() < 0.5)

    def test_lookup_hash(self):
        self.assertEqual(self.client.lookup_hash(catbus_string_hash('device_id')), ['device_id'])
        self.assertEqual(self.client.lookup_hash(catbus_string_hash('device_id'), catbus_string_hash('kv_test_key')), ['device_id', 'kv_test_key'])

    def test_get_meta(self):
        self.assertTrue('meta_tag_name' in self.client.get_meta())

    def test_get(self):
        self.assertEqual(self.client.get_key('meta_tag_name'), 'catbus')

    def test_set(self):
        self.client.set_key('kv_test_key', 123)
        self.assertEqual(self.client.get_key('kv_test_key'), 123)


class ProtocolTestsLocal(unittest.TestCase, ProtocolTestBase):
    CATBUS_TEST_TAG = '___CATBUS_UNIT_TEST____'

    def setUp(self):
        self.catbus = CatbusService(name='catbus', location='test_location', tags=[self.CATBUS_TEST_TAG])
        self.client = Client()
        self.client.connect(('localhost', self.catbus._data_port))

    def tearDown(self):
        self.catbus.stop()

class ProtocolTestsRemote(unittest.TestCase, ProtocolTestBase):
    CATBUS_TEST_TAG = '__catbus_remote_test__'

    def setUp(self):
        self.client = Client()
        self.client.connect(('10.0.0.117', CATBUS_MAIN_PORT))


