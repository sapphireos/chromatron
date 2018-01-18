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

"""UDPX socket module.

UDPX is a thin protocol on top of UDP that provides acknowledged datagram
delivery with an automatic repeat request mechanism in the client.


"""


import socket
import random
import time

import bitstring



class Packet(object):

    VERSION = 0
    HEADER_FORMAT = 'uint:2, uint:1, uint:1, uint:1, uint:3, uint:8'

    def __init__(self,
                 server=False,
                 ack_request=True,
                 ack=False,
                 id=None,
                 data=''):

        self.__version  = Packet.VERSION

        self.__server = 0
        self.__ack_request = 0
        self.__ack = 0

        if server:
            self.__server = 1

        if ack_request:
            self.__ack_request = 1

        if ack:
            self.__ack = 1

        if id is None:
            id = random.randint(0,255)

        self.__id = id

        self.__payload = data

        self.time = None

    def __str__(self):

        s = "Ver:%d | Svr:%d | Arq:%d | Ack:%d | ID:%3d | PayloadLength:%3d" % \
            (self.__version,
             self.__server,
             self.__ack_request,
             self.__ack,
             self.__id,
             len(self.__payload))

        if self.time:
            s += " | Time(ms):%5d" % (self.time * 1000)

        return s

    def pack(self):
        header = bitstring.pack(Packet.HEADER_FORMAT,
                                self.__version,
                                self.__server,
                                self.__ack_request,
                                self.__ack,
                                0,
                                self.__id)

        return header.bytes + self.__payload

    def unpack(self, data):
        s = bitstring.BitStream(bytes=data[0:2])
        header = s.unpack(Packet.HEADER_FORMAT)

        self.__version      = header[0]
        self.__server       = header[1]
        self.__ack_request  = header[2]
        self.__ack          = header[3]
        # header[4] is reserved bits
        self.__id           = header[5]

        self.__payload = data[2:]

        return self

    def get_version(self):
        return self.__version

    def get_server(self):
        return self.__server != 0

    def get_ack_request(self):
        return self.__ack_request != 0

    def get_ack(self):
        return self.__ack != 0

    def get_id(self):
        return self.__id

    def get_payload(self):
        return self.__payload

    def set_payload(self, value):
        self.__payload = value

    version = property(get_version)
    server = property(get_server)
    ack_request = property(get_ack_request)
    ack = property(get_ack)
    id = property(get_id)
    payload = property(get_payload, set_payload)


class ClientSocket(object):

    DEFAULT_TRIES = 10
    INITIAL_TIMEOUT = 1.0
    MIN_TIMEOUT = 0.25

    def __init__(self, addr_family=None, sock_type=None):
        self._sock = socket.socket(socket.AF_INET,
                                    socket.SOCK_DGRAM)

        self._min_timeout = ClientSocket.MIN_TIMEOUT
        self._tries = ClientSocket.DEFAULT_TRIES
        self._initial_timeout = ClientSocket.INITIAL_TIMEOUT

        self._received_data = None
        self._received_addr = None

        self._packet_id = random.randint(0,255)

        self.rtt = -1
        self.filter = 0.1
        self.timeout_multiplier = 3.0

    def flush(self):
        self._received_data = None
        self._received_addr = None

        # set sock to non-blocking
        self._sock.setblocking(0)
        try:
            while True:
                ack_data, host = self._sock.recvfrom(4096)

        except socket.error:
            pass

        # set back to blocking
        self._sock.setblocking(1)

    def bind(self, address):
        self._sock.bind(address)

    def settimeout(self, seconds):
        self._initial_timeout = seconds

    def setsockopt(self, level, optname, value):
        self._sock.setsockopt(level, optname, value)

    def sendto(self, data, address):

        # Flush socket.
        # This is really important.  If we bail out from sendto() due to some error condition
        # and the socket then receives a packet, the next time we call sendto() we will receive
        # the previous buffered packet (from the cancelled transaction).  This will often cause
        # a retry because the ack packet will have the wrong ID from the packet we just sent.
        # However, you can't rely on that, because the ID space is only 8 bits, so an ack packet
        # with a matching ID but incorrect data may still get through.
        #
        self.flush()

        # cannot broadcast on a client socket
        assert address[0] != '<broadcast>'

        self._packet_id += 1
        self._packet_id %= 256
        
        self._sock.connect(address)

        # build data packet
        packet = Packet(data=data, id=self._packet_id)

        start = time.time()

        # print '',
        # retry loop
        for i in xrange(self._tries):

            # set initial timeout
            timeout = self._initial_timeout
            # timeout = 2.0

            # set timeout
            # print i, timeout
            self._sock.settimeout(timeout)

            rtt_start = time.time()

            # send packet
            try:
                self._sock.send(packet.pack())

            except socket.error:
                # we'll get this if the host is unreachable,
                # or if some other error occurred
                raise


            # wait for timeout or received data
            try:
                ack_data, host = self._sock.recvfrom(4096)

                # parse ack
                ack_msg = Packet().unpack(ack_data)

                # print packet
                # print ack_msg, host
                # print hex(ord(ack_data[0])), hex(ord(ack_data[1]))

                # check packet for errors
                if ack_msg.version != ack_msg.VERSION:
                    raise InvalidPacketException(packet)

                elif not ack_msg.server:
                    raise InvalidPacketException(packet)

                elif ack_msg.ack_request:
                    raise InvalidPacketException(packet)

                elif not ack_msg.ack:
                    raise InvalidPacketException(packet)

                ack_msg.time = time.time() - start
                rtt = time.time() - rtt_start

                # initialize RTT
                if self.rtt < 0:
                    self.rtt = rtt

                else:
                    # update filtered rtt
                    self.rtt = (self.filter * rtt) + ((1.0 - self.filter) * self.rtt)

                    # update timeout
                    self._initial_timeout = self.timeout_multiplier * self.rtt

                if self._initial_timeout < self._min_timeout:
                    self._initial_timeout = self._min_timeout

                # print rtt, self.rtt, self._initial_timeout
                # print host, ack_msg

                if ack_msg.id != packet.id:
                    # print "Incorrect msg ID %x != %x" % (ack_msg.id, packet.id)
                    # raise InvalidPacketException(packet)   

                    self._initial_timeout += 1.0

                    continue

                # set data and server host buffers
                self._received_data = ack_msg.payload
                self._received_addr = host

                # print "message completed on try: %d" % (i)

                return

            except socket.timeout:

                # print "timeout"

                # increase timeout, exponential backoff
                timeout = timeout * 2.0

            except socket.error:
                raise

        # we didn't receive an ack, raise the timeout exception
        raise socket.timeout


    def recvfrom(self, bufsize=4096):
        # check if there is already data waiting from a transaction
        if self._received_data is not None:
            data = self._received_data
            addr = self._received_addr

            self._received_data = None
            self._received_addr = None

            return data, addr

        else:
            raise InvalidOperationException("recvfrom() but no ack from server")

    def close(self):
        self._sock.close()

    def shutdown(self, how=socket.SHUT_RDWR):
        try:
            self._sock.shutdown(how)

        except socket.error:
            pass


