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

import serial
from elysianfields import *

try:
    import serial.tools.list_ports
except ImportError:
    pass

import crcmod
from . import sapphiredata
import socket
import struct
import time
import threading
from queue import Queue

MFG_NAME = "Sapphire Open Systems"

SERIAL_SOF = 0xfd
SERIAL_ACK = 0xa1
SERIAL_NAK = 0x1b

# legacy:
CMD_USART_VERSION_LEGACY = 0x01

# UDP serial transport:
CMD_USART_VERSION = 0x02
CMD_USART_UDP_SOF = 0x85
CMD_USART_UDP_ACK = 0x97
CMD_USART_UDP_NAK = 0x14

CHROMATRON_VID = 0xF055
CHROMATRON_PID = 0x73D5

CP2104_VID = 0x10C4
CP2104_PID = 0xEA60

CMD2_UDP_PORT = 16386

class ChannelException(Exception):
    def __init__(self, value = None):
        self.value = value

    def __str__(self):
        return repr(self.value)

class ChannelTimeoutException(ChannelException):
    pass

class ChannelErrorException(ChannelException):
    pass

class ChannelUnreachableException(ChannelException):
    pass

class ChannelInvalidHostException(ChannelException):
    pass

class Channel(object):
    def __init__(self, host, channel_type='none'):
        self.host = host
        self.channel_type = channel_type

    def __del__(self):
        self.close()

    def open(self):
        raise NotImplementedError

    def close(self):
        raise NotImplementedError

    def read(self):
        raise NotImplementedError

    def write(self, data, port=None):
        raise NotImplementedError

    def settimeout(self, timeout=None):
        raise NotImplementedError

class NullChannel(Channel):
    def __init__(self, host):
        super(NullChannel, self).__init__(host)

    def open(self):
        pass

    def close(self):
        pass

    def read(self):
        pass

    def write(self, data):
        pass

    def settimeout(self, timeout=None):
        pass


UART_RX_BUF_SIZE = 255

class SerialChannel(Channel):
    def __init__(self, host):
        super(SerialChannel, self).__init__(host, 'serial')

        self.open(host)

        # set up crc function
        self.crc_func = crcmod.predefined.mkCrcFun('crc-aug-ccitt')

    def open(self, host):
        self.port = serial.Serial(host, baudrate=2000000)
        self.settimeout(timeout=2.0)
        self.host = self.port.port

    def close(self):
        self.port.close()

    def _read_data(self, length):
        start = time.time()
        data = ''

        while len(data) < length:
            data += self.port.read(length)

            if (len(data) < length) and ((time.time() - start) > self.port.timeout):
                # raise ChannelTimeoutException()
                return data

        return data

    def read(self):
        try:
            tries = 4

            while tries > 0:

                tries -= 1

                header = sapphiredata.SerialFrameHeader()

                # wait for header data
                header_data = self._read_data(header.size())

                # unpack header
                header.unpack(header_data)

                # check header
                if ( ( header.len != ( ~header.inverted_len & 0xffff ) ) ):
                    print("read header error")
                    continue

                # receive data
                data = self._read_data(header.len)
                if len(data) != header.len:
                    print("invalid data length")
                    print(header.len, len(data))

                    raise ChannelErrorException()


                # receive CRC
                crc = struct.unpack('>H', self._read_data(2))[0]

                # check crc
                if self.crc_func(data) == crc:
                    return data

                print("read crc error: %x %x" % (crc, self.crc_func(data)))
                print(header.len, len(data))

                raise ChannelErrorException()

            if tries == 0:
                print("read tries exceeded")
                raise ChannelErrorException()

        except serial.SerialTimeoutException:
            print("read timeout")
            raise ChannelTimeoutException

        except struct.error:
            # print "read struct error"
            raise ChannelErrorException


    def _write_data(self, data):
        while len(data) > 0:
            count = len(data)

            if count > UART_RX_BUF_SIZE:
               count = UART_RX_BUF_SIZE

            self.port.write(chr(count))
            space = ord(self.port.read(1))

            if space < count:
                count = space

            if count == 0:
                time.sleep(0.002)
                continue

            self.port.write(data[:count])

            # slice data
            data = data[count:]

    def write(self, data, port=None):
        tries = 4

        while tries > 0:

            tries -= 1

            try:
                # flush serial input buffer
                self.port.flushInput()

                self._write_data(struct.pack('B', SERIAL_SOF))

                # set up header
                header = sapphiredata.SerialFrameHeader(len=len(data),
                                                        inverted_len=(~len(data) & 0xffff))

                self._write_data(header.pack())

                # send data
                self._write_data(data)

                # compute crc
                crc = self.crc_func(data)

                # send CRC
                self._write_data(struct.pack('>H', crc)) # note the big endian!

                # wait for ACK
                response = struct.unpack('B', self.port.read(1))[0]

                if response == SERIAL_ACK:
                    return

                else:
                    error = struct.unpack('B', self.port.read(1))[0]
                    print("error: %d" % (error))

                    time.sleep(0.5)

            except Exception as e:
                if tries == 0:
                    raise

                # try to reopen port
                self.close()
                self.open(self.host)


        if tries == 0:
            print("write tries exceeded")
            raise ChannelErrorException()

    def settimeout(self, timeout):
        self.port.timeout = timeout


