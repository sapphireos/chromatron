from catbus.link import *
from catbus import CatbusService
from sapphire.common import util, run_all



util.setup_basic_logging(console=True)

c = CatbusService(tags=['__TEST__'])

# v = [1,2,3,4]
# v = [9,8,7,6]

# c.publish('kv_test_array', v, '10.0.0.66')

# c.publish('kv_test_key', 123, '10.0.0.66')

c['kv_test_array'] = [5,7,9,11]
c['kv_test_key'] = 554


c.send('kv_test_array', 'kv_test_array', ['jerbear'])
c.send('kv_test_key', 'kv_test_key', ['jerbear'])


run_all()

