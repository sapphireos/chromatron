
import os
import pytest

from fixtures import *
from chromatron import code_gen, Chromatron

@pytest.mark.skip
def test_getkey(client):
    client.get_key('kv_test_key')

@pytest.mark.skip
def test_setkey(client):
    client.set_key('kv_test_key', 123)
    assert client.get_key('kv_test_key') == 123

    client.set_key('kv_test_key', 456)
    assert client.get_key('kv_test_key') == 456

@pytest.mark.skip
def test_resolve_hash(client):
    h = 0x3802fc29
    assert client.lookup_hash(h, skip_cache=True)[0x3802fc29] == 'kv_test_key'


dynamic_test_program = """
a = Number(publish=True)
b = Number(publish=True)
c = Number(publish=True)

def init():
    pass
"""

# @pytest.mark.skip
def test_resolve_hash_dynamic_key(network_client):
    client = network_client

    ct = Chromatron(host=NETWORK_ADDR)
    builder = code_gen.compile_text(dynamic_test_program, debug_print=False)
    builder.assemble()
    data = builder.generate_binary('test.fxb')

    ct.stop_vm()
    
    # change vm program
    ct.set_key('vm_prog', 'test.fxb')
    ct.put_file('test.fxb', data)
    ct.start_vm()
    
    time.sleep(0.5)

    for k in ['a', 'b', 'c']:
        h = catbus_string_hash(k)

        check_key = client.lookup_hash(h, skip_cache=True)[h]

        assert check_key == k

def test_get_dynamic_key(network_client):
    client = network_client

    ct = Chromatron(host=NETWORK_ADDR)
    builder = code_gen.compile_text(dynamic_test_program, debug_print=False)
    builder.assemble()
    data = builder.generate_binary('test.fxb')

    ct.stop_vm()
    
    # change vm program
    ct.set_key('vm_prog', 'test.fxb')
    ct.put_file('test.fxb', data)
    ct.start_vm()
    
    time.sleep(0.5)

    keys = client.get_all_keys()

    for k in ['a', 'b', 'c']:
        assert k in keys
        assert client.get_key(k) != None


@pytest.mark.skip
def test_resolve_hash_from_tag(client):
    # check second code path, server looking up a tag
    h = 0x75dd6565
    assert client.lookup_hash(h, skip_cache=True)[0x75dd6565] == '__TEST__'

@pytest.mark.skip
def test_ping(client):
    assert client.ping() < 2.0


@pytest.fixture
def random_data():
    length = 250000
    test_data = os.urandom(length)
    
    yield test_data

@pytest.mark.skip
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

