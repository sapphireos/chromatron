import sys
import time
from catbus import CatbusService
from ..common.ribbon import wait_for_signal
from ..protocols.msgflow import MsgFlowReceiver
from ..common import util

class LogServer(MsgFlowReceiver):
    def initialize(self, settings={}):
        super().initialize(name='logserver')
        self.settings = settings
        
        self.kv = CatbusService(name=self.name, visible=True, tags=[])

    def loop(self):
        self.wait(1.0)

    def clean_up(self):
        self.kv.stop()
        self.kv.wait()

def main():
    util.setup_basic_logging(console=True)

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


if __name__ == '__main__':
    main()
