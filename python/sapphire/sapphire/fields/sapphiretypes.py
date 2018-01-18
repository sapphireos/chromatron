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


SAPPHIRE_TYPE_NONE             = 0
SAPPHIRE_TYPE_BOOL             = 1
SAPPHIRE_TYPE_UINT8            = 2
SAPPHIRE_TYPE_INT8             = 3
SAPPHIRE_TYPE_UINT16           = 4
SAPPHIRE_TYPE_INT16            = 5
SAPPHIRE_TYPE_UINT32           = 6
SAPPHIRE_TYPE_INT32            = 7
SAPPHIRE_TYPE_UINT64           = 8
SAPPHIRE_TYPE_INT64            = 9
SAPPHIRE_TYPE_FLOAT            = 10

SAPPHIRE_TYPE_STRING128        = 40
SAPPHIRE_TYPE_MAC48            = 41
SAPPHIRE_TYPE_MAC64            = 42
SAPPHIRE_TYPE_KEY128           = 43
SAPPHIRE_TYPE_IPv4             = 44
SAPPHIRE_TYPE_STRING512        = 45
SAPPHIRE_TYPE_STRING32         = 46
SAPPHIRE_TYPE_STRING64         = 47

SAPPHIRE_TYPE_MISMATCH         = -6

type_registry = {
    SAPPHIRE_TYPE_NONE: None,
    SAPPHIRE_TYPE_BOOL: BooleanField,
    SAPPHIRE_TYPE_UINT8: Uint8Field,
    SAPPHIRE_TYPE_INT8: Int8Field,
    SAPPHIRE_TYPE_UINT16: Uint16Field,
    SAPPHIRE_TYPE_INT16: Int16Field,
    SAPPHIRE_TYPE_UINT32: Uint32Field,
    SAPPHIRE_TYPE_INT32: Int32Field,
    SAPPHIRE_TYPE_UINT64: Uint64Field,
    SAPPHIRE_TYPE_INT64: Int64Field,
    SAPPHIRE_TYPE_FLOAT: FloatField,

    SAPPHIRE_TYPE_STRING128: String128Field,
    SAPPHIRE_TYPE_STRING32: String32Field,
    SAPPHIRE_TYPE_STRING64: String64Field,
    SAPPHIRE_TYPE_STRING512: String512Field,
    SAPPHIRE_TYPE_MAC48: Mac48Field,
    SAPPHIRE_TYPE_MAC64: Mac64Field,
    SAPPHIRE_TYPE_KEY128: Key128Field,
    SAPPHIRE_TYPE_IPv4: Ipv4Field,
    SAPPHIRE_TYPE_MISMATCH: ErrorField,
}

type_id_registry = {
    'none': SAPPHIRE_TYPE_NONE,
    'bool': SAPPHIRE_TYPE_BOOL,
    'uint8': SAPPHIRE_TYPE_UINT8,
    'int8': SAPPHIRE_TYPE_INT8,
    'uint16': SAPPHIRE_TYPE_UINT16,
    'int16': SAPPHIRE_TYPE_INT16,
    'uint32': SAPPHIRE_TYPE_UINT32,
    'int32': SAPPHIRE_TYPE_INT32,
    'uint64': SAPPHIRE_TYPE_UINT64,
    'int64': SAPPHIRE_TYPE_INT64,
    'float': SAPPHIRE_TYPE_FLOAT,

    'string128': SAPPHIRE_TYPE_STRING128,
    'string32': SAPPHIRE_TYPE_STRING32,
    'string64': SAPPHIRE_TYPE_STRING64,
    'string512': SAPPHIRE_TYPE_STRING512,
    'mac48': SAPPHIRE_TYPE_MAC48,
    'mac64': SAPPHIRE_TYPE_MAC64,
    'key128': SAPPHIRE_TYPE_KEY128,
    'ipv4': SAPPHIRE_TYPE_IPv4,
}

def get_type_id(type_name):
    return type_id_registry[type_name]

def get_field_for_type(t, **kwargs):
    try:
        return type_registry[t](**kwargs)

    except KeyError:
        type_id = type_id_registry[t]
        return type_registry[type_id](**kwargs)
