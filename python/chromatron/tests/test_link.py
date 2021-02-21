import time
import pytest
import logging
from catbus import *
from catbus.link import *

from fixtures import *


class _TestClient(Client):
    def assert_key(self, key, value, timeout=30.0):
        start = time.time()
        while (time.time() - start) < timeout:
            if self.get_key(key) == value:
                return value
            time.sleep(0.1)

        raise AssertionError(self.get_key(key), value)

class _Device(_TestClient):
    def __init__(self, host=NETWORK_ADDR):
        super().__init__((host, CATBUS_MAIN_PORT))
        self.reset()

    def reset(self):
        self.set_key('test_link_mode', 0)
        time.sleep(0.5)

    def __str__(self):
        return f'Link@{self.device.host}'

    def send(self):
        self.set_key('test_link_mode', 2)

    def consume(self):
        self.set_key('test_link_mode', 3)



@pytest.fixture
def local_sender():
    c = CatbusService(tags=['__TEST__'])
    c.send('link_test_key', 'link_test_key2', ['__TEST__'], rate=100)
    yield _TestClient(('localhost', c._data_port))
    c.stop()
    c.join()

@pytest.fixture
def network_sender():
    host= checkout_device()
    c = _Device(host)
    c.send()
    yield c
    c.reset()
    checkin_device(host)


@pytest.fixture
def local_consumer():
    c = CatbusService(tags=['__TEST__'])
    yield _TestClient(('localhost', c._data_port))
    c.stop()
    c.join()

@pytest.fixture
def network_consumer():
    host= checkout_device()
    c = _Device(host)
    c.consume()
    yield c
    c.reset()
    checkin_device(host)



senders = ['local_sender', 'network_sender']
@pytest.fixture(params=senders)
def sender(request):
    return request.getfixturevalue(request.param)

consumers = ['local_consumer', 'network_consumer']
@pytest.fixture(params=consumers)
def consumer(request):
    return request.getfixturevalue(request.param)


def test_send(sender, consumer):
    for v in [123, 456, 999]:
        sender.set_key('link_test_key', v)

        consumer.assert_key('link_test_key', 0)
        consumer.assert_key('link_test_key2', v)
        sender.assert_key('link_test_key', v)
        sender.assert_key('link_test_key2', 0)















# @pytest.fixture
# def network_target():
#     host = checkout_device()
#     d = Device(host=host)
#     d.scan()
#     if d.get_key('test_link') != 2:
#         logging.debug("resetting network device...")
#         d.set_key('test_link', 2)
#         d.reboot()
#         d.wait()
#         logging.debug("network device ready!")

#     d.set_key('kv_test_key', 0)
#     d.set_key('link_test_key', 0)
        
#     yield (d.host, d.port), CATBUS_LINK_PORT

#     checkin_device(host)

# @pytest.fixture
# def local_target():
#     c = CatbusService(tags=['__TEST__'])
#     c['kv_test_key'] = 0
#     c['link_test_key'] = 0
#     yield ('localhost', c._data_port), c._link_manager._port
#     c.stop()
#     c.join()


# link_targets = ['network_target', 'local_target']

# @pytest.fixture(params=link_targets)
# def link_client(request):
#     return _TestClient(request.getfixturevalue(request.param)[0]), request.getfixturevalue(request.param)[1]

# @pytest.fixture
# def send_link():
#     lm = LinkManager(test_mode=False)
#     lm._database.add_item('link_test_key', 0, 'int32')
#     lm.send('link_test_key', 'kv_test_key', query=['__TEST__'], rate=100)
#     yield lm
#     lm.stop()
#     lm.join()

# @pytest.mark.skip
# def test_send_link(send_link, link_client):
#     link_port = link_client[1]
#     link_client = link_client[0]
#     assert link_client.get_key('kv_test_key') == 0
#     send_link._database['kv_test_key'] = 0
#     send_link._database['link_test_key'] = 123

#     for i in range(300):
#         time.sleep(0.1)

#         consumers = [c for c in send_link._consumers.values() if c.host[1] == link_port]
#         if len(consumers) > 0:
#             break
            
#     assert len(send_link._consumers) > 0
#     link_client.assert_key('kv_test_key', 123)



