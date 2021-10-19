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
"""Sapphire Data
"""

from elysianfields import *
from catbus import get_field_for_type, CatbusHash, CatbusData


class FileInfoField(StructField):
    def __init__(self, **kwargs):
        fields = [Int32Field(_name="filesize"),
                  StringField(_name="filename", _length=32),
                  Uint8Field(_name="flags"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=15)]

        super(FileInfoField, self).__init__(_name="file_info", _fields=fields, **kwargs)

class FileInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = FileInfoField

        super(FileInfoArray, self).__init__(_field=field, **kwargs)

class HandleInfoField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="handle_size"),
                  Uint8Field(_name="handle_type")]

        super(HandleInfoField, self).__init__(_name="mem_info", _fields=fields, **kwargs)

class HandleInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = HandleInfoField

        super(HandleInfoArray, self).__init__(_field=field, **kwargs)

class FirmwareInfoField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="firmware_length"),
                  UuidField(_name="firmware_id"),
                  StringField(_name="os_name", _length=128),
                  StringField(_name="os_version", _length=16),
                  StringField(_name="firmware_name", _length=128),
                  StringField(_name="firmware_version", _length=16),
                  StringField(_name="board", _length=32)]

        super(FirmwareInfoField, self).__init__(_name="firmware_info", _fields=fields, **kwargs)

class SerialFrameHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="len"),
                  Uint16Field(_name="inverted_len")]

        super(SerialFrameHeader, self).__init__(_name="frame_header", _fields=fields, **kwargs)

class DnsCacheField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="status"),
                  Ipv4Field(_name="ip"),
                  Uint32Field(_name="ttl"),
                  StringField(_name="query")]

        super(DnsCacheField, self).__init__(_name="dns_cache", _fields=fields, **kwargs)

class DnsCacheArray(ArrayField):
    def __init__(self, **kwargs):
        field = DnsCacheField

        super(DnsCacheArray, self).__init__(_field=field, **kwargs)

class ThreadInfoField(StructField):
    def __init__(self, **kwargs):
        fields = [StringField(_name="name", _length=64),
                  Uint16Field(_name="flags"),
                  Uint32Field(_name="addr"),
                  Uint16Field(_name="data_size"),
                  Uint32Field(_name="run_time"),
                  Uint32Field(_name="runs"),
                  Uint16Field(_name="line"),
                  Uint64Field(_name="alarm"),
                  ArrayField(_name="reserved", _field=Uint8Field, _length=24)]

        super(ThreadInfoField, self).__init__(_fields=fields, **kwargs)

class ThreadInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = ThreadInfoField

        super(ThreadInfoArray, self).__init__(_field=field, **kwargs)

class NTPTimestampField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="seconds"),
                  Uint32Field(_name="fraction")]

        super(NTPTimestampField, self).__init__(_fields=fields, **kwargs)

    def copy(self):
        return NTPTimestampField()

class KVMetaField(StructField):
    def __init__(self, **kwargs):
        fields = [Int8Field(_name="type"),
                  Uint8Field(_name="array_count"),
                  Uint8Field(_name="flags"),
                  Uint16Field(_name="__var_ptr"),
                  Uint16Field(_name="__notifier_ptr"),
                  StringField(_name="param_name", _length=32),
                  Uint32Field(_name="hash"),
                  Uint8Field(_name="padding")]

        super(KVMetaField, self).__init__(_fields=fields, **kwargs)

class KVMetaFieldWidePtr(StructField):
    def __init__(self, **kwargs):
        fields = [Int8Field(_name="type"),
                  Uint8Field(_name="array_count"),
                  Uint8Field(_name="flags"),
                  Uint32Field(_name="__var_ptr"),
                  Uint32Field(_name="__notifier_ptr"),
                  StringField(_name="param_name", _length=32),
                  Uint32Field(_name="hash"),
                  Uint8Field(_name="padding")]

        super(KVMetaFieldWidePtr, self).__init__(_fields=fields, **kwargs)

class KVMetaArray(ArrayField):
    def __init__(self, **kwargs):
        field = KVMetaField

        super(KVMetaArray, self).__init__(_field=field, **kwargs)


