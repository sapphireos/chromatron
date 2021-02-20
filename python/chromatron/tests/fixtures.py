
import pytest

from catbus import *
from sapphire.common.util import setup_basic_logging
from sapphire.common.msgserver import stop_all
import logging

setup_basic_logging(level=logging.DEBUG)

NETWORK_ADDR = '10.0.0.235'

NETWORK_DEVICES = [NETWORK_ADDR, '10.0.0.242']

AVAILABLE = NETWORK_DEVICES

def checkout_device():
	d = AVAILABLE.pop()
	logging.debug(f'Checking OUT device @ {d}')
	return d
	
def checkin_device(d):
	logging.debug(f'Checking IN device @ {d}')
	AVAILABLE.append(d)

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
def network_client(network_server):
    # return Client(('localhost', server._data_port))
    # return request.getfixturevalue(request.param)
    return Client(network_server)

@pytest.fixture
def service_manager():
    return ServiceManager()

