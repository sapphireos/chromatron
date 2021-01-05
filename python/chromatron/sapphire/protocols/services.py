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

SERVICES_PORT               = 32041
SERVICES_MAGIC              = 0x56524553 # 'SERV'
SERVICES_VERSION            = 2


SERVICE_LISTEN_TIMEOUT              = 10
SERVICE_CONNECTED_TIMEOUT           = 64
SERVICE_CONNECTED_PING_THRESHOLD    = 48
SERVICE_CONNECTED_WARN_THRESHOLD    = 16


class UnknownMessage(Exception):
    pass

class InvalidMessage(Exception):
    pass

class InvalidVersion(Exception):
    pass

class ServiceNotConnected(Exception):
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
STATE_CONNECTED = 1
STATE_SERVER    = 2

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
        self._service_id = service_id
        self._group = group
        self._port = port
        self._priority = priority

        self._reset()

    def __str__(self):
        return f'Team: {self.service_id}:{self.group}'

    def __eq__(self, other):
        return self.key == other.key

    def _reset(self):
        self._uptime = 0
        self._state = STATE_LISTEN
        self._best_offer = None
        self._best_host = None
        self._timeout = SERVICE_LISTEN_TIMEOUT

    @property
    def key(self):
        return (self._service_id << 64) + self._group

    @property
    def _flags(self):
        flags = SERVICE_OFFER_FLAGS_TEAM

        if self._state == STATE_SERVER:
            flags |= SERVICE_OFFER_FLAGS_SERVER

        return flags

    @property
    def _offer(self):
        offer = ServiceOffer(
            id=self._service_id,
            group=self._group,
            priority=self._priority,
            port=self._port,
            uptime=self._uptime,
            flags=self._flags)
        
        return offer

    @property
    def connected(self):
        return self._state != STATE_LISTEN

    @property
    def server(self):
        if not self.connected:
            raise ServiceNotConnected

        return self._best_host

    def _process_offer(self, offer, host):
        if (self._best_offer is None) or (offer > self._best_offer):
            if self._best_host != host:
                logging.debug(f"Tracking host: {host}")

            if ((self._best_offer is None) or not self._best_offer.server_valid) and offer.server_valid:
                logging.debug(f"Server is valid: {host}")

            self._best_offer = offer
            self._best_host = host

    def _process_timer(self, elapsed):
        self._timeout -= elapsed

        if self._timeout > 0.0:
            # pre-timeout
            return

        # timeout
        if self._state == STATE_LISTEN:
            assert self._priority == 0

            # check if we have a server available
            if (self._best_offer is not None) and self._best_offer.server_valid:
                logging.debug(f"CONNECTED to: {self._best_host}")
                self._state = STATE_CONNECTED
                self._timeout = SERVICE_CONNECTED_TIMEOUT

        elif self._state == STATE_CONNECTED:
            logging.debug(f"CONNECTED timeout: lost server")
            self._reset()

        else:
            assert False


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

        self.__send_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.__send_sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

        self._port = self.__service_sock.getsockname()[1]

        logging.info(f"ServiceManager on port: {self._port}")

        self.__service_sock.setblocking(0)

        self._inputs = [self.__service_sock, self.__send_sock]
        
        self._msg_handlers = {
            ServiceMsgOffers: self._handle_offers,
            ServiceMsgQuery: self._handle_query,
        }

        self._offers = []
        self._teams = {}
        self._services = []
        
        self._last_timer = time.time()
        
        
    def clean_up(self):
        pass


    def join_team(self, service_id, group, port, priority=0):
        if priority != 0:
            raise NotImplementedError("Services only available as follower")

        team = Team(service_id, group, priority, port)

        assert team.key not in self._teams
        
        self._teams[team.key] = team

        self._send_query(service_id, group)

        return team

    def _send_msg(self, msg, host):
        s = self.__send_sock

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
                self._teams[offer.key]._process_offer(offer, host)

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

        now = time.time()
        elapsed = now - self._last_timer
        self._last_timer = now
        
        for team in self._teams.values():
            team._process_timer(elapsed)

def main():
    util.setup_basic_logging(console=True)

    s = ServiceManager()

    team = s.join_team(0x1234, 0, 0, 0)

    try:
        while True:
            # if team.connected:
            #     print(team.server)

            time.sleep(1.0)


    except KeyboardInterrupt:
        pass

    s.stop()
    s.join()

if __name__ == '__main__':
    main()
    