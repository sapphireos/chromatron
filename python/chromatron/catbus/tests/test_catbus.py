
import pytest

import asyncio
from catbus import *
from sapphire.common.util import setup_basic_logging
from sapphire.common.msgserver import run_all, stop_all
import threading

@pytest.fixture
def background_async():
    setup_basic_logging()
    loop = asyncio.get_event_loop()

    t = threading.Thread(target=loop.run_forever)
    # t = threading.Thread(target=run_all)
    t.start()
    
    yield

    stop_all()
    loop.stop()


@pytest.fixture
def server(background_async):
    return CatbusService()
    
@pytest.fixture
def client(server):
    return Client(('localhost', server._data_port))

def test_getkey(client):
    client.get_key('kv_test_key')