class KVLegacyMetaField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="group"),
                  Uint8Field(_name="id"),
                  Int8Field(_name="type"),
                  Uint8Field(_name="array_count"),
                  Uint8Field(_name="flags"),
                  Uint16Field(_name="__var_ptr"),
                  Uint16Field(_name="__notifier_ptr"),
                  StringField(_name="param_name", _length=32)]

        super(KVLegacyMetaField, self).__init__(_fields=fields, **kwargs)

class KVLegacyMetaArray(ArrayField):
    def __init__(self, **kwargs):
        field = KVLegacyMetaField

        super(KVLegacyMetaArray, self).__init__(_field=field, **kwargs)

class KVParamField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="group"),
                  Uint8Field(_name="id"),
                  Int8Field(_name="type")]

        # look up type
        if 'type' in kwargs:
            valuefield = get_field_for_type(kwargs['type'], _name='param_value')
            fields.append(valuefield)

            if 'param_value' in kwargs:
                valuefield._value = kwargs['param_value']
                kwargs['param_value'] = valuefield._value

        super(KVParamField, self).__init__(_fields=fields, **kwargs)

    def unpack(self, buffer):
        super(KVParamField, self).unpack(buffer)

        buffer = buffer[self.size():]

        # get value field based on type
        try:
            valuefield = get_field_for_type(self.type, _name='param_value')
            valuefield.unpack(buffer)

            self._fields[valuefield._name] = valuefield

        except KeyError:
            print("ERROR:", self.type)
            print(self)
            raise

        return self

class KVParamArray(ArrayField):
    def __init__(self, **kwargs):
        field = KVParamField

        super(KVParamArray, self).__init__(_field=field, **kwargs)

class KVStatusField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="group"),
                  Uint8Field(_name="id"),
                  Int8Field(_name="status")]

        super(KVStatusField, self).__init__(_fields=fields, **kwargs)

class KVStatusArray(ArrayField):
    def __init__(self, **kwargs):
        field = KVStatusField

        super(KVStatusArray, self).__init__(_field=field, **kwargs)

class KVRequestField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint8Field(_name="group"),
                  Uint8Field(_name="id"),
                  Int8Field(_name="type")]

        super(KVRequestField, self).__init__(_fields=fields, **kwargs)

        self.__type = None

    # return size of returned param in response to this request
    def paramSize(self):
        if self.__type != self.type:
            self.__type = self.type
            self.__param_size = KVParamField(type=self.type).size()

        return self.__param_size

    # return size of returned status in response to this request
    def statusSize(self):
        return KVStatusField().size()

class KVRequestArray(ArrayField):
    def __init__(self, **kwargs):
        field = KVRequestField

        super(KVRequestArray, self).__init__(_field=field, **kwargs)

    # return size of returned param in response to this request
    def paramSize(self):
        return sum([request.paramSize() for request in self._fields])

    # return size of returned status in response to this request
    def statusSize(self):
        return sum([request.statusSize() for request in self._fields])


class ServiceInfo(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="id"),
                  Uint64Field(_name="group"),
                  
                  Uint64Field(_name="server_origin"),
                  Uint16Field(_name="server_priority"),
                  Uint32Field(_name="server_uptime"),
                  Uint16Field(_name="server_port"),
                  Ipv4Field(_name="server_ip"),
                  BooleanField(_name="server_valid"),

                  Uint16Field(_name="local_priority"),
                  Uint16Field(_name="local_port"),
                  Uint32Field(_name="local_uptime"),
                  
                  BooleanField(_name="is_team"),
                  Uint8Field(_name="state"),
                  Uint8Field(_name="timeout")]

        super(ServiceInfo, self).__init__(_fields=fields, **kwargs)

class ServiceInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = ServiceInfo

        super(ServiceInfoArray, self).__init__(_field=field, **kwargs)


class LinkInfo(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="source_key"),
                  Uint32Field(_name="dest_key"),
                  ArrayField(_name="query", _field=Uint32Field, _length=8),
                  Uint8Field(_name="mode"),
                  Uint8Field(_name="aggregation"),
                  Uint16Field(_name="filter"),
                  Uint16Field(_name="rate"),
                  Uint32Field(_name="tag"),
                  Uint32Field(_name="data_hash"),
                  Int16Field(_name="retransmit_timer"),
                  Int16Field(_name="ticks"),
                  Uint64Field(_name="hash")]

        super().__init__(_fields=fields, **kwargs)

class LinkInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = LinkInfo

        super().__init__(_field=field, **kwargs)


