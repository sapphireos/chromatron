import time
import pytest
import logging
from catbus import *
from catbus.link import *
from sapphire.devices.device import Device

from fixtures import *


class _TestClient(Client):
    def assert_key(self, key, value, timeout=30.0):
        start = time.time()
        while (time.time() - start) < timeout:
            if self.get_key(key) == value:
                return value
            time.sleep(0.1)

        raise AssertionError(self.get_key(key), value)



@pytest.fixture
def local_device():
    c = CatbusService(tags=['__TEST__'])
    c['link_test_key'] = 0
    c['link_test_key2'] = 0
    yield c
    c.stop()
    c.join()

@pytest.fixture
def local_send(local_device):
    local_device.send('link_test_key', 'link_test_key2', ['__TEST__'], rate=100)
    yield _TestClient(('localhost', local_device._data_port))
    
@pytest.fixture
def local_consumer(local_device):
    yield _TestClient(('localhost', local_device._data_port))



def test_send(local_send, local_consumer):
    local_send.set_key('link_test_key', 123)

    local_consumer.assert_key('link_test_key', 0)
    local_consumer.assert_key('link_test_key2', 123)













@pytest.fixture
def network_target():
    host = checkout_device()
    d = Device(host=host)
    d.scan()
    if d.get_key('test_link') != 2:
        logging.debug("resetting network device...")
        d.set_key('test_link', 2)
        d.reboot()
        d.wait()
        logging.debug("network device ready!")

    d.set_key('kv_test_key', 0)
    d.set_key('link_test_key', 0)
        
    yield (d.host, d.port), CATBUS_LINK_PORT

    checkin_device(host)

@pytest.fixture
def local_target():
    c = CatbusService(tags=['__TEST__'])
    c['kv_test_key'] = 0
    c['link_test_key'] = 0
    yield ('localhost', c._data_port), c._link_manager._port
    c.stop()
    c.join()


link_targets = ['network_target', 'local_target']

@pytest.fixture(params=link_targets)
def link_client(request):
    return _TestClient(request.getfixturevalue(request.param)[0]), request.getfixturevalue(request.param)[1]

@pytest.fixture
def send_link():
    lm = LinkManager(test_mode=False)
    lm._database.add_item('link_test_key', 0, 'int32')
    lm.send('link_test_key', 'kv_test_key', query=['__TEST__'], rate=100)
    yield lm
    lm.stop()
    lm.join()

@pytest.mark.skip
def test_send_link(send_link, link_client):
    link_port = link_client[1]
    link_client = link_client[0]
    assert link_client.get_key('kv_test_key') == 0
    send_link._database['kv_test_key'] = 0
    send_link._database['link_test_key'] = 123

    for i in range(300):
        time.sleep(0.1)

        consumers = [c for c in send_link._consumers.values() if c.host[1] == link_port]
        if len(consumers) > 0:
            break
            
    assert len(send_link._consumers) > 0
    link_client.assert_key('kv_test_key', 123)



