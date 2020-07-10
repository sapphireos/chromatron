#
# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2020  Jeremy Billheimer
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


MSGFLOW_FLAGS_VERSION           = 0
MSGFLOW_FLAGS_VERSION_MASK      = 0x0F

code_book = {
    MSGFLOW_CODE_INVALID            :0,
    MSGFLOW_CODE_NONE               :1,
    MSGFLOW_CODE_ARQ                :2,
    MSGFLOW_CODE_TRIPLE             :3,
    MSGFLOW_CODE_SMALLBUF           :4,
    MSGFLOW_CODE_1_OF_4_PARITY      :5,
    MSGFLOW_CODE_2_OF_7_PARITY      :6,
    MSGFLOW_CODE_ANY                :0xff,
}

MSGFLOW_TYPE_SINK                   = 1


class MsgFlowHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="flags"),
                  Uint8Field(_name="type")]

        super(MsgFlowHeader, self).__init__(_name="msg_flow_header", _fields=fields, **kwargs)


class MsgFlowMsgSink(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  Uint32Field(_name="service"),
                  ArrayField(_name="codebook", _field=Uint8Field, _length=8)]

        super(MsgFlowMsgSink, self).__init__(_name="msg_flow_msg_sink", _fields=fields, **kwargs)

        self.header.msg_type = MSGFLOW_TYPE_SINK
