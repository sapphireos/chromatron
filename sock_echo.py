
import struct
import numpy
import time
import socket
import sys
import random

MAX_DATA_LEN = 548


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# sock.settimeout(0.3)
sock.settimeout(1.0)

# host = ('10.0.1.13', 7)
host = (sys.argv[1], 7)
# host = ('10.0.1.74', 7)
# host = ('10.0.1.13', 12345)

errors = 0
lost = 0
sent = 0
timeout_run = 0
longest_timeout_run = 0

msg_number = 0

times = []

try:
    while True:

        msg_number += 1
        # time.sleep(0.10)  

        # generate random len
        # data_len = random.randint(4, MAX_DATA_LEN)
        # data_len = MAX_DATA_LEN
        # data_len = 463
        data_len = 64

        # generate random data
        # data = ''
        data = struct.pack('<L', msg_number)
        data += bytes([0] * (data_len - 4))

        start = time.time()

        sent += 1

        sock.sendto(data, host)

        try:
            reply_data, host = sock.recvfrom(1024)
            # time.sleep(0.005)


            elapsed = time.time() - start

            timeout_run = 0

            # print host, len(reply_data), elapsed * 1000

            # skip first sample, it can be really long due to ARP query
            if sent > 1:
                times.append(elapsed * 1000)

            print("S %6d L %6d E %6d R %6d     %04.1f len %3d msg %5d %4x" % (sent, lost, errors, longest_timeout_run, round(elapsed * 1000, 1), data_len, msg_number, msg_number))
            # raise KeyboardInterrupt

            if reply_data != data:
                errors += 1
                print("DATA ERROR: %d %d" % (len(reply_data), len(data)))
                time.sleep(0.5)
                # for i in xrange(len(data)):
                #     if reply_data[i] != data[i]:
                #         print "#%d %d != %d" % (i, reply_data[i], data[i])



            # break

        except KeyboardInterrupt:
            raise

        except:
            timeout_run += 1
            if timeout_run > longest_timeout_run:
                longest_timeout_run = timeout_run

            lost += 1
            print("timed out", "S %6d L %6d E %6d R %6d     %04.1f len %3d msg %5d" % (sent, lost, errors, longest_timeout_run, round(elapsed * 1000, 1), data_len, msg_number))

except KeyboardInterrupt:
    pass



print('')
print("sent, lost, errors")
print(sent, lost, errors)
print("min, avg, max, stddev")
print(round(min(times), 1), round(sum(times) / len(times), 1), round(max(times), 1), round(numpy.std(times), 1))


