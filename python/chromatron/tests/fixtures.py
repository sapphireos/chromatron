
import pytest

import asyncio
from catbus import *
from sapphire.common.util import setup_basic_logging
from sapphire.common.msgserver import run_all, stop_all
import threading


@pytest.fixture
def background_async():
    # setup_basic_logging()

    # create a new event loop
    loop = asyncio.new_event_loop()

    # set the new loop
    asyncio.set_event_loop(loop)

    # start a thread to run our async servers in the background
    t = threading.Thread(target=run_all, args=(loop,))
    t.start()
    
    # yield to tests
    yield

    # tell the loop to stop
    loop.stop()

    # wait for background thread to complete
    t.join()


@pytest.fixture
def server(background_async):
    return CatbusService(tags=['__TEST__'])
    
@pytest.fixture
def client(server):
    return Client(('localhost', server._data_port))
