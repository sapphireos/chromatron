# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
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
from . import code_gen
from .vm import VM
import time
import threading
from sapphire.common.ribbon import Ribbon
from sapphire.common.app import App
import hashlib
from pprint import pprint


class VMContainer(Ribbon):
    def initialize(self, fx_file=None, width=2, height=2):
        self.fx_file = fx_file

        self.width = width
        self.height = height
        self.frame_rate = 0.025

        self.file_hash = None

        self.check_file_hash()

        self.load_vm()


    def load_vm(self):
        self.code = code_gen.compile_script(self.fx_file)
        self.vm = VM(self.code,
                     pix_size_x=self.width,
                     pix_size_y=self.height)

        self.vm.run('init')

    def check_file_hash(self):
        m = hashlib.md5()
        with open(self.fx_file, 'rb') as f:
            m.update(f.read())

        new_hash = m.hexdigest()
        old_hash = self.file_hash

        self.file_hash = new_hash

        if new_hash != old_hash:
            return True

        return False

    def loop(self):
        next_run = time.time() + self.frame_rate

        if self.check_file_hash():
            print("Reloading VM")
            self.load_vm()

        self.vm.run('loop')

        self.wait(next_run - time.time())


if __name__ == '__main__':

    vm = VMContainer(fx_file=sys.argv[1])
    vm.stop()
    vm.join()

    pprint(vm.vm.dump_registers())
    pprint(vm.vm.dump_hsv())
