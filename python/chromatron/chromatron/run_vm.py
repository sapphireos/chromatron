# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>
import sys
import code_gen
import time
import threading
from sapphire.common.ribbon import Ribbon
from sapphire.common.app import App
import hashlib


class VMContainer(Ribbon):
    def initialize(self, fx_file=None, width=8, height=8):
        self.fx_file = fx_file

        self.width = width
        self.height = height
        self.frame_rate = 0.025

        self.file_hash = None

        self.check_file_hash()

        self.load_vm()


    def load_vm(self):
        self.code = code_gen.compile_script(self.fx_file)
        self.vm = code_gen.VM(
                    self.code["vm_code"],
                    self.code["vm_data"],
                    pix_size_x=self.width,
                    pix_size_y=self.height)

        self.vm.init()

    def check_file_hash(self):
        m = hashlib.md5()
        m.update(open(self.fx_file).read())

        new_hash = m.hexdigest()
        old_hash = self.file_hash

        self.file_hash = new_hash

        if new_hash != old_hash:
            return True

        return False

    def loop(self):
        next_run = time.time() + self.frame_rate

        if self.check_file_hash():
            print "Reloading VM"
            self.load_vm()

        self.vm.loop()

        self.wait(next_run - time.time())


if __name__ == '__main__':

    # app = App()

    vm = VMContainer(fx_file=sys.argv[1])
    vm.stop()
    vm.join()

    # app.run()

    print vm.vm.dump_registers()
    print vm.vm.kv
