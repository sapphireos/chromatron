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

import os
import sys
import time
import logging
import threading
from elysianfields import *
from ..common import MsgServer, run_all, util, catbus_string_hash, synchronized

SERVICES_PORT               = 32041
SERVICES_MAGIC              = 0x56524553 # 'SERV'
SERVICES_VERSION            = 3

SERVICES_MCAST_ADDR         = "239.43.96.31"

SERVICE_LISTEN_TIMEOUT              = 10.0
SERVICE_CONNECTED_TIMEOUT           = 32.0
SERVICE_CONNECTED_PING_THRESHOLD    = 16.0
SERVICE_CONNECTED_WARN_THRESHOLD    = 4.0


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
                  Uint8Field(_name="reserved"),
                  Uint64Field(_name="origin")]

        super().__init__(_fields=fields, **kwargs)

        self.magic          = SERVICES_MAGIC
        self.version        = SERVICES_VERSION

SERVICE_FLAGS_SHUTDOWN      = 0x80


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

def make_key(service_id, group):
    if isinstance(service_id, str):
        service_id = catbus_string_hash(service_id)

    if isinstance(group, str):
        group = catbus_string_hash(group)

    return (service_id << 64) + group

class ServiceOffer(StructField):
    def __init__(self, origin=None, **kwargs):
        fields = [Uint32Field(_name="id"),
                  Uint64Field(_name="group"),
                  Uint16Field(_name="priority"),
                  Uint16Field(_name="port"),
                  Uint32Field(_name="uptime"),
                  Uint8Field(_name="flags"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=3)]

        super().__init__(_fields=fields, **kwargs)

        self.origin = origin

    def __str__(self):
        return f'Offer: {hex(self.id)}/{hex(self.group)} prio: {self.priority}'

    @property
    def key(self):
        return make_key(self.id, self.group)

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

        if self.origin > other.origin:
            return True

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
    def __init__(self, service_id, group, priority=0, port=0, origin=os.getpid(), _lock=None):
        self._service_id = service_id
        self._group = group
        self._port = port
        self._priority = priority
        self._origin = origin

        assert self._port is not None

        self._lock = _lock
        assert self._lock is not None

        self._cancelled = False

        self._reset()        

    def _init_server(self):
        if self._priority > 0: # offering a service, start out as server
            logging.debug(f"{self} SERVER")
            self._state = STATE_SERVER

    def __str__(self):
        return f'Service: {hex(self._service_id)}:{hex(self._group)}'

    def __eq__(self, other):
        return self.key == other.key

    @synchronized
    def _reset(self):
        self._uptime = 0
        self._state = STATE_LISTEN
        self._best_offer = None
        self._best_host = None
        self._timeout = SERVICE_LISTEN_TIMEOUT

        self._init_server()

    @property
    @synchronized
    def key(self):
        return (self._service_id << 64) + self._group

    @property
    @synchronized
    def _flags(self):
        flags = 0
        if self._state == STATE_SERVER:
            flags |= SERVICE_OFFER_FLAGS_SERVER

        return flags

    @property
    @synchronized
    def _offer(self):
        offer = ServiceOffer(
            origin=self._origin,
            id=self._service_id,
            group=self._group,
            priority=self._priority,
            port=self._port,
            uptime=self._uptime,
            flags=self._flags)
        
        return offer

    @property
    @synchronized
    def connected(self):
        return self._state != STATE_LISTEN

    @property
    @synchronized
    def is_server(self):
        return self._state == STATE_SERVER

    @property
    @synchronized
    def server(self):
        if not self.connected:
            raise ServiceNotConnected

        if self.is_server:
            host = ('127.0.0.1', self._port) # local server is on loopback

        else:
            host = (self._best_host[0], self._best_offer.port)
        
        return host

    def _shutdown(self):
        self._reset()

    @synchronized
    def _cancel(self):
        self._cancelled = True
        self._reset()

    def wait_until_state(self, state, timeout=60.0):
        logging.debug(f"{self} waiting for state {state}, timeout {timeout}")
        
        while self._state != state:
            time.sleep(0.1)

            if timeout > 0.0:
                timeout -= 0.1

                if timeout < 0.0:
                    raise ServiceNotConnected

    def wait_until_connected(self, timeout=60.0):
        logging.debug(f"{self} waiting for connected, timeout {timeout}")
        
        while self._state == STATE_LISTEN:
            time.sleep(0.1)

            if timeout > 0.0:
                timeout -= 0.1

                if timeout < 0.0:
                    raise ServiceNotConnected

    def _process_offer(self, offer, host):
        # logging.debug(f"Received OFFER from {host}: {offer}")

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
                if (offer.flags & SERVICE_OFFER_FLAGS_SERVER) == 0:
                    return

                if (host == self._best_host) and (self._state == STATE_CONNECTED):
                    # update timeout, we are connected to this server
                    self._timeout = SERVICE_CONNECTED_TIMEOUT

                # compare priorities
                elif (self._best_offer is None) or (offer > self._best_offer):
                    self._best_offer = offer
                    self._best_host = host

                    logging.debug(f"{self} switched to: {(self._best_host, self._best_offer.port)}")

            # NOTE non-team servers don't process offers

        # TEAM
        # update tracking
        elif (self._state != STATE_SERVER) and (self._best_host == host):
            if ((self._best_offer is None) or not self._best_offer.server_valid) and offer.server_valid:
                logging.debug(f"{self} Server is valid: {host}")

            # are we connected?
            if self._state == STATE_CONNECTED:

                assert self._best_offer is not None
                assert self._best_host is not None

                # reset timeout
                self._timeout = SERVICE_CONNECTED_TIMEOUT

                # did the server reboot and we didn't notice?
                # OR did the server change to invalid?
                diff = self._best_offer.uptime - offer.uptime
                if (diff > SERVICE_UPTIME_MIN_DIFF) or \
                   (not offer.server_valid):
                    # reset, maybe there is a better server available                    
                    logging.info(f"{self} {host} is no longer valid")

                    self._reset()

                # check if server is still better than we are
                elif self._offer > offer:
                    # no, we are better
                    logging.debug(f"{self} we are better server")
                    logging.info(f"{self} -> SERVER")
                    self._state = STATE_SERVER

            # update tracking
            self._best_offer = offer
            self._best_host = host

        # TEAM
        # state machine
        else:
            if self._state == STATE_LISTEN:
                # check if server in packet is better than current tracking   
                if (self._best_offer is None) or (offer > self._best_offer):
                    logging.debug(f"{self} state: LISTEN")
                    
                    # update tracking
                    self._best_offer = offer
                    self._best_host = host

            elif self._state == STATE_CONNECTED:
                assert self._best_offer is not None
                assert self._best_host is not None

                # check if packet is a better server than current tracking (and this packet is not from the current server)            
                if (host != self._best_host) and offer > self._best_offer:
                    logging.debug(f"{self} state: CONNECTED")

                    # update tracking
                    self._best_offer = offer
                    self._best_host = host

                    if offer.server_valid:
                        logging.debug(f"{self} hop to better server {host}")
                        self._timeout = SERVICE_CONNECTED_TIMEOUT

            elif self._state == STATE_SERVER:
                # check if this packet is better than current tracking
                if (self._best_offer is None) or (offer > self._best_offer):
                    # check if server is valid - we will only consider
                    # other valid servers, not candidates
                    if offer.server_valid:
                        # update tracking
                        self._best_offer = offer
                        self._best_host = host

                        # now that we've updated tracking
                        # check if the tracked server is better than us
                        if offer > self._offer:

                            logging.debug(f"{self} found a better server: {host}")
                            logging.info(f"{self} -> CONNECTED")

                            self._timeout = SERVICE_CONNECTED_TIMEOUT
                            self._state = STATE_CONNECTED

                        else:
                            # we are the better server.
                            # possibly the other server is missing our broadcasts (this can happen)

                            # unicast our offer directly to it
                            return 'transmit_service'

                else:
                    # received server is not as good as we are.
                    # possibly it is missing our broadcasts (this can happen)

                    # unicast our offer directly to it
                    return 'transmit_service'


    def _process_timer(self, elapsed):
        # logging.debug(f'timer: {self._state}')

        if self._state == STATE_SERVER:
            self._uptime += elapsed
            self._timeout = 0.0

        elif self._best_offer is not None:
            self._best_offer.uptime += elapsed

        if self._state != STATE_SERVER:
            self._timeout -= elapsed

        if self._timeout > 0.0:
            # pre-timeout

            if self._state == STATE_CONNECTED:
                if self._timeout <= SERVICE_CONNECTED_PING_THRESHOLD:
                    return 'ping'

        # timeout
        elif self._state == STATE_LISTEN:
            # are we follower only?
            if self._priority == 0:
                # check if we have a server available
                if (self._best_offer is not None) and self._best_offer.server_valid:
                    logging.debug(f"{self} CONNECTED to: {self._best_host}")
                    self._state = STATE_CONNECTED
                    self._timeout = SERVICE_CONNECTED_TIMEOUT

                else:
                    self._reset()

            else:
                if (self._best_offer is None) or (self._best_offer < self._offer):
                    logging.debug(f"{self} SERVER")
                    self._state = STATE_SERVER

                elif (self._best_offer is not None) and self._best_offer.server_valid:
                    logging.debug(f"{self} CONNECTED to: {self._best_host}")
                    self._state = STATE_CONNECTED
                    self._timeout = SERVICE_CONNECTED_TIMEOUT

                else:
                    self._reset()

        elif self._state == STATE_CONNECTED:
            logging.debug(f"{self} CONNECTED timeout: lost server")
            self._reset()

        
