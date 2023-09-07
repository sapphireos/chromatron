
import pytest

import chromatron
from catbus import *
from sapphire.common.util import setup_basic_logging
from sapphire.common.msgserver import stop_all
from sapphire.common import catbus_string_hash
import logging
from pprint import pprint

setup_basic_logging(level=logging.INFO)

NETWORK_ADDR = '10.0.0.143'

NETWORK_DEVICES = [NETWORK_ADDR, '10.0.0.160', '10.0.0.161']

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


NETWORK_CT = None

@pytest.fixture
def network_device():
    global NETWORK_CT

    if NETWORK_CT is None:
        NETWORK_CT = chromatron.Chromatron(host=NETWORK_ADDR)

    return NETWORK_CT


from chromatron.ir2 import OptPasses


TEST_OPT_PASSES = [
    OptPasses.SSA, 
    # OptPasses.GVN, 
    # OptPasses.LOOP, 
    # OptPasses.LS_SCHED,
    # [OptPasses.GVN, OptPasses.LOOP],
    # [OptPasses.GVN, OptPasses.LS_SCHED],
    # [OptPasses.LOOP, OptPasses.LS_SCHED],
    # [OptPasses.GVN, OptPasses.LS_SCHED, OptPasses.LOOP],
]