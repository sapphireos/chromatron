import sys
import time
from catbus import CatbusService
from sapphire.common.ribbon import wait_for_signal, Ribbon

class LogServer(Ribbon):
    def initialize(self, settings={}):
        self.name = 'logserver'
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

    def loop(self):
        self.wait(1.0)

    def clean_up(self):
        self.kv.stop()
        self.kv.wait()

settings = {}
try:
    with open('settings.json', 'r') as f:
        settings = json.loads(f.read())

except FileNotFoundError:
    pass

l = LogServer(settings=settings)

wait_for_signal()

l.stop()
l.join()