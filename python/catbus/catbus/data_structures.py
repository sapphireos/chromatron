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


from elysianfields import *
from catbustypes import *
from fnvhash import fnv1a_32


CATBUS_QUERY_TAG_LEN                    = 8


class ReadOnlyException(Exception):
    pass

class UnknownMessageException(Exception):
    pass


class InvalidMessageException(Exception):
    pass

class NoResponseFromHost(Exception):
    def __init__(self, msg=None, *args):
        super(NoResponseFromHost, self).__init__(*args)
        self.msg = msg

    def __str__(self):
        return "NoResponseFromHost: %d" % (self.msg)

class GenericProtocolException(Exception):
    pass

class ProtocolErrorException(Exception):
    def __init__(self, error_code, msg='', *args):
        super(ProtocolErrorException, self).__init__(*args)
        self.error_code = error_code
        self.msg = msg

    def __str__(self):
        return "ProtocolErrorException: %x %s" % (self.error_code, self.msg)


class DataUnpackingError(Exception):
    pass



class CatbusStringField(String32Field):
    pass

class CatbusHash(Uint32Field):
    pass

class CatbusQuery(FixedArrayField):
    def __init__(self, **kwargs):
        field = CatbusHash
        super(CatbusQuery, self).__init__(_field=field, _length=CATBUS_QUERY_TAG_LEN, **kwargs)

class CatbusType(Uint8Field):
    pass

class CatbusFlags(Uint8Field):
    pass

class CatbusMeta(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusHash(_name="hash"),
                  CatbusType(_name="type"),
                  Uint8Field(_name="array_len"),
                  CatbusFlags(_name="flags"),
                  Uint8Field(_name="reserved")]

        if 'type' in kwargs:
            try:
                type_id = get_type_id(kwargs['type'])
                kwargs['type'] = type_id

            except KeyError:
                pass

        super(CatbusMeta, self).__init__(_fields=fields, **kwargs)

class CatbusData(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusMeta(_name="meta")]
                  # data follows

        # look up type
        if 'meta' in kwargs:
            valuefield = get_field_for_type(kwargs['meta'].type, _name='value')

            if kwargs['meta'].array_len == 0:
                fields.append(valuefield)

            else:
                array = FixedArrayField(_field=type(valuefield), _length=kwargs['meta'].array_len + 1, _name='value')
                fields.append(array)
                valuefield = array

            if 'value' in kwargs:
                valuefield.value = kwargs['value']
                kwargs['value'] = valuefield.value

        super(CatbusData, self).__init__(_fields=fields, **kwargs)

    def unpack(self, buffer):
        super(CatbusData, self).unpack(buffer)

        buffer = buffer[self.size():]

        # get value field based on type
        try:
            valuefield = get_field_for_type(self.meta.type, _name='value')

            if self.meta.array_len == 0:
                valuefield.unpack(buffer)
                self._fields['value'] = valuefield

            else:
                array = FixedArrayField(_field=type(valuefield), _length=self.meta.array_len + 1, _name='value')
                array.unpack(buffer)
                self._fields['value'] = array

        except KeyError as e:
            raise DataUnpackingError(e)

        except UnknownTypeError:
            self._fields['value'] = UnknownField()

        return self

class CatbusDataArray(ArrayField):
    def __init__(self, **kwargs):
        field = CatbusData

        super(CatbusDataArray, self).__init__(_field=field, **kwargs)

class CatbusFileMeta(StructField):
    def __init__(self, **kwargs):
        fields = [Int32Field(_name="filesize"),
                  Uint8Field(_name="flags"),
                  FixedArrayField(_name="reserved", _field=Uint8Field, _length=3),
                  CatbusStringField(_name="filename")]

        super(CatbusFileMeta, self).__init__(_fields=fields, **kwargs)


# query tags from source tags that match tags in dest tags.
def query_tags(src_tags, dst_tags):
    for tag in src_tags:
        if tag == 0:
            continue

        if tag not in dst_tags:
            return False

    return True

def query_compare(tags0, tags1):
    for tag in tags0:
        if tag not in tags1:
            return False

    for tag in tags1:
        if tag not in tags0:
            return False

    return True


from sapphire.common import util


@util.memoize
def catbus_string_hash(s):
    s = str(s) # no unicode!
    
    if len(s) == 0:
        return 0

    return fnv1a_32(s)



CATBUS_FLAGS_READ_ONLY                  = 0x0001
CATBUS_FLAGS_PERSIST                    = 0x0004
CATBUS_FLAGS_DYNAMIC                    = 0x0008

CATBUS_MSG_DISC_FLAG_QUERY_ALL          = 0x01

CATBUS_MSG_DATA_FLAG_TIME_SYNC          = 0x01

CATBUS_MSG_LINK_FLAG_SOURCE             = 0x01
CATBUS_MSG_LINK_FLAGS_DEST              = 0x04

META_TAG_NAME = 'meta_tag_name'
META_TAG_LOC = 'meta_tag_location'
META_TAG_GROUP_COUNT = 8 - 2

META_TAGS = [
    META_TAG_NAME,
    META_TAG_LOC,
    'meta_tag_0',
    'meta_tag_1',
    'meta_tag_2',
    'meta_tag_3',
    'meta_tag_4',
    'meta_tag_5',
]



META_TAG_NAME = 'meta_tag_name'
META_TAG_LOC = 'meta_tag_location'
META_TAG_BASE = 'meta_tag_%d'
META_TAG_GROUP_COUNT = 6

META_TAGS = [
    META_TAG_NAME,
    META_TAG_LOC,
]

for i in xrange(META_TAG_GROUP_COUNT):
    META_TAGS.append(META_TAG_BASE % (i))

