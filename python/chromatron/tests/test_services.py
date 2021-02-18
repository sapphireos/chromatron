import time
import pytest
import logging
from catbus import *
from sapphire.protocols.services import ServiceManager, ServiceNotConnected, STATE_LISTEN, STATE_CONNECTED, STATE_SERVER
from sapphire.devices.device import Device

from fixtures import *


class DeviceService(object):
    def __init__(self, host=NETWORK_ADDR):
        d = Device(host=host)
        d.scan()
        self.device = d

        self.host = self.device.host

        self.reset()

    def reset(self):
        self.device.set_key('test_service_mode', 0)
        time.sleep(0.1)

    def __str__(self):
        return f'Service@{self.device.host}'

    @property
    def _service(self):
        return self.device.get_service(1234, 5678)

    @property
    def _state(self):
        return self._service.state

    @property
    def connected(self):
        return self._state != STATE_LISTEN

    @property
    def is_server(self):
        return self._state == STATE_SERVER

    @property
    def server(self):
        if not self.connected:
            raise ServiceNotConnected

        if self.is_server:
            host = ('127.0.0.1', self._port) # local server is on loopback

        else:
            host = (self._best_host[0], self._best_offer.port)
        
        return host

    def wait_until_state(self, state, timeout=60.0):
        logging.debug(f"{self} waiting for state {state}, timeout {timeout}")
        
        while self._state != state:
            time.sleep(0.1)

            if timeout > 0.0:
                timeout -= 0.1

                if timeout < 0.0:
                    raise ServiceNotConnected

    def wait_until_connected(self, timeout=60.0):
        if self.connected:
            return

        logging.debug(f"{self} waiting for connection, timeout {timeout}")
        
        while not self.connected:
            time.sleep(0.1)

            if timeout > 0.0:
                timeout -= 0.1

                if timeout < 0.0:
                    raise ServiceNotConnected

    def listen(self, service_id=1234, group=5678):
        self.device.set_key('test_service_priority', 0)
        self.device.set_key('test_service_mode', 2)
        time.sleep(0.1)

        return self

    def offer(self, service_id=1234, group=5678, priority=99):
        self.device.set_key('test_service_priority', priority)
        self.device.set_key('test_service_mode', 1)
        time.sleep(0.1)

        return self

    def join_team(self, service_id=1234, group=5678, port=1000, priority=99):
        self.device.set_key('test_service_priority', priority)
        self.device.set_key('test_service_mode', 3)
        time.sleep(0.1)

        return self


@pytest.fixture()
def local_idle():
    s = ServiceManager()
    yield s
    s.stop()
    s.join()

@pytest.fixture()
def local_listen():
    s = ServiceManager()
    yield s.listen(1234, 5678)
    s.stop()
    s.join()

@pytest.fixture
def local_offer():
    s = ServiceManager()
    yield s.offer(1234, 5678, 1000, priority=99)
    s.stop()
    s.join()

@pytest.fixture
def local_team():
    s = ServiceManager(origin=1)
    yield s.join_team(1234, 5678, 1000, priority=99)
    s.stop()
    s.join()

@pytest.fixture
def local_team2():
    s = ServiceManager(origin=2)
    yield s.join_team(1234, 5678, 1000, priority=99)
    s.stop()
    s.join()

@pytest.fixture
def local_team_follower():
    s = ServiceManager(origin=3)
    yield s.join_team(1234, 5678, 1000, priority=0)
    s.stop()
    s.join()


@pytest.fixture
def network_idle():
    host = checkout_device()
    d = DeviceService(host)
    yield d
    d.reset()
    checkin_device(host)

@pytest.fixture
def network_listen():
    host = checkout_device()
    d = DeviceService(host).listen()
    yield d
    d.reset()
    checkin_device(host)
    
@pytest.fixture
def network_offer():
    host= checkout_device()
    d = DeviceService(host).offer()
    yield d
    d.reset()
    checkin_device(host)

@pytest.fixture
def network_team():
    host= checkout_device()
    d = DeviceService(host).join_team(priority=99)
    yield d
    d.reset()
    checkin_device(host)

@pytest.fixture
def network_team2():
    host= checkout_device()
    d = DeviceService(host).join_team(priority=99)
    yield d
    d.reset()
    checkin_device(host)

@pytest.fixture
def network_team_follower():
    host= checkout_device()
    d = DeviceService(host).join_team(priority=0)
    yield d
    d.reset()
    checkin_device(host)


idlers = ['local_idle', 'network_idle']
@pytest.fixture(params=idlers)
def idler(request):
    return request.getfixturevalue(request.param)

listeners = ['local_listen', 'network_listen']
@pytest.fixture(params=listeners)
def listen(request):
    return request.getfixturevalue(request.param)

offers = ['local_offer', 'network_offer']
@pytest.fixture(params=offers)
def offer(request):
    return request.getfixturevalue(request.param)

team_leaders = ['local_team', 'network_team']
@pytest.fixture(params=team_leaders)
def team_leader(request):
    return request.getfixturevalue(request.param)

team_leaders2 = ['local_team2', 'network_team2']
@pytest.fixture(params=team_leaders2)
def team_leader2(request):
    return request.getfixturevalue(request.param)

team_followers = ['local_team_follower', 'network_team_follower']
@pytest.fixture(params=team_followers)
def team_follower(request):
    return request.getfixturevalue(request.param)


# @pytest.mark.skip
def test_basic(listen, offer):
    listen.wait_until_state(STATE_CONNECTED)
    offer.wait_until_state(STATE_SERVER)

    assert offer.is_server
    assert offer.connected
    assert listen.connected
    assert not listen.is_server

# @pytest.mark.skip
def test_team(team_leader, team_follower):
    team_leader.wait_until_state(STATE_SERVER)
    team_follower.wait_until_state(STATE_CONNECTED)

    assert team_leader.is_server
    assert not team_follower.is_server
    assert team_follower.connected


# @pytest.mark.skip
def test_team_leader_no_switch(team_leader, idler):
    team_leader.wait_until_state(STATE_SERVER)

    idler = idler.join_team(1234, 5678, 1000, priority=1)

    idler.wait_until_state(STATE_CONNECTED)
    team_leader.wait_until_state(STATE_SERVER)


# @pytest.mark.skip
def test_team_leader_switch(team_leader, idler):
    team_leader.wait_until_state(STATE_SERVER)

    idler = idler.join_team(1234, 5678, 1000, priority=255)

    idler.wait_until_state(STATE_SERVER)
    team_leader.wait_until_state(STATE_CONNECTED)