class TransparentClientSocket(ClientSocket):
    def sendto(self, data, address):
        # build data packet
        packet = Packet(data=data)

        self._sock.settimeout(self._initial_timeout)
        
        self._sock.sendto(packet.pack(), address)  

    def recvfrom(self, bufsize=4096):
        ack_data, host = self._sock.recvfrom(4096)

        # parse ack
        ack_msg = Packet().unpack(ack_data)

        if ack_msg.version != ack_msg.VERSION:
            raise InvalidPacketException(packet)

        elif not ack_msg.server:
            raise InvalidPacketException(packet)

        elif ack_msg.ack_request:
            raise InvalidPacketException(packet)

        return ack_msg.payload, host


class ServerSocket(object):

    def __init__(self, addr_family=None, sock_type=None):
        self.__sock = socket.socket(socket.AF_INET,
                                    socket.SOCK_DGRAM)

        self.__ack_host = None
        self.__ack_packet = None

    def bind(self, address):
        self.__sock.bind(address)

    def setsockopt(self, level, optname, value):
        self.__sock.setsockopt(level, optname, value)

    def settimeout(self, seconds):
        self.__sock.settimeout(seconds)

    def getsockname(self):
        return self.__sock.getsockname()

    def sendto(self, data="", address=None):
        # check if there is an ack packet to send
        if self.__ack_packet:

            # attach data to packet
            self.__ack_packet.payload = data

            # send packet
            self.__sock.sendto(self.__ack_packet.pack(), self.__ack_host)

            # delete ack packet and host
            self.__ack_host = None
            self.__ack_packet = None

        else:
            raise InvalidOperationException("sendto() but no message from client")

    def recvfrom(self, bufsize=4096):
        # receive packet and host address
        try:
            data, host = self.__sock.recvfrom(bufsize)

            # parse packet
            packet = Packet().unpack(data)

            # check packet for errors
            if packet.version != packet.VERSION:
                raise InvalidPacketException(packet)

            elif packet.server:
                raise InvalidPacketException(packet)

            elif packet.ack:
                raise InvalidPacketException(packet)

            # build and save ack packet
            self.__ack_packet = Packet(server=True,
                                       ack_request=False,
                                       ack=True,
                                       id=packet.id,
                                       data=packet.payload)

            # save client host
            self.__ack_host = host

            return packet.payload, host

        except socket.timeout:
            raise socket.timeout


class EchoServer(object):
    def __init__(self, address):
        self.__sock = ServerSocket()
        self.__sock.bind(address)

    def serve_forever(self):
        while True:
            data, host = self.__sock.recvfrom()
            self.__sock.sendto(data)

class InvalidOperationException(Exception):
    def __init__(self, value=None):
        self.value = value

    def __str__(self):
        return repr(self.value)

class InvalidPacketException(Exception):
    def __init__(self, value=None):
        self.value = value

    def __str__(self):
        return repr(self.value)


if __name__ == '__main__':

    c = ClientSocket()


    c.sendto("Jeremy", ("192.168.2.233", 1234))

    print c.recvfrom()
