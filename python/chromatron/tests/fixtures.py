
import pytest

from catbus import *
from sapphire.common.util import setup_basic_logging
from sapphire.common.msgserver import stop_all
import logging

setup_basic_logging(level=logging.DEBUG)

NETWORK_ADDR = '10.0.0.156'

@pytest.fixture
def local_server():
    c = CatbusService(tags=['__TEST__'])
    yield ('localhost', c._data_port)
    c.stop()
    c.join()

@pytest.fixture
def network_server():
    return (NETWORK_ADDR, CATBUS_MAIN_PORT)

servers = ['local_server', 'network_server']

@pytest.fixture(params=servers)
def client(request):
    # return Client(('localhost', server._data_port))
    # return request.getfixturevalue(request.param)
    return Client(request.getfixturevalue(request.param))

@pytest.fixture
def service_manager():
    return ServiceManager()