class LinkProducerInfo(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="source_key"),
                  Uint64Field(_name="link_hash"),
                  Ipv4Field(_name="leader_ip"),
                  Uint16Field(_name="leader_port"),
                  Uint32Field(_name="data_hash"),
                  Uint16Field(_name="rate"),
                  Int16Field(_name="ticks"),
                  Int16Field(_name="retransmit_timer"),
                  Int32Field(_name="timeout")]

        super().__init__(_fields=fields, **kwargs)

class LinkProducerInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = LinkProducerInfo

        super().__init__(_field=field, **kwargs)

class LinkConsumerInfo(StructField):
    def __init__(self, **kwargs):
        fields = [Int16Field(_name="link"),
                  Ipv4Field(_name="ip"),
                  Uint16Field(_name="port"),
                  Int32Field(_name="timeout")]

        super().__init__(_fields=fields, **kwargs)

class LinkConsumerInfoArray(ArrayField):
    def __init__(self, **kwargs):
        field = LinkConsumerInfo

        super().__init__(_field=field, **kwargs)

# this data structure needs rework to be viable
# class LinkRemoteInfo(StructField):
#     def __init__(self, **kwargs):
#         fields = [Int16Field(_name="link"),
#                   Ipv4Field(_name="ip"),
#                   Int32Field(_name="timeout")]

#         super().__init__(_fields=fields, **kwargs)

# class LinkRemoteInfoArray(ArrayField):
#     def __init__(self, **kwargs):
#         field = LinkRemoteInfo

#         super().__init__(_field=field, **kwargs)

class DatalogEntry(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusHash(_name="hash"),
                  Uint16Field(_name="rate"),
                  Uint16Field(_name="padding")]

        super().__init__(_fields=fields, **kwargs)

class DatalogEntryArray(ArrayField):
    def __init__(self, **kwargs):
        field = DatalogEntry

        super().__init__(_field=field, **kwargs)

class DatalogData(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusData(_name="data")]

        super().__init__(_fields=fields, **kwargs)



raw_events = """

#define EVENT_ID_NONE                           0
#define EVENT_ID_LOG_INIT                       1
#define EVENT_ID_LOG_RECORD                     2

#define EVENT_ID_SYS_ASSERT                     10
#define EVENT_ID_SIGNAL                         11
#define EVENT_ID_WATCHDOG_KICK                  12

#define EVENT_ID_DEBUG_0                        20
#define EVENT_ID_DEBUG_1                        21
#define EVENT_ID_DEBUG_2                        22
#define EVENT_ID_DEBUG_3                        23
#define EVENT_ID_DEBUG_4                        24
#define EVENT_ID_DEBUG_5                        25
#define EVENT_ID_DEBUG_6                        26
#define EVENT_ID_DEBUG_7                        27

#define EVENT_ID_CPU_SLEEP                      200
#define EVENT_ID_CPU_WAKE                       201

#define EVENT_ID_FFS_GARBAGE_COLLECT            400
#define EVENT_ID_FFS_WEAR_LEVEL                 401

#define EVENT_ID_TIMER_ALARM_MISS               500

#define EVENT_ID_MEM_DEFRAG                     600

#define EVENT_ID_CMD_START                      700
#define EVENT_ID_CMD_TIMEOUT                    701
#define EVENT_ID_CMD_CRC_ERROR                  702

#define EVENT_ID_THREAD_ID                      800
#define EVENT_ID_FUNC_ENTER                     801
#define EVENT_ID_FUNC_EXIT                      802
#define EVENT_ID_STACK_POINTER                  803

"""

def parse_raw_events(events):
    event_dict = {}
    lines = events.split('\n')

    for line in lines:
        tokens = line.split()

        if len(tokens) < 3:
            continue

        event_dict[int(tokens[2])] = tokens[1].lower().replace('event_id_', '')

    return event_dict

event_lookup = parse_raw_events(raw_events)


class EventField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="event_id"),
                  Uint16Field(_name="param"),
                  Uint32Field(_name="timestamp")]

        super(EventField, self).__init__(_fields=fields, **kwargs)

    def get_event_str(self):
        return event_lookup[self.event_id]

    event_str = property(get_event_str)


class EventArray(ArrayField):
    def __init__(self, **kwargs):
        field = EventField

        super(EventArray, self).__init__(_field=field, **kwargs)



if __name__ == '__main__':
    pass
