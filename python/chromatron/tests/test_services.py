import time
import pytest
import logging
from catbus import *
from sapphire.protocols.services import ServiceManager, STATE_CONNECTED, STATE_SERVER
from sapphire.devices.device import Device

from fixtures import *


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


def network_device(mode):
	d = Device(host=NETWORK_ADDR)
	d.scan()
	if d.get_key('test_services') != mode:
		logging.debug(f"resetting network device for mode {mode}...")
		d.set_key('test_services', mode)
		d.reboot()
		d.wait()
		logging.debug("network device ready!")
		
	return d

@pytest.fixture
def network_offer():
	return network_device(1)

@pytest.fixture
def network_listen():
	return network_device(2)
	
@pytest.fixture
def network_team():
	return network_device(3)


# @pytest.mark.skip
def test_basic_service(listen_service, offer_service):
	listen_service.wait_until_connected(timeout=60.0)
	offer_service.wait_until_connected(timeout=60.0)

	assert offer_service.is_server
	assert offer_service.connected
	assert listen_service.connected
	assert not listen_service.is_server

# @pytest.mark.skip
def test_basic_team(team_service1, team_service2):
	team_service1.wait_until_connected(timeout=60.0)
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
	listen_service.wait_until_connected(timeout=60.0)

	assert listen_service.connected
	assert not listen_service.is_server
	assert listen_service.server[0] == network_offer.host

# @pytest.mark.skip
def test_network_offer(offer_service, network_listen):
	offer_service.wait_until_connected(timeout=60.0)

	s = network_listen.wait_service(1234, 5678)
	assert s
	assert s.state == STATE_CONNECTED

# @pytest.mark.skip
def test_network_team(team_service1, network_team):
	team_service1.wait_until_state(STATE_SERVER, timeout=60.0)

	s = network_team.wait_service(1234, 5678)
	assert s
	assert s.state == STATE_CONNECTED

	assert team_service1.connected
	assert team_service1.is_server


def test_network_team_local_follower(team_service_follower, network_team):
	s = network_team.wait_service(1234, 5678)
	assert s
	assert s.state == STATE_SERVER

	team_service_follower.wait_until_state(STATE_CONNECTED, timeout=60.0)
	assert team_service_follower.connected
	assert not team_service_follower.is_server
	


