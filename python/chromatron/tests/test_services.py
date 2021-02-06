import time
import pytest

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

@pytest.mark.skip
def test_basic_service(listen_service, offer_service):
	listen_service.wait_until_connected(timeout=30.0)
	offer_service.wait_until_connected(timeout=30.0)

	assert offer_service.is_server
	assert offer_service.connected
	assert listen_service.connected
	assert not listen_service.is_server

@pytest.fixture
def network_offer():
	client = Client(('10.0.0.157', CATBUS_MAIN_PORT))
	if client.get_key('test_services') != 1:
		client.set_key('test_services', 1)
		client.set_key('reboot', 1)
	return client

@pytest.fixture
def network_listen():
	client = Client(('10.0.0.157', CATBUS_MAIN_PORT))
	if client.get_key('test_services') != 2:
		client.set_key('test_services', 2)
		client.set_key('reboot', 1)
	return client

@pytest.mark.skip
def test_network_listen(listen_service, network_offer):
	listen_service.wait_until_connected(timeout=30.0)

	assert listen_service.connected
	assert not listen_service.is_server
	assert listen_service.server[0] == network_offer._connected_host[0]

def test_network_offer(offer_service, network_listen):
	offer_service.wait_until_connected(timeout=30.0)
	
	# d = Device(host='10.0.0.157')
	# for i in range(300):
	# 	time.sleep(0.1)

	# 	services = d.get_service_info()
	# 	for s in services:
	# 		if s.id == 1234 and s.group == 5678 and s.state == STATE_CONNECTED:
	# 			return

	# assert False
