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

        d.set_key('test_service_mode', 0)
        time.sleep(0.1)

        self.device = d
        self.host = self.device.host

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


    def wait_until_state(self, state, timeout=0.0):
        logging.debug(f"{self} waiting for state {state}, timeout {timeout}")
        
        while self._state != state:
            time.sleep(0.1)

            if timeout > 0.0:
                timeout -= 0.1

                if timeout < 0.0:
                    raise ServiceNotConnected

    def wait_until_connected(self, timeout=0.0):
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

    def join_team(self, service_id=1234, group=5678, priority=99):
        self.device.set_key('test_service_priority', priority)
        self.device.set_key('test_service_mode', 3)
        time.sleep(0.1)

        return self


@pytest.fixture()
def listen_service():
    s = ServiceManager()
    yield s.listen(1234, 5678)
    s.stop()
    s.join()

@pytest.fixture
def offer_service():
    s = ServiceManager()
    yield s.offer(1234, 5678, 1000, priority=99)
    s.stop()
    s.join()

@pytest.fixture
def team_service1():
    s = ServiceManager()
    yield s.join_team(1234, 5678, 1000, priority=99)
    s.stop()
    s.join()

@pytest.fixture
def team_service2():
    s = ServiceManager()
    yield s.join_team(1234, 5678, 1000, priority=98)
    s.stop()
    s.join()


@pytest.fixture
def team_service_follower():
    s = ServiceManager()
    yield s.join_team(1234, 5678, 1000, priority=0)
    s.stop()
    s.join()


@pytest.fixture
def network_offer():
    return DeviceService().offer()

@pytest.fixture
def network_listen():
    return DeviceService().listen()
    
@pytest.fixture
def network_team():
    return DeviceService().join_team()

@pytest.fixture
def network_team_follower():
    return DeviceService().join_team(priority=0)


@pytest.mark.skip
def test_basic_service(listen_service, offer_service):
    listen_service.wait_until_connected(timeout=60.0)
    offer_service.wait_until_connected(timeout=60.0)

    assert offer_service.is_server
    assert offer_service.connected
    assert listen_service.connected
    assert not listen_service.is_server

@pytest.mark.skip
def test_basic_team(team_service1, team_service2):
    team_service1.wait_until_state(STATE_SERVER, timeout=60.0)
    # wait for first service to set to server.
    # team_service1 should have the higher priority.
    assert team_service1.is_server
    assert team_service1.connected

    team_service2.wait_until_state(STATE_CONNECTED, timeout=60.0)

    assert team_service1.is_server
    assert team_service1.connected
    assert not team_service2.is_server
    assert team_service2.connected

# @pytest.mark.skip
def test_network_listen(listen_service, network_offer):
    listen_service.wait_until_state(STATE_CONNECTED, timeout=60.0)
    network_offer.wait_until_state(STATE_SERVER, timeout=60.0)

    assert listen_service.connected
    assert not listen_service.is_server
    assert listen_service.server[0] == network_offer.host
    assert network_offer.is_server

# @pytest.mark.skip
def test_network_offer(offer_service, network_listen):
    offer_service.wait_until_state(STATE_SERVER, timeout=60.0)
    network_listen.wait_until_state(STATE_CONNECTED, timeout=60.0)

    assert offer_service.is_server
    assert not network_listen.is_server
    assert network_listen.connected

# @pytest.mark.skip
def test_network_team(team_service1, network_team):
    team_service1.wait_until_state(STATE_SERVER, timeout=60.0)
    network_team.wait_until_state(STATE_CONNECTED, timeout=60.0)

    assert network_team.connected
    assert not network_team.is_server
    assert team_service1.connected
    assert team_service1.is_server

# @pytest.mark.skip
def test_network_team_local_follower(team_service_follower, network_team):
    network_team.wait_until_state(STATE_SERVER, timeout=60.0)
    assert network_team.is_server

    team_service_follower.wait_until_state(STATE_CONNECTED, timeout=60.0)
    assert team_service_follower.connected
    assert not team_service_follower.is_server
    assert network_team.is_server