class CmdUsartUDPHeaderLegacy(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="version"),
                  Uint16Field(_name="lport"),
                  Uint16Field(_name="rport"),
                  Uint8Field(_name="reserved"),
                  Uint16Field(_name="data_len"),
                  Uint16Field(_name="header_crc")]

        super(CmdUsartUDPHeaderLegacy, self).__init__(_name="cmd_usart_udp_header_legacy", _fields=fields, **kwargs)

class CmdUsartUDPHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="lport"),
                  Uint16Field(_name="rport"),
                  Uint16Field(_name="data_len"),
                  Uint16Field(_name="header_crc")]

        super(CmdUsartUDPHeader, self).__init__(_name="cmd_usart_udp_header", _fields=fields, **kwargs)


class SerialUDPChannel(Channel):
    def __init__(self, host):
        super(SerialUDPChannel, self).__init__(host, 'serial_udp')

        # set up crc function
        self.crc_func = crcmod.predefined.mkCrcFun('crc-aug-ccitt')

        self.lport = 1
        self.rport = 1

        self.open(host)

    def open(self, host=None):
        if host is None:
            host = self.host

        self.port = serial.Serial(host, baudrate=115200)
        self.settimeout(timeout=0.5)
        self.host = self.port.port

    def close(self):
        try:
            self.port.close()
        except AttributeError:
            pass

    def _read_data(self, length):
        start = time.time()
        data = bytes()

        while len(data) < length:
            data += self.port.read(length)

            if (len(data) < length) and ((time.time() - start) > self.port.timeout):
                # raise ChannelTimeoutException()
                return data

        return data

    def test(self):
        self.write('', tries=1)

    def read(self):
        try:
            header = CmdUsartUDPHeader()

            # wait for SOF
            sof = struct.unpack('<B', self._read_data(1))[0]

            if sof != CMD_USART_UDP_SOF:
                print('sof error', sof)
                raise ChannelErrorException()

            # wait for version
            version = struct.unpack('<B', self._read_data(1))[0]

            if version != CMD_USART_VERSION:
                print('version error', version)
                raise ChannelErrorException()

            # wait for header data
            header_data = self._read_data(header.size())

            # unpack header
            header.unpack(header_data)

            header_crc = header.header_crc
            header.header_crc = 0
            computed_crc = self.crc_func(header.pack())

            # check header
            if header_crc != computed_crc:
                print("read header error")
                raise ChannelErrorException()

            # receive data
            data = self._read_data(header.data_len)
            if len(data) != header.data_len:
                print("invalid data length")
                print(header.data_len, len(data))

                raise ChannelErrorException()

            # receive CRC
            crc = struct.unpack('<H', self._read_data(2))[0]

            # check crc
            if self.crc_func(data) == crc:
                return data

            print("read crc error: %x %x" % (crc, self.crc_func(data)))
            print(header.len, len(data))

            raise ChannelErrorException()

        except serial.SerialTimeoutException:
            print("read timeout")
            raise ChannelTimeoutException

        except struct.error:
            # print "read struct error"
            raise ChannelErrorException


    def _write_data(self, data):
        self.port.write(data)
        
    def write(self, data, port=None, tries=None):
        if tries == None:
            tries = 1

        if port:
            rport = port
        else:
            rport = self.rport

        while tries > 0:
            tries -= 1

            try:
                # flush serial input buffer
                self.port.flushInput()

                self._write_data(struct.pack('B', CMD_USART_UDP_SOF))

                self._write_data(struct.pack('B', CMD_USART_VERSION))

                # set up header
                header = CmdUsartUDPHeader(
                            lport=self.lport,
                            rport=rport,
                            data_len=len(data))

                crc = self.crc_func(header.pack())
                header.header_crc = crc

                self._write_data(header.pack())

                # check if sending data.
                # we can send an empty message as a comms check.
                if len(data) > 0:
                    # send data
                    self._write_data(data)

                    # compute crc
                    crc = self.crc_func(data)

                    # send CRC
                    self._write_data(struct.pack('<H', crc))

                # wait for ACK
                response = struct.unpack('B', self.port.read(1))[0]

                if response == CMD_USART_UDP_ACK:
                    return

                else:
                    error = struct.unpack('B', self.port.read(1))[0]
                    print("error: %d" % (error))

                    time.sleep(0.5)

            except Exception as e:
                # if tries == 0:
                    # raise

                print(e)

                # try to reopen port
                self.close()
                self.open(self.host)


        if tries == 0:
            print("write tries exceeded")
            raise ChannelErrorException()

    def settimeout(self, timeout):
        self.port.timeout = timeout




