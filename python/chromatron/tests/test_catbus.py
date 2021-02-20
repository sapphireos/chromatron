
import os
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

@pytest.mark.skip
def test_resolve_hash_from_tag(client):
    # check second code path, server looking up a tag
    h = 0x75dd6565
    assert client.lookup_hash(h, skip_cache=True)[0x75dd6565] == '__TEST__'


def test_ping(client):
    assert client.ping() < 2.0


@pytest.fixture
def random_data():
    length = 250000
    test_data = os.urandom(length)
    
    yield test_data

# @pytest.mark.skip
def test_file_io(network_client, random_data):
    client = network_client

    fname = '__TEST__.bin'

    client.write_file(fname, file_data=random_data)

    file_list = client.list_files()
    assert fname in file_list
    assert file_list[fname]['size'] == len(random_data)

    check_data = client.read_file(fname)
    
    assert random_data == check_data

    client.delete_file(fname)

    file_list = client.list_files()
    assert fname not in file_list

