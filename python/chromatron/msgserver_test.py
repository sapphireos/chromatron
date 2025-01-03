from sapphire.common.msgserver import *
from sapphire.common.util import setup_basic_logging

SERVICES_PORT               = 32041
SERVICES_MCAST_ADDR         = "239.43.96.31"

def meow():
    print('meow')

if __name__ == '__main__':
    setup_basic_logging()

    s = MsgServer(listener_port=SERVICES_PORT, listener_mcast=SERVICES_MCAST_ADDR)
    # s.start_timer(1.0, meow)

    print(s.port)

    run_all()

