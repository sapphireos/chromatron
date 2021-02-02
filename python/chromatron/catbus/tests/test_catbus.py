
import pytest

import asyncio
from catbus import *
from sapphire.common.msgserver import run_all, stop_all
import threading

@pytest.fixture
def background_async():
    loop = asyncio.get_event_loop()

    # t = threading.Thread(target=loop.run_forever)
    t = threading.Thread(target=run_all)
    t.start()
    print(asyncio.get_event_loop().is_running())
    yield
    # stop_all()
    loop.stop()
    # loop.close()


@pytest.fixture
def server(background_async):
    return CatbusService()
    
@pytest.fixture
def client(server):
    return Client(('localhost', server._data_port))

def test_getkey(client):
    # print(client._connected_host)
    print(asyncio.get_event_loop().is_running())
    # client.get_key('kv_test_key')
