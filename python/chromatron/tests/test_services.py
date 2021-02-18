import time
import pytest
import logging
from catbus import *
from sapphire.protocols.services import ServiceManager, STATE_CONNECTED
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
def network_offer():
	d = Device(host=NETWORK_ADDR)
	d.scan()
	if d.get_key('test_services') != 1:
		logging.debug("resetting network device...")
		d.set_key('test_services', 1)
		d.reboot()
		d.wait()
		logging.debug("network device ready!")
		
	return d

@pytest.fixture
def network_listen():
	d = Device(host=NETWORK_ADDR)
	d.scan()
	if d.get_key('test_services') != 2:
		logging.debug("resetting network device...")
		d.set_key('test_services', 2)
		d.reboot()
		d.wait()
		logging.debug("network device ready!")
		
	return d

def test_basic_service(listen_service, offer_service):
	listen_service.wait_until_connected(timeout=60.0)
	offer_service.wait_until_connected(timeout=60.0)

	assert offer_service.is_server
	assert offer_service.connected
	assert listen_service.connected
	assert not listen_service.is_server

def test_network_listen(listen_service, network_offer):
	listen_service.wait_until_connected(timeout=60.0)

	assert listen_service.connected
	assert not listen_service.is_server
	assert listen_service.server[0] == network_offer.host

def test_network_offer(offer_service, network_listen):
	offer_service.wait_until_connected(timeout=60.0)

	s = network_listen.wait_service(1234, 5678)
	assert s
	assert s.state == STATE_CONNECTED

