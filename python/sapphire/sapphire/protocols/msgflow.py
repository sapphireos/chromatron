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

import time
import socket
import select
import logging
from elysianfields import *
from sapphire.common.broadcast import send_udp_broadcast
from sapphire.common import Ribbon, MsgQueueEmptyException, util

MSGFLOW_LISTEN_PORT             = 32039

MSGFLOW_FLAGS_VERSION           = 0
MSGFLOW_FLAGS_VERSION_MASK      = 0x0F

code_book = {
    'MSGFLOW_CODE_INVALID'            :0,
    'MSGFLOW_CODE_NONE'               :1,
    'MSGFLOW_CODE_ARQ'                :2,
    'MSGFLOW_CODE_TRIPLE'             :3,
    'MSGFLOW_CODE_SMALLBUF'           :4,
    'MSGFLOW_CODE_1_OF_4_PARITY'      :5,
    'MSGFLOW_CODE_2_OF_7_PARITY'      :6,
    'MSGFLOW_CODE_ANY'                :0xff,
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



class MsgFlowReceiver(Ribbon):
    def initialize(self, name='msgflow_receiver'):
        self.name = name

        self.__service_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__service_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__service_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__service_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self.__service_sock.setblocking(0)

        self._inputs = [self.__service_sock]
        
        self._msg_handlers = {
            # ErrorMsg: self._handle_error,
        }

    def clean_up(self):
        # self._send_shutdown()
        # time.sleep(0.1)
        # self._send_shutdown()
        # time.sleep(0.1)
        # self._send_shutdown()
        pass

    def _send_data_msg(self, msg, host):
        msg.header.origin_id = self._origin_id
        s = self.__data_sock

        try:
            if host[0] == '<broadcast>':
                send_udp_broadcast(s, host[1], serialize(msg))

            else:
                s.sendto(serialize(msg), host)

        except socket.error:
            pass

    def _send_announce(self, host=('<broadcast>', MSGFLOW_LISTEN_PORT), discovery_id=None):
        pass
        # msg = AnnounceMsg(
        #         data_port=self._data_port,
        #         query=self._database.get_query())

        # if discovery_id:
        #     msg.header.transaction_id = discovery_id

        # self._send_data_msg(msg, host)

    def _process_msg(self, msg, host):        
        tokens = self._msg_handlers[type(msg)](msg, host)

        # normally, you'd just try to access the tuple and
        # handle the exception. However, that will raise a TypeError, 
        # and if we handle a TypeError here, then any TypeError generated
        # in the message handler will essentially get eaten.
        if not isinstance(tokens, tuple):
            return None, None

        if len(tokens) < 2:
            return None, None

        return tokens[0], tokens[1]

    def loop(self):
        try:
            readable, writable, exceptional = select.select(self._inputs, [], [], 1.0)

            for s in readable:
                try:
                    data, host = s.recvfrom(1024)

                    msg = deserialize(data)
                    response = None                    

                    # filter our own messages
                    try:
                        if msg.header.origin_id == self._origin_id:
                            continue

                        if host[1] == self._data_port:
                            continue

                    except KeyError:
                        pass

                    response, host = self._process_msg(msg, host)
                    
                    if response:
                        response.header.transaction_id = msg.header.transaction_id
                        self._send_data_msg(response, host)

                except UnknownMessageException as e:
                    pass

                except Exception as e:
                    logging.exception(e)


        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)



def main():
    util.setup_basic_logging(console=True)

    m = MsgFlowReceiver()

    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

    m.stop()
    m.join()

if __name__ == '__main__':
    main()

