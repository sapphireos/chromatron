#
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
#

from elysianfields import *
from sapphire.fields.protocol import *
from sapphiredata import *

CMD2_APP_CMD_BASE = 32768


class DeviceCommandProtocol(Protocol):

    PORT = 16385

    class Echo(Payload):
        msg_type = 1
        fields = [StringField(_length=64, _name="echo_data")]

    class Reboot(Payload):
        msg_type = 2
        fields = []

    class SafeMode(Payload):
        msg_type = 3
        fields = []

    class LoadFirmware(Payload):
        msg_type = 4
        fields = []

    class FormatFS(Payload):
        msg_type = 10
        fields = []

    class GetFileID(Payload):
        msg_type = 20
        fields = [StringField(_length=64, _name="name")]

    class CreateFile(Payload):
        msg_type = 21
        fields = [StringField(_length=64, _name="name")]

    class ReadFileData(Payload):
        msg_type = 22
        fields = [Uint8Field(_name="file_id"),
                  Uint32Field(_name="position"),
                  Uint32Field(_name="length")]

    class WriteFileData(Payload):
        msg_type = 23
        fields = [Uint8Field(_name="file_id"),
                  Uint32Field(_name="position"),
                  Uint32Field(_name="length"),
                  RawBinField(_name="data")]

    class RemoveFile(Payload):
        msg_type = 24
        fields = [Uint8Field(_name="file_id")]

    class ResetCfg(Payload):
        msg_type = 32
        fields = []

    class SetRegbits(Payload):
        msg_type = 40
        fields = [Uint16Field(_name="reg"),
                  Uint8Field(_name="mask")]

    class ClrRegbits(Payload):
        msg_type = 41
        fields = [Uint16Field(_name="reg"),
                  Uint8Field(_name="mask")]

    class GetRegbits(Payload):
        msg_type = 42
        fields = [Uint16Field(_name="reg")]

    class SetKV(Payload):
        msg_type = 80
        fields = [RawBinField(_name="data")]

    class GetKV(Payload):
        msg_type = 81
        fields = [RawBinField(_name="data")]

    class SetKVServer(Payload):
        msg_type = 85
        fields = [Ipv4Field(_name="ip"),
                  Uint16Field(_name="port")]

    class SetSecurityKey(Payload):
        msg_type = 90
        fields = [Uint8Field(_name="key_id"),
                  Key128Field(_name="key")]

    msg_type_format = Uint16Field()


class DeviceCommandResponseProtocol(Protocol):

    class Echo(Payload):
        msg_type = 1
        fields = [StringField(_length=64, _name="echo_data")]

    class Reboot(Payload):
        msg_type = 2
        fields = []

    class SafeMode(Payload):
        msg_type = 3
        fields = []

    class LoadFirmware(Payload):
        msg_type = 4
        fields = []

    class FormatFS(Payload):
        msg_type = 10
        fields = []

    class GetFileID(Payload):
        msg_type = 20
        fields = [Int8Field(_name="file_id")]

    class CreateFile(Payload):
        msg_type = 21
        fields = [Int8Field(_name="file_id")]

    class ReadFileData(Payload):
        msg_type = 22
        fields = [RawBinField(_name="data")]

    class WriteFileData(Payload):
        msg_type = 23
        fields = [Uint16Field(_name="write_length")]

    class RemoveFile(Payload):
        msg_type = 24
        fields = [Uint8Field(_name="status")]

    class ResetCfg(Payload):
        msg_type = 32
        fields = []

    class SetRegbits(Payload):
        msg_type = 40
        fields = []

    class ClrRegbits(Payload):
        msg_type = 41
        fields = []

    class GetRegbits(Payload):
        msg_type = 42
        fields = [Uint8Field(_name="data")]

    class SetKV(Payload):
        msg_type = 80
        fields = [RawBinField(_name="data")]

    class GetKV(Payload):
        msg_type = 81
        fields = [RawBinField(_name="data")]

    class SetKVServer(Payload):
        msg_type = 85
        fields = []

    class SetSecurityKey(Payload):
        msg_type = 90
        fields = []

    msg_type_format = Uint16Field()


if __name__ == '__main__':
    pass
