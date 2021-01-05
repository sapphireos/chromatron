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
SERVICES_VERSION            = 2


class UnknownMessage(Exception):
    pass

class InvalidMessage(Exception):
    pass

class InvalidVersion(Exception):
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

        super().__init__(_fields=fields, **kwargs)

        self.magic          = SERVICES_MAGIC
        self.version        = SERVICES_VERSION

class ServiceMsgOfferHeader(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=3)]

        super().__init__(_fields=fields, **kwargs)

        self.type = SERVICE_MSG_TYPE_OFFERS

STATE_LISTEN    = 0
STATE_CONNECTED = 0
STATE_SERVER    = 0

SERVICE_UPTIME_MIN_DIFF = 5

SERVICE_OFFER_FLAGS_TEAM    = 0x01
SERVICE_OFFER_FLAGS_SERVER  = 0x02

class ServiceOffer(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="id"),
                  Uint64Field(_name="group"),
                  Uint16Field(_name="priority"),
                  Uint16Field(_name="port"),
                  Uint32Field(_name="uptime"),
                  Uint8Field(_name="flags"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=3)]

        super().__init__(_fields=fields, **kwargs)

    @property
    def key(self):
        return (self.id << 64) + self.group

    @property
    def server_valid(self):
        return (self.flags & SERVICE_OFFER_FLAGS_SERVER) != 0

    def __gt__(self, other):
        if other.priority > self.priority:
            return False # other is better

        if other.priority < self.priority:
            return True # we are better

        diff = other.uptime - self.uptime

        if diff > SERVICE_UPTIME_MIN_DIFF:
            # other is better
            return False

        if diff < (-1 * SERVICE_UPTIME_MIN_DIFF):
            # we are better
            return True


        # IP address priority is not supported for now.
        # this is hard to do on a Python system since we might
        # have multiple IP addresses.
        # We are assuming our services are always talking to hardware
        # which will have priority.
        return False

class ServiceMsgOffers(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgOfferHeader(_name="offer_header"),
                  ArrayField(_name="offers", _field=ServiceOffer)]

        super().__init__(_fields=fields, **kwargs)

class ServiceMsgQuery(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgHeader(_name="header"),
                  Uint32Field(_name="id"),
                  Uint64Field(_name="group")]

        super().__init__(_fields=fields, **kwargs)

        self.type = SERVICE_MSG_TYPE_QUERY


messages = {
    SERVICE_MSG_TYPE_OFFERS:    ServiceMsgOffers,
    SERVICE_MSG_TYPE_QUERY:     ServiceMsgQuery,
}

def deserialize(buf):
    version = int(buf[4])
    msg_id = int(buf[5])

    if version != SERVICES_VERSION:
        raise InvalidVersion()

    try:
        return messages[msg_id]().unpack(buf)

    except KeyError:
        raise UnknownMessage(msg_id)

    except (struct.error, UnicodeDecodeError) as e:
        raise InvalidMessage(msg_id, len(buf), e)


class Team(object):
    def __init__(self, service_id, group, priority, port):
        self.service_id = service_id
        self.group = group
        self.port = port
        self.priority = priority

        self.uptime = 0

        self.state = STATE_LISTEN

        self.best_offer = self.offer

    def __str__(self):
        return f'Team: {self.service_id}:{self.group}'

    def __eq__(self, other):
        return self.key == other.key

    @property
    def key(self):
        return (self.service_id << 64) + self.group

    @property
    def flags(self):
        flags = SERVICE_OFFER_FLAGS_TEAM

        if self.state == STATE_SERVER:
            flags |= SERVICE_OFFER_FLAGS_SERVER

        return flags

    @property
    def offer(self):
        offer = ServiceOffer(
            id=self.service_id,
            group=self.group,
            priority=self.priority,
            port=self.port,
            uptime=self.uptime,
            flags=self.flags)
        
        return offer

    def _process_offer(self, offer):
        if offer > self.best_offer:
            print("received better offer")

            self.best_offer = offer


class ServiceManager(Ribbon):
    def initialize(self, 
                   name='service_manager'):

        self.name = name

        self.__service_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__service_sock.bind(('0.0.0.0', SERVICES_PORT))

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

        self._offers = []
        self._teams = {}
        self._services = []
        
        # self._last_announce = time.time() - 10.0
        
        
    def clean_up(self):
        pass


    def join_team(self, service_id, group, port, priority=0):
        team = Team(service_id, group, priority, port)

        assert team.key not in self._teams
        
        self._teams[team.key] = team
        team._process_offer(team.offer)

    def _send_msg(self, msg, host):
        s = self.__service_sock

        try:
            if host[0] == '<broadcast>':
                send_udp_broadcast(s, host[1], msg.pack())

            else:
                s.sendto(msg.pack(), host)

        except socket.error:
            pass

    def _send_query(self, service_id, group, host=('<broadcast>', SERVICES_PORT)):
        msg = ServiceMsgQuery(
                id=service_id,
                group=group)
        
        self._send_msg(msg, host)
    
    def _handle_offers(self, msg, host):
        for offer in msg.offers:
            try:
                self._teams[offer.key]._process_offer(offer)

            except KeyError:
                continue

    def _handle_query(self, msg, host):
        pass

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

                    response, host = self._process_msg(msg, host)
                    
                    if response:
                        response.header.transaction_id = msg.header.transaction_id
                        self._send_msg(response, host)

                except InvalidVersion:
                    pass

                except UnknownMessage as e:
                    raise

                except Exception as e:
                    logging.exception(e)


        except select.error as e:
            logging.exception(e)

        except Exception as e:
            logging.exception(e)


def main():
    util.setup_basic_logging(console=True)

    s = ServiceManager()

    s.join_team(232457833, 15608638596488529903, 0, 0)

    try:
        while True:
            time.sleep(1.0)

    except KeyboardInterrupt:
        pass

    s.stop()
    s.join()

if __name__ == '__main__':
    main()

