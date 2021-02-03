
import pytest

from sapphire.protocols.services import ServiceManager

from fixtures import *

@pytest.fixture
def listen_service():
	s = ServiceManager()
	return s.listen(1234, 5678)

@pytest.fixture
def offer_service():
	s = ServiceManager()
	return s.offer(1234, 5678, 1000, priority=99)

@pytest.mark.asyncio
async def test_basic_service(listen_service, offer_service):
	await listen_service.wait_until_connected(timeout=30.0)
	await offer_service.wait_until_connected(timeout=30.0)

	assert offer_service.is_server
	assert offer_service.connected
	assert listen_service.connected
	assert not listen_service.is_server

