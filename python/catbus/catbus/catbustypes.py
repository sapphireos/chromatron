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

from elysianfields import *

class UnknownTypeError(Exception):
    pass


CATBUS_TYPE_NONE             = 0
CATBUS_TYPE_BOOL             = 1
CATBUS_TYPE_UINT8            = 2
CATBUS_TYPE_INT8             = 3
CATBUS_TYPE_UINT16           = 4
CATBUS_TYPE_INT16            = 5
CATBUS_TYPE_UINT32           = 6
CATBUS_TYPE_INT32            = 7
CATBUS_TYPE_UINT64           = 8
CATBUS_TYPE_INT64            = 9
CATBUS_TYPE_FLOAT            = 10

CATBUS_TYPE_STRING128        = 40
CATBUS_TYPE_MAC48            = 41
CATBUS_TYPE_MAC64            = 42
CATBUS_TYPE_KEY128           = 43
CATBUS_TYPE_IPv4             = 44
CATBUS_TYPE_STRING512        = 45
CATBUS_TYPE_STRING32         = 46
CATBUS_TYPE_STRING64         = 47

CATBUS_TYPE_MISMATCH         = -6

type_registry = {
    CATBUS_TYPE_NONE: None,
    CATBUS_TYPE_BOOL: BooleanField,
    CATBUS_TYPE_UINT8: Uint8Field,
    CATBUS_TYPE_INT8: Int8Field,
    CATBUS_TYPE_UINT16: Uint16Field,
    CATBUS_TYPE_INT16: Int16Field,
    CATBUS_TYPE_UINT32: Uint32Field,
    CATBUS_TYPE_INT32: Int32Field,
    CATBUS_TYPE_UINT64: Uint64Field,
    CATBUS_TYPE_INT64: Int64Field,
    CATBUS_TYPE_FLOAT: FloatField,

    CATBUS_TYPE_STRING128: String128Field,
    CATBUS_TYPE_STRING32: String32Field,
    CATBUS_TYPE_STRING64: String64Field,
    CATBUS_TYPE_STRING512: String512Field,
    CATBUS_TYPE_MAC48: Mac48Field,
    CATBUS_TYPE_MAC64: Mac64Field,
    CATBUS_TYPE_KEY128: Key128Field,
    CATBUS_TYPE_IPv4: Ipv4Field,
    CATBUS_TYPE_MISMATCH: ErrorField,
}

type_id_registry = {
    'none': CATBUS_TYPE_NONE,
    'bool': CATBUS_TYPE_BOOL,
    'uint8': CATBUS_TYPE_UINT8,
    'int8': CATBUS_TYPE_INT8,
    'uint16': CATBUS_TYPE_UINT16,
    'int16': CATBUS_TYPE_INT16,
    'uint32': CATBUS_TYPE_UINT32,
    'int32': CATBUS_TYPE_INT32,
    'uint64': CATBUS_TYPE_UINT64,
    'int64': CATBUS_TYPE_INT64,
    'float': CATBUS_TYPE_FLOAT,

    'string128': CATBUS_TYPE_STRING128,
    'string32': CATBUS_TYPE_STRING32,
    'string64': CATBUS_TYPE_STRING64,
    'string512': CATBUS_TYPE_STRING512,
    'mac48': CATBUS_TYPE_MAC48,
    'mac64': CATBUS_TYPE_MAC64,
    'key128': CATBUS_TYPE_KEY128,
    'ipv4': CATBUS_TYPE_IPv4,
}

def get_type_id(type_name):
    return type_id_registry[type_name]

def get_type_name(catbus_type):
    for k, v in type_id_registry.iteritems():
        if catbus_type == v:
            return k

    raise KeyError(catbus_type)

def get_field_for_type(t, **kwargs):
    try:
        try:
            return type_registry[t](**kwargs)

        except KeyError:
            type_id = type_id_registry[t]
            return type_registry[type_id](**kwargs)

    except Exception as e:
        raise UnknownTypeError(t)
        
class UnknownField(ErrorField):
    def __init__(self, _value=None, **kwargs):
        super(UnknownField, self).__init__(_value=_value, **kwargs)

    def unpack(self, buffer):
        return self

    def pack(self):
        return ""

    def size(self):
        return 0