class Team(Service):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self._reset()

    def _init_server(self):
        pass

    def __str__(self):
        return f'Team: {self._service_id}:{self._group}'
    
    @property
    @synchronized
    def _flags(self):
        flags = super()._flags
        flags |= SERVICE_OFFER_FLAGS_TEAM

        return flags


class ServiceManager(MsgServer):
    def __init__(self, origin=os.getpid()):
        super().__init__(name='service_manager', listener_port=SERVICES_PORT, listener_mcast=SERVICES_MCAST_ADDR)
        
        self._services = {}
        self.origin = origin
        
        self.register_message(ServiceMsgOffers, self._handle_offers)
        self.register_message(ServiceMsgQuery, self._handle_query)
            
        self.start_timer(1.0, self._process_timers)
        self.start_timer(4.0, self._process_offer_timer)

        self.start()

    def clean_up(self):
        for svc in self._services.values():
            svc._shutdown()

        servers = [svc for svc in self._services.values() if svc.is_server]
        offers = [svc._offer for svc in servers]

        if len(offers) == 0:
            return

        self._send_offers(offers, shutdown=True)
        time.sleep(0.1)
        self._send_offers(offers, shutdown=True)
        time.sleep(0.1)
        self._send_offers(offers, shutdown=True)

    def _convert_catbus_hash(self, n):
        if isinstance(n, str):
            n = catbus_string_hash(n)

        return n

    @synchronized
    def join_team(self, service_id, group=0, port=0, priority=0):
        service_id = self._convert_catbus_hash(service_id)
        group = self._convert_catbus_hash(group)

        team = Team(service_id, group, priority, port, origin=self.origin, _lock=self._lock)

        assert team.key not in self._services

        logging.info(f"Added JOIN for {hex(service_id)}/{hex(group)}")
        
        self._services[team.key] = team

        self._send_query(service_id, group)

        return team

    @synchronized
    def offer(self, service_id, group=0, port=0, priority=1):
        assert priority != 0

        service_id = self._convert_catbus_hash(service_id)
        group = self._convert_catbus_hash(group)

        service = Service(service_id, group, priority, port, origin=self.origin, _lock=self._lock)

        assert service.is_server
        assert service.key not in self._services

        logging.info(f"Added OFFER for {hex(service_id)}/{hex(group)}")
        
        self._services[service.key] = service

        return service
    
    @synchronized
    def listen(self, service_id, group=0, queries=1):
        service_id = self._convert_catbus_hash(service_id)
        group = self._convert_catbus_hash(group)

        service = Service(service_id, group, origin=self.origin, _lock=self._lock)

        assert service.key not in self._services
        
        self._services[service.key] = service

        logging.info(f"Added LISTEN for {hex(service_id)}/{hex(group)}")

        while queries > 0:
            self._send_query(service_id, group)

            queries -= 1

            if queries > 0:
                time.sleep(0.1)

        return service

    @synchronized
    def is_connected(self, service_id, group):
        try:
            return self._services[make_key(service_id, group)].is_connected

        except KeyError:
            return False

    @synchronized
    def is_server(self, service_id, group):
        try:
            return self._services[make_key(service_id, group)].is_server

        except KeyError:
            return False

    def _send_query(self, service_id, group, host=('<broadcast>', SERVICES_PORT)):
        msg = ServiceMsgQuery(
                id=service_id,
                group=group)

        msg.header.origin = self.origin
        
        logging.debug(f"Send QUERY for {hex(service_id)}/{hex(group)} to {host}")

        self.transmit(msg, host)

    def _send_offers(self, offers, host=('<broadcast>', SERVICES_PORT), shutdown=False):
        header = ServiceMsgOfferHeader(
            count=len(offers))

        msg = ServiceMsgOffers(
                offer_header=header,
                offers=offers)

        if shutdown:
            msg.header.flags |= SERVICE_FLAGS_SHUTDOWN

        msg.header.origin = self.origin
        
        self.transmit(msg, host)
    
    def _handle_offers(self, msg, host):
        if msg.header.flags & SERVICE_FLAGS_SHUTDOWN:
            for svc in self._services.values():
                if svc.is_server:
                    continue

                try:
                    # check if host that is shutting down is a server
                    if svc.origin == origin:
                        logging.debug(f'{svc} server {svc.host} shutdown')
                        svc._reset()

                except ServiceNotConnected:
                    pass

        else:
            for offer in msg.offers:
                offer.origin = msg.header.origin # attach origin to offer
                try:
                    svc = self._services[offer.key]
                except KeyError:
                    continue

                if svc._process_offer(offer, host) == 'transmit_service':
                    # unicast offer for our server
                    self._send_offers([svc._offer], host)


    def _handle_query(self, msg, host):
        key = make_key(msg.id, msg.group)

        if key not in self._services:
            return

        # if we aren't a server, we don't need to do anything with a query
        if not self._services[key].is_server:
            return

        logging.debug(f"QUERY for {msg.id}/{msg.group}, sending OFFER to {host}")

        msg = ServiceMsgOffers(
                offer_header=ServiceMsgOfferHeader(count=1),
                offers=[self._services[key]._offer])
        
        msg.header.origin = self.origin

        self.transmit(msg, host)


    def _process_timers(self):
        # check if cancelled
        self._services = {k: v for k, v in self._services.items() if not v._cancelled}

        for svc in self._services.values():
            if svc._process_timer(1.0) == 'ping':
                self._send_query(svc._service_id, svc._group, host=svc.server)

    def _process_offer_timer(self):
        servers = [svc for svc in self._services.values() if svc.is_server]
        offers = [svc._offer for svc in servers]

        if len(offers) > 0:
            self._send_offers(offers)

def main():
    util.setup_basic_logging(console=True)

    s = ServiceManager()

    if sys.argv[1] == 'listen':
        svc = s.listen(1234, 5678)

    elif sys.argv[1] == 'offer':
        svc = s.offer(1234, 5678, 1000, priority=99)

    elif sys.argv[1] == 'join':
        svc = s.join_team(1234, 5678, 1000, priority=99)

    elif sys.argv[1] == 'listen_directory':
        svc = s.listen("directory")

        svc.wait_until_connected()

        print(svc.server)

        sys.exit(0)

    run_all()

if __name__ == '__main__':
    main()
    