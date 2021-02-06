import time
import pytest
import logging
from catbus import *
from sapphire.devices.device import Device

from fixtures import *



@pytest.fixture
def network_idle():
    d = Device(host=NETWORK_ADDR)
    d.scan()
    if d.get_key('test_link') != 1:
        logging.debug("resetting network device...")
        d.set_key('test_link', 1)
        d.reboot()
        d.wait()
        logging.debug("network device ready!")
        
    return d


@pytest.fixture
def send_link():
    lm = LinkManager(test_mode=True)
    lm.send('link_test_key', 'kv_test_key', query=['__TEST__'])
    yield lm
    lm.stop()
    lm.join()


def test_send_link(send_link, network_idle):
    pass
