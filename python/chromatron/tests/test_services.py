
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
	for i in range(200):
		await asyncio.sleep(0.1)

		if offer_service.is_server and listen_service.connected:
			break

	assert offer_service.is_server
	assert offer_service.connected
	assert listen_service.connected
	assert not listen_service.is_server

