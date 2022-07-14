
from .test_code_gen import CompilerTests, HSVArrayTests

from conftest import *

import chromatron
from chromatron import code_gen
from catbus import ProtocolErrorException
import time

ct = chromatron.Chromatron(host=NETWORK_ADDR)

@pytest.mark.skip
@pytest.mark.device
@pytest.mark.parametrize("opt_passes", TEST_OPT_PASSES)
class TestCompilerOnDevice(CompilerTests):
    def run_test(self, program, expected={}, opt_passes=[OptPasses.SSA]):
        global ct
        # ct = chromatron.Chromatron(host='usb', force_network=True)
        # ct = chromatron.Chromatron(host='10.0.0.108')

        tries = 3

        while tries > 0:
            try:
                # ct.load_vm(bin_data=program)
                with open('test.fx', 'w') as f:
                    f.write(program) # write out the program source for debug if it fails

                program = code_gen.compile_text(program, debug_print=False, opt_passes=opt_passes)
                image = program.assemble()
                stream = image.render('test.fxb')

                ct.stop_vm()

                # reset test key
                ct.set_key('kv_test_key', 0)
                
                # change vm program
                ct.set_key('vm_prog', 'test.fxb')
                ct.put_file('test.fxb', stream)
                ct.start_vm()

                for i in range(100):
                    time.sleep(0.1)

                    vm_status = ct.get_key('vm_status')

                    if vm_status != 4 and vm_status != -127:
                        # vm reports READY (4), we need to wait until it changes 
                        # (READY means it is waiting for the internal VM to start)
                        # -127 means the VM is not initialized
                        break

                assert vm_status == 0

                ct.init_scan()
                
                for reg, expected_value in expected.items():
                    tries = 5
                    while tries > 0:
                        tries -= 1

                        if reg == 'kv_test_key':
                            actual = ct.get_key(reg)

                        else:
                            actual = ct.get_vm_reg(str(reg))

                        try:
                            assert actual == expected_value

                        except AssertionError:
                            # print "Expected: %s Actual: %s Reg: %s" % (expected_value, actual, reg)
                            # print "Resetting VM..."
                            if tries == 0:
                                raise

                            ct.reset_vm()
                            time.sleep(0.2)

                return

            except ProtocolErrorException:
                print("Protocol error, trying again.")
                tries -= 1

                if tries < 0:
                    raise

        ct.stop_vm()



@pytest.mark.local
@pytest.mark.parametrize("opt_passes", TEST_OPT_PASSES)
class TestHSVArrayLocal(HSVArrayTests):
    def run_test(self, program, opt_passes=[OptPasses.SSA]):
        global ct
        # ct = chromatron.Chromatron(host='usb', force_network=True)
        # ct = chromatron.Chromatron(host='10.0.0.108')

        tries = 3

        while tries > 0:
            try:
                # ct.load_vm(bin_data=program)
                with open('test.fx', 'w') as f:
                    f.write(program) # write out the program source for debug if it fails

                program = code_gen.compile_text(program, debug_print=False, opt_passes=opt_passes)
                image = program.assemble()
                stream = image.render('test.fxb')

                ct.stop_vm()

                # reset test key
                ct.set_key('kv_test_key', 0)
                
                ct.set_key('gfx_debug', True)

                ct.set_key('gfx_hsfade', 0)
                ct.set_key('gfx_vfade', 0)

                ct.set_key('gfx_debug_reset', True)

                ct.set_key('pix_count', 16)
                ct.set_key('pix_size_x', 4)                
                ct.set_key('pix_size_y', 4)                

                time.sleep(0.3)

                # change vm program
                ct.set_key('vm_prog', 'test.fxb')
                ct.put_file('test.fxb', stream)
                ct.start_vm()

                for i in range(100):
                    time.sleep(0.1)

                    vm_status = ct.get_key('vm_status')

                    if vm_status != 4 and vm_status != -127:
                        # vm reports READY (4), we need to wait until it changes 
                        # (READY means it is waiting for the internal VM to start)
                        # -127 means the VM is not initialized
                        break

                assert vm_status == 0

                ct.init_scan()

                hsv = ct.dump_hsv()
                
                from pprint import pprint
                pprint(hsv)

                return hsv
                
            except ProtocolErrorException:
                print("Protocol error, trying again.")
                tries -= 1

                if tries < 0:
                    raise

        ct.stop_vm()