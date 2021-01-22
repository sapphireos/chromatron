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

SERVICES_PORT               = 32040
SERVICES_MAGIC              = 0x56524553 # 'SERV'
SERVICES_VERSION            = 1

SERVICES_MCAST_ADDR         = "239.43.96.31"


class UnknownMessageException(Exception):
    pass

class InvalidMessageException(Exception):
    pass


SERVICE_MSG_TYPE_OFFERS     = 1
SERVICE_MSG_TYPE_QUERY      = 2


class ServiceMsgHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="magic"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="type"),
                  Uint8Field(_name="flags"),
                  Uint8Field(_name="reserved")]

        super(self).__init__(_fields=fields, **kwargs)

        self.magic          = SERVICES_MAGIC
        self.version        = SERVICES_VERSION

class ServiceMsgOfferHeader(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=3)]

        super(self).__init__(_fields=fields, **kwargs)

        self.type = SERVICE_MSG_TYPE_OFFERS

class ServiceOffer(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="id"),
                  Uint32Field(_name="group"),
                  Uint16Field(_name="priority"),
                  Uint16Field(_name="port"),
                  Uint32Field(_name="uptime"),
                  Uint8Field(_name="flags"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=3)]

        super(self).__init__(_fields=fields, **kwargs)

class ServiceMsgOffers(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgOfferHeader(_name="offer_header"),
                  ArrayField(_name="reserved", _field=ServiceOffer)]

        super(self).__init__(_fields=fields, **kwargs)

SERVICE_OFFER_FLAGS_TEAM    = 0x01
SERVICE_OFFER_FLAGS_SERVER  = 0x02

class ServiceMsgQuery(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgHeader(_name="header"),
                  Uint32Field(_name="id"),
                  Uint32Field(_name="group")]

        super(self).__init__(_fields=fields, **kwargs)

        self.type = SERVICE_MSG_TYPE_QUERY


messages = {
    SERVICE_MSG_TYPE_OFFERS:    ServiceMsgOffers,
    SERVICE_MSG_TYPE_QUERY:     ServiceMsgQuery,
}

def deserialize(buf):
    msg_id = int(buf[0])

    try:
        return messages[msg_id]().unpack(buf)

    except KeyError:
        raise UnknownMessageException(msg_id)

    except (struct.error, UnicodeDecodeError) as e:
        raise InvalidMessageException(msg_id, len(buf), e)




class ServiceManager(Ribbon):
    def initialize(self, 
                   name='service_manager', 
                   service=None, 
                   group=None,
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

        logging.info(f"ServiceManager on port: {self._port}")

        self.__service_sock.setblocking(0)

        self._inputs = [self.__service_sock]
        
        self._msg_handlers = {
            ServiceMsgOffers: self._handle_offers,
            ServiceMsgQuery: self._handle_query,
        }

        self.service = service
        self.group = group
        self.port = port
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

    m = ServiceManager(port=12345, service='test', on_receive=on_receive)

    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

    m.stop()
    m.join()

if __name__ == '__main__':
    main()