class USBChannel(SerialChannel):
    def __init__(self, host):
        super(USBChannel, self).__init__(host)

        self.open(host)

    def close(self):
        try:
            super(USBChannel, self).close()

        except AttributeError:
            pass

        except TypeError:
            # we'll get a TypeError when __del__ is called
            pass

    def open(self, host):
        try:
            self.port = serial.Serial(host, baudrate=2000000)
            self.settimeout(timeout=2.0)
            self.host = self.port.port

        except serial.serialutil.SerialException:
            raise ChannelInvalidHostException("Could not find attached device")


class USBUDPChannel(SerialUDPChannel):
    def __init__(self, host):
        super(USBUDPChannel, self).__init__(host)

    def close(self):
        try:
            super(USBUDPChannel, self).close()

        except AttributeError:
            pass

        except TypeError:
            # we'll get a TypeError when __del__ is called
            pass

    def open(self, host=None):
        if host is None:
            host = self.host

        try:
            self.port = serial.Serial(host, baudrate=2000000)
            self.settimeout(timeout=0.5)
            self.host = self.port.port

        except serial.serialutil.SerialException:
            raise
            # raise ChannelInvalidHostException("Could not find attached device: %s" % (host))



class UDPSerialBridge(threading.Thread):
    def __init__(self, channel, rport):
        super(UDPSerialBridge, self).__init__()

        self.channel = channel
        self.rport = rport
        self.daemon = True

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('localhost', 0))

        self.port = self.sock.getsockname()[1]

        super(UDPSerialBridge, self).start()

    def run(self):
        while True:
            try:
                self.sock.settimeout(1.0)
                data, host = self.sock.recvfrom(4096)

                # print(time.time(), "SOCK RECV", len(data))

                self.channel.write(data, port=self.rport)

                # print(time.time(), "SERIAL WRITE", len(data))

                try:
                    response = self.channel.read()

                    # print(time.time(), "SERIAL READ", len(response))

                    self.sock.sendto(response, host)

                    # print(time.time(), "SOCK SEND", len(response))

                except serial.serialutil.SerialException:
                    raise

                except Exception as e:
                    # flush
                    print(e)
                    while True:
                        try:
                            self.sock.settimeout(0.1)
                            data, host = self.sock.recvfrom(4096)
                            
                        except socket.timeout:
                             break
    
        
            except socket.timeout:
                pass

            except serial.serialutil.SerialException:
                time.sleep(1.0)

                # try to reopen port
                self.channel.close()
                self.channel.open()

            except Exception as e:
                print(e)
                pass

class NotSerialChannel(Exception):
    pass

class InvalidChannel(Exception):
    pass

def createSerialChannel(host, port=None):
    if host == 'usb':
        comport = None

        # scan ports
        ports = list(serial.tools.list_ports.comports())

        # look for chromatron
        for port in ports:
            if port.vid == CHROMATRON_VID and port.pid == CHROMATRON_PID:
                comport = port.device
                ch = USBUDPChannel(comport)

                try:
                    ch.test()

                    return ch

                except ChannelErrorException:
                    ch.close()


        if comport == None:
            # try to find CP2104
            for port in ports:
                if port.vid == CP2104_VID and port.pid == CP2104_PID:
                    comport = port.device

                    return SerialUDPChannel(comport)

            if comport == None:
                raise ChannelInvalidHostException("Could not find attached device")

    try:
        socket.inet_aton(host)
        raise NotSerialChannel

    except NotSerialChannel:
        raise

    except:
        try:
            return SerialUDPChannel(host)

        except:
            raise InvalidChannel


if __name__ == '__main__':
    ports = list(serial.tools.list_ports.comports())

    for port in ports:
        print(port.device, port.vid, port.pid, port.manufacturer, port.product)
