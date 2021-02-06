
import pytest

from fixtures import *


def test_getkey(client):
    client.get_key('kv_test_key')

def test_setkey(client):
    client.set_key('kv_test_key', 123)
    assert client.get_key('kv_test_key') == 123

    client.set_key('kv_test_key', 456)
    assert client.get_key('kv_test_key') == 456

def test_resolve_hash(client):
    h = 0x3802fc29
    assert client.lookup_hash(h, skip_cache=True)[0x3802fc29] == 'kv_test_key'

    # check second code path, server looking up a tag
    h = 0x75dd6565
    assert client.lookup_hash(h, skip_cache=True)[0x75dd6565] == '__TEST__'

def test_ping(client):
    assert client.ping() < 2.0



