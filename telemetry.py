
from pprint import pprint
from elysianfields import *


class TelemetryLog0(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="sys_time"),
                  Uint64Field(_name="src_addr"),
                  Int16Field(_name="rssi"),
                  Int16Field(_name="snr"),
                  Uint16Field(_name="sample"),
                  Uint16Field(_name="batt_volts"),
                  Uint16Field(_name="charge_current"),
                  Uint32Field(_name="als_filtered"),
                  Int8Field(_name="batt_temp"),
                  Int8Field(_name="case_temp"),
                  Int8Field(_name="ambient_temp"),
                  Uint8Field(_name="batt_fault")]

        super().__init__(_name="telemetry_msg_0_t", _fields=fields, **kwargs)


with open('telemetry_log', 'rb') as f:
    data = f.read()


chunk_len = TelemetryLog0().size()

SPIRES = [
    # 0,
    101469591321096,
    259794970753544,
    48693033187848,
    272989110286856,
    53091079698952,
    198242321695656,    
]

NAMES = {
    198242321695656: 'spire1',
    53091079698952: 'spire2',
    272989110286856: 'spire3',
    101469591321096: 'spire4',
    48693033187848: 'spire5',
    259794970753544: 'spire6',
}

nodes = {}

while len(data) > 0:
    chunk = data[:chunk_len]
    data = data[chunk_len:]

    log_info = TelemetryLog0().unpack(chunk)

    # if log_info.src_addr not in SPIRES:
    #     continue

    # # print(log_info.src_addr)
    # # print(log_info.src_addr in NAMES)
    # if log_info.src_addr in NAMES:
    #     # print('meow')
    #     log_info.src_addr = NAMES[log_info.src_addr]

    nodes[log_info.src_addr] = log_info

    # print(log_info)
for k, v in nodes.items():
    # print(hex(k))
    # print(v)
    print(v)

    


