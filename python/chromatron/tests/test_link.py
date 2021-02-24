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

        assert self.get_key(key) == value

class _Device(_TestClient):
    def __init__(self, host=NETWORK_ADDR):
        super().__init__((host, CATBUS_MAIN_PORT))

        d = Device(host=host)
        d.scan()
        self.device = d

        self.reset()

    def reset(self):
        self.set_key('test_link_mode', 0)
        time.sleep(0.5)
        self.set_key('link_test_key', 0)
        self.set_key('link_test_key2', 0)

    def __str__(self):
        return f'Link@{self.device.host}'

    def send(self):
        self.set_key('test_link_mode', 2)
        time.sleep(0.5)

    def consume(self):
        self.set_key('test_link_mode', 3)
        time.sleep(0.5)

    def receive(self):
        self.set_key('test_link_mode', 4)
        time.sleep(0.5)

    def produce(self):
        self.set_key('test_link_mode', 5)
        time.sleep(0.5)



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
def local_receiver():
    c = CatbusService(tags=['__TEST__'])
    c.receive('link_test_key', 'link_test_key2', ['__TEST__'], rate=100)
    yield _TestClient(('localhost', c._data_port))
    c.stop()
    c.join()

@pytest.fixture
def network_receiver():
    host= checkout_device()
    c = _Device(host)
    c.receive()
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

@pytest.fixture
def local_producer():
    c = CatbusService(tags=['__TEST__'])
    yield _TestClient(('localhost', c._data_port))
    c.stop()
    c.join()

@pytest.fixture
def network_producer():
    host= checkout_device()
    c = _Device(host)
    c.produce()
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

receivers = ['local_receiver', 'network_receiver']
@pytest.fixture(params=receivers)
def receiver(request):
    return request.getfixturevalue(request.param)

producers = ['local_producer', 'network_producer']
@pytest.fixture(params=producers)
def producer(request):
    return request.getfixturevalue(request.param)

# @pytest.mark.skip
def test_send(sender, consumer):
    for v in [123, 456, 999]:
        sender.set_key('link_test_key', v)

        consumer.assert_key('link_test_key', 0)
        consumer.assert_key('link_test_key2', v)
        sender.assert_key('link_test_key', v)
        sender.assert_key('link_test_key2', 0)


# @pytest.mark.skip
def test_receive(receiver, producer):
    for v in [123, 456, 999]:
        producer.set_key('link_test_key', v)    

        receiver.assert_key('link_test_key', 0)
        receiver.assert_key('link_test_key2', v)
        producer.assert_key('link_test_key', v)
        producer.assert_key('link_test_key2', 0)



