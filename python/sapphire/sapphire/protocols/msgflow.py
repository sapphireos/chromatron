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

import sys
import time
import socket
import select
import logging
from elysianfields import *
from ..common.broadcast import send_udp_broadcast
from ..common import Ribbon, util, catbus_string_hash


MSGFLOW_LISTEN_PORT             = 32039

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

MSGFLOW_TYPE_SINK                   = 1
MSGFLOW_TYPE_RESET                  = 2
MSGFLOW_TYPE_READY                  = 3
MSGFLOW_TYPE_STATUS                 = 4
MSGFLOW_TYPE_DATA                   = 5
MSGFLOW_TYPE_STOP                   = 6


MSGFLOW_MSG_RESET_FLAGS_RESET_SEQ   = 0x01

CONNECTION_TIMEOUT                  = 32.0
ANNOUNCE_INTERVAL                   = 4.0
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

class MsgFlowMsgSink(StructField):
    def __init__(self, **kwargs):
        fields = [MsgFlowHeader(_name="header"),
                  Uint32Field(_name="service"),
                  ArrayField(_name="codebook", _field=Uint8Field, _length=8)]

        super(MsgFlowMsgSink, self).__init__(_name="msg_flow_msg_sink", _fields=fields, **kwargs)

        self.header.type = MSGFLOW_TYPE_SINK

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


messages = {
    MSGFLOW_TYPE_SINK:      MsgFlowMsgSink,
    MSGFLOW_TYPE_RESET:     MsgFlowMsgReset,
    MSGFLOW_TYPE_READY:     MsgFlowMsgReady,
    MSGFLOW_TYPE_STATUS:    MsgFlowMsgStatus,
    MSGFLOW_TYPE_DATA:      MsgFlowMsgData,
    MSGFLOW_TYPE_STOP:      MsgFlowMsgStop,
}

def deserialize(buf):
    msg_id = int(buf[0])

    try:
        return messages[msg_id]().unpack(buf)

    except KeyError:
        raise UnknownMessageException(msg_id)

    except (struct.error, UnicodeDecodeError) as e:
        raise InvalidMessageException(msg_id, len(buf), e)


class MsgFlowReceiver(Ribbon):
    def initialize(self, 
                   name='msgflow_receiver', 
                   service=None, 
                   port=None,
                   on_receive=None,
                   on_connect=None,
                   on_disconnect=None):

        self.name = name

        self.__service_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        if port is None:
            self.__service_sock.bind(('0.0.0.0', 0))
            
        else:
            self.__service_sock.bind(('0.0.0.0', port))

        self.__service_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.__service_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            # this option may fail on some platforms
            self.__service_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        except AttributeError:
            pass

        self._port = self.__service_sock.getsockname()[1]

        logging.info(f"MsgFlowReceiver on port: {self._port}")

        self.__service_sock.setblocking(0)

        self._inputs = [self.__service_sock]
        
        self._msg_handlers = {
            MsgFlowMsgSink: self._handle_sink,
            MsgFlowMsgReset: self._handle_reset,
            MsgFlowMsgData: self._handle_data,
            MsgFlowMsgStop: self._handle_stop,
        }

        self.service = service
        self._service_hash = catbus_string_hash(service)

        self._connections = {}

        self._last_announce = time.time() - 10.0
        self._last_status = time.time() - 10.0

        if on_receive is not None:
            self.on_receive = on_receive

        if on_connect is not None:
            self.on_connect = on_connect

        if on_disconnect is not None:
            self.on_disconnect = on_disconnect

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

    def _send_msg(self, msg, host):
        s = self.__service_sock

        try:
            if host[0] == '<broadcast>':
                send_udp_broadcast(s, host[1], msg.pack())

            else:
                s.sendto(msg.pack(), host)

        except socket.error:
            pass

    def _send_sink(self, host=('<broadcast>', MSGFLOW_LISTEN_PORT)):
        msg = MsgFlowMsgSink(
                service=self._service_hash,
                codebook=[0,0,0,0,0,0,0,0])

        self._send_msg(msg, host)

    def _send_status(self, host):
        msg = MsgFlowMsgStatus(
                sequence=self._connections[host]['sequence'])

        self._send_msg(msg, host)

    def _send_ready(self, host, sequence=None, code=0):
        msg = MsgFlowMsgReady(
                code=code)

        self._send_msg(msg, host)

    def _send_stop(self, host):
        msg = MsgFlowMsgStop()
                
        self._send_msg(msg, host)

    def _handle_sink(self, msg, host):
        pass

    def _handle_data(self, msg, host):
        if host not in self._connections:
            logging.warning(f"Host: {host} not found")
            return

        if len(msg.data) == 0:
            # logging.debug(f"Keepalive from: {host}")
            pass

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

                    response, host = self._process_msg(msg, host)
                    
                    if response:
                        response.header.transaction_id = msg.header.transaction_id
                        self._send_msg(response, host)

                except UnknownMessageException as e:
                    raise

                except Exception as e:
                    logging.exception(e)


        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)


        if time.time() - self._last_announce > ANNOUNCE_INTERVAL:
            self._last_announce = time.time()
            
            self._send_sink()

        if time.time() - self._last_status > STATUS_INTERVAL:
            self._last_status = time.time()

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
        # print(sequence)
        pass

    m = MsgFlowReceiver(port=12345, service='test', on_receive=on_receive)

    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

    m.stop()
    m.join()

if __name__ == '__main__':
    main()

