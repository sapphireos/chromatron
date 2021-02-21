#
# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2021  Jeremy Billheimer
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
import sys
import socket
import select
import logging
from elysianfields import *
from ..common import MsgServer, run_all, util, catbus_string_hash, synchronized
from .services import ServiceManager


MSGFLOW_FLAGS_VERSION           = 1
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

MSGFLOW_TYPE_RESET                  = 2
MSGFLOW_TYPE_READY                  = 3
MSGFLOW_TYPE_STATUS                 = 4
MSGFLOW_TYPE_DATA                   = 5
MSGFLOW_TYPE_STOP                   = 6
MSGFLOW_TYPE_QUERY_CODEBOOK         = 7
MSGFLOW_TYPE_CODEBOOK               = 8


MSGFLOW_MSG_RESET_FLAGS_RESET_SEQ   = 0x01

CONNECTION_TIMEOUT                  = 32.0
STATUS_INTERVAL                     = 4.0

class UnknownMessageException(Exception):
    pass

class InvalidMessageException(Exception):
    pass

class MsgFlowHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="type"),
                  Uint8Field(_name="flags"),
                  Uint16Field(_name="reserved")]

        super(MsgFlowHeader, self).__init__(_fields=fields, **kwargs)

        self.flags = MSGFLOW_FLAGS_VERSION

class MsgFlowMsgReset(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  Uint16Field(_name="max_data_len"),
                  Uint8Field(_name="code"),
                  Uint8Field(_name="flags"),
                  Uint64Field(_name="device_id")]

        super(MsgFlowMsgReset, self).__init__(_name="msg_flow_msg_reset", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_RESET

class MsgFlowMsgReady(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  Uint8Field(_name="code"),
                  Uint8Field(_name="reserved")]

        super(MsgFlowMsgReady, self).__init__(_name="msg_flow_msg_ready", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_READY

class MsgFlowMsgStatus(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  Uint64Field(_name="sequence"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=12)]

        super(MsgFlowMsgStatus, self).__init__(_name="msg_flow_msg_statis", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_STATUS

class MsgFlowMsgData(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  Uint64Field(_name="sequence"),
                  ArrayField(_name="data", _field=Uint8Field)]

        super(MsgFlowMsgData, self).__init__(_name="msg_flow_msg_data", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_DATA

class MsgFlowMsgStop(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header")]

        super(MsgFlowMsgStop, self).__init__(_name="msg_flow_msg_stop", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_STOP

class MsgFlowMsgQueryCodebook(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header")]

        super(MsgFlowMsgQueryCodebook, self).__init__(_name="msg_flow_msg_query_codebook", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_QUERY_CODEBOOK

class MsgFlowMsgCodebook(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  ArrayField(_name="codebook", _field=Uint8Field, _length=8)]

        super(MsgFlowMsgCodebook, self).__init__(_name="msg_flow_msg_codebook", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_CODEBOOK


class MsgFlowReceiver(MsgServer):
    def __init__(self, 
                   service=None, 
                   port=None,
                   on_receive=None,
                   on_connect=None,
                   on_disconnect=None):

        super().__init__(name=service, port=port)
        
        self.register_message(MsgFlowMsgReset, self._handle_reset)
        self.register_message(MsgFlowMsgData, self._handle_data)
        self.register_message(MsgFlowMsgStop, self._handle_stop)

        self.start_timer(STATUS_INTERVAL, self._process_status_timer)

        self.service = service
        self._service_hash = catbus_string_hash(service)

        self._connections = {}

        if on_receive is not None:
            self.on_receive = on_receive

        if on_connect is not None:
            self.on_connect = on_connect

        if on_disconnect is not None:
            self.on_disconnect = on_disconnect

        self.services_manager = ServiceManager()
        self.services_manager.offer("msgflow", self.service, self._port)

        self.start()

    def on_connect(self, host, device_id=None):
        pass

    def on_disconnect(self, host):
        pass

    def on_receive(self, host, data):
        pass

    def clean_up(self):
        for host in self._connections:
            self._send_stop(host)

        time.sleep(0.1)

        for host in self._connections:
            self._send_stop(host)

        time.sleep(0.1)

        for host in self._connections:
            self._send_stop(host)

    def _send_status(self, host):
        msg = MsgFlowMsgStatus(
                sequence=self._connections[host]['sequence'])

        self.transmit(msg, host)

    def _send_ready(self, host, sequence=None, code=0):
        msg = MsgFlowMsgReady(
                code=code)

        self.transmit(msg, host)

    def _send_stop(self, host):
        msg = MsgFlowMsgStop()
                
        self.transmit(msg, host)

    def _handle_data(self, msg, host):
        if host not in self._connections:
            logging.warning(f"Host: {host} not found")
            return

        if len(msg.data) == 0: # this is a keepalive message
            logging.debug(f"Keepalive from: {host}")

            # update sequence!
            self._connections[host]['sequence'] = msg.sequence
            
            # send status to confirm
            self._send_status(host) 

        else:
            # check sequence
            if msg.sequence > self._connections[host]['sequence']:
                prev_seq = self._connections[host]['sequence']
                self._connections[host]['sequence'] = msg.sequence
                    
                # TODO assuming ARQ!
                self._send_status(host)

                # data!
                data = bytes(msg.data.toBasic())
                try:
                    self.on_receive(host, data)

                except Exception as e:
                    logging.exception(e)

                logging.info(f"Msg {msg.sequence} len: {len(msg.data)} host: {host}")

            elif msg.sequence <= self._connections[host]['sequence']:
                # TODO assuming ARQ!
                # got a duplicate data message, resend status
                self._send_status(host)

                logging.warning(f"Dup {msg.sequence} sequence: {self._connections[host]['sequence']}")

        self._connections[host]['timeout'] = CONNECTION_TIMEOUT

    def _handle_stop(self, msg, host):
        if host in self._connections:
            logging.info(f"Removing: {host}")
            del self._connections[host]

            try:
                self.on_disconnect(host)
            except Exception as e:
                logging.exception(e)

    def _handle_reset(self, msg, host):
        if host not in self._connections:
            logging.info(f"Connection from: {host} max_len: {msg.max_data_len}")
            try:
                self.on_connect(host, msg.device_id)

            except Exception as e:
                logging.exception(e)

        self._connections[host] = {'sequence': 0, 'timeout': CONNECTION_TIMEOUT}

        self._send_ready(host, code=msg.code)

    def _process_status_timer(self):
        for host in self._connections:
            # process timeouts
            self._connections[host]['timeout'] -= STATUS_INTERVAL

            if self._connections[host]['timeout'] <= 0.0:

                logging.info(f"Timed out: {host}")
                self.on_disconnect(host)

                continue

            self._send_status(host)

        self._connections = {k: v for k, v in self._connections.items() if v['timeout'] > 0.0}
            
def main():
    util.setup_basic_logging(console=True)

    def on_receive(host, data):
        pass

    m = MsgFlowReceiver(port=12345, service='test', on_receive=on_receive)

    run_all()

if __name__ == '__main__':
    main()

