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
from ..common import Ribbon, RibbonServer, util, catbus_string_hash, UnknownMessage

SERVICES_PORT               = 32041
SERVICES_MAGIC              = 0x56524553 # 'SERV'
SERVICES_VERSION            = 2

SERVICE_RATE                        = 4.0
SERVICE_LISTEN_TIMEOUT              = 10.0
SERVICE_CONNECTED_TIMEOUT           = 64.0
SERVICE_CONNECTED_PING_THRESHOLD    = 48.0
SERVICE_CONNECTED_WARN_THRESHOLD    = 16.0

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
        fields = [Uint8Field(_name="count"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=3)]

        super().__init__(_fields=fields, **kwargs)

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
        fields = [ServiceMsgHeader(_name="header"),
                  ServiceMsgOfferHeader(_name="offer_header"),
                  ArrayField(_name="offers", _field=ServiceOffer)]

        super().__init__(_fields=fields, **kwargs)

        self.header.type = SERVICE_MSG_TYPE_OFFERS

class ServiceMsgQuery(StructField):
    def __init__(self, **kwargs):
        fields = [ServiceMsgHeader(_name="header"),
                  Uint32Field(_name="id"),
                  Uint64Field(_name="group")]

        super().__init__(_fields=fields, **kwargs)

        self.header.type = SERVICE_MSG_TYPE_QUERY


class Service(object):
    def __init__(self, service_id, group, priority=0, port=0):
        self._service_id = service_id
        self._group = group
        self._port = port
        self._priority = priority

        self._reset()

    def __str__(self):
        return f'Service: {self._service_id}:{self._group}'

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
        flags = 0
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
    def is_server(self):
        return self._state == STATE_SERVER

    @property
    def server(self):
        if not self.connected:
            raise ServiceNotConnected

        return self._best_host

    def _process_offer(self, offer, host):
        # filter packet
        if isinstance(self, Team):
            if (offer.flags & SERVICE_OFFER_FLAGS_TEAM) == 0:
                return
        else:
            if (offer.flags & SERVICE_OFFER_FLAGS_TEAM) != 0:
                return

        # SERVICE
        if not isinstance(self, Team):
            if self._state in [STATE_LISTEN, STATE_CONNECTED]:
                # check if NOT a server
                if (offer.flags & SERVICE_OFFER_FLAGS_SERVER) != 0:
                    return

                if host == self._best_host:
                    # update timeout, we are connected to this server
                    self._timeout = SERVICE_CONNECTED_TIMEOUT

                # compare priorities
                elif offer > self._best_offer:
                    logging.debug(f"Service switched to: {host}")

                    self._best_offer = offer
                    self._best_host = host

        # TEAM
        elif (self._best_offer is None) or (offer > self._best_offer):
            if self._best_host != host:
                logging.debug(f"Tracking host: {host}")

            if ((self._best_offer is None) or not self._best_offer.server_valid) and offer.server_valid:
                logging.debug(f"Server is valid: {host}")

            self._best_offer = offer
            self._best_host = host

    def _process_timer(self, elapsed):
        if self._state == STATE_SERVER:
            self._uptime += elapsed

        elif self._best_offer is not None:
            self._best_offer._uptime += elapsed

        if self._state != STATE_SERVER:
            self._timeout -= elapsed

        if self._timeout > 0.0:
            # pre-timeout

            return

        # timeout
        if self._state == STATE_LISTEN:
            # are we follower only?
            if self._priority == 0:
                # check if we have a server available
                if (self._best_offer is not None) and self._best_offer.server_valid:
                    logging.debug(f"CONNECTED to: {self._best_host}")
                    self._state = STATE_CONNECTED
                    self._timeout = SERVICE_CONNECTED_TIMEOUT

                else:
                    self.reset()

            else:
                if (self._best_offer is None) or (self._best_offer < self._offer):
                    logging.debug(f"SERVER")
                    self._state = STATE_SERVER

                elif (self._best_offer is not None) and self._best_offer.server_valid:
                    logging.debug(f"CONNECTED to: {self._best_host}")
                    self._state = STATE_CONNECTED
                    self._timeout = SERVICE_CONNECTED_TIMEOUT

                else:
                    self.reset()

        elif self._state == STATE_CONNECTED:
            logging.debug(f"CONNECTED timeout: lost server")
            self._reset()

        
class Team(Service):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        assert self._priority == 0

    def __str__(self):
        return f'Team: {self._service_id}:{self._group}'
    
    @property
    def _flags(self):
        flags = super()._flags
        flags |= SERVICE_OFFER_FLAGS_TEAM

        return flags


class ServiceManager(RibbonServer):
    NAME = 'service_manager'
    PORT = SERVICES_PORT

    def initialize(self):
        self._services = {}
        
        self.register_message(ServiceMsgOffers, self._handle_offers)
        self.register_message(ServiceMsgQuery, self._handle_query)
            
        self.start_timer(1.0, self._process_timers)
        self.start_timer(4.0, self._process_offer_timer)

    def clean_up(self):
        pass

    def _convert_catbus_hash(self, n):
        if isinstance(n, str):
            n = catbus_string_hash(n)

        return n

    def join_team(self, service_id, group, port, priority=0):
        if priority != 0:
            raise NotImplementedError("Services only available as follower")

        service_id = self._convert_catbus_hash(service_id)
        group = self._convert_catbus_hash(group)

        team = Team(service_id, group, priority, port)

        assert team.key not in self._services
        
        self._services[team.key] = team

        self._send_query(service_id, group)

        return team

    def offer(self, service_id, group, port, priority=1):
        assert priority != 0

        service_id = self._convert_catbus_hash(service_id)
        group = self._convert_catbus_hash(group)

        service = Service(service_id, group, priority, port)

        assert service.key not in self._services
        
        self._services[service.key] = service

        self._send_query(service_id, group)

        return service

    def listen(self, service_id, group):
        service_id = self._convert_catbus_hash(service_id)
        group = self._convert_catbus_hash(group)

        service = Service(service_id, group)

        assert service.key not in self._services
        
        self._services[service.key] = service

        self._send_query(service_id, group)

        return service

    def _send_query(self, service_id, group, host=('<broadcast>', SERVICES_PORT)):
        msg = ServiceMsgQuery(
                id=service_id,
                group=group)
        
        self.transmit(msg, host)

    def _send_offers(self, offers, host=('<broadcast>', SERVICES_PORT)):
        header = ServiceMsgOfferHeader(
            count=len(offers))

        msg = ServiceMsgOffers(
                offer_header=header,
                offers=offers)
        
        self.transmit(msg, host)
    
    def _handle_offers(self, msg, host):
        for offer in msg.offers:
            try:
                self._services[offer.key]._process_offer(offer, host)

            except KeyError:
                continue

    def _handle_query(self, msg, host):
        pass

    def _process_timers(self):
        for svc in self._services.values():
            svc._process_timer(1.0)

    def _process_offer_timer(self):
        servers = [svc for svc in self._services.values() if svc.is_server]
        offers = [svc._offer for svc in servers]

        if len(offers) > 0:
            self._send_offers(offers)


def main():
    util.setup_basic_logging(console=True)

    s = ServiceManager()

    # team = s.join_team(0x1234, 0, 0, 0)
    svc = s.listen(1234, 5678)
    # svc = s.offer(1234, 5678, 1000, priority=99)

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
    