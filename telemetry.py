
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

while len(data) > 0:
	chunk = data[:chunk_len]

	log_info = TelemetryLog0().unpack(chunk)

	print(log_info)

	data = data[chunk_len:]


