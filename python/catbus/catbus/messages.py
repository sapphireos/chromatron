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
from data_structures import *
from catbustypes import *
from options import *
import random


CATBUS_MEOW                        = 0x574f454d # 'MEOW'

CATBUS_MSG_TYPE_OFFSET             = 4

CATBUS_MAX_HASH_LOOKUPS            = 16
CATBUS_MAX_GET_KEY_ITEM_COUNT      = 32
CATBUS_MAX_KEY_META                = 64
CATBUS_MAX_DATA                    = 512

CATBUS_MSG_GENERAL_GROUP_OFFSET    = 1
CATBUS_MSG_DISCOVERY_GROUP_OFFSET  = 10
CATBUS_MSG_DATABASE_GROUP_OFFSET   = 20
CATBUS_MSG_LINK_GROUP_OFFSET       = 40
CATBUS_MSG_FILE_GROUP_OFFSET       = 60

CATBUS_MSG_TYPE_ERROR                      = CATBUS_MSG_GENERAL_GROUP_OFFSET + 0

CATBUS_MSG_TYPE_ANNOUNCE                   = CATBUS_MSG_DISCOVERY_GROUP_OFFSET + 0
CATBUS_MSG_TYPE_DISCOVER                   = CATBUS_MSG_DISCOVERY_GROUP_OFFSET + 1

CATBUS_MSG_TYPE_LOOKUP_HASH                = CATBUS_MSG_DATABASE_GROUP_OFFSET + 1
CATBUS_MSG_TYPE_RESOLVED_HASH              = CATBUS_MSG_DATABASE_GROUP_OFFSET + 2
CATBUS_MSG_TYPE_GET_KEY_META               = CATBUS_MSG_DATABASE_GROUP_OFFSET + 3
CATBUS_MSG_TYPE_KEY_META                   = CATBUS_MSG_DATABASE_GROUP_OFFSET + 4
CATBUS_MSG_TYPE_GET_KEYS                   = CATBUS_MSG_DATABASE_GROUP_OFFSET + 5
CATBUS_MSG_TYPE_SET_KEYS                   = CATBUS_MSG_DATABASE_GROUP_OFFSET + 6
CATBUS_MSG_TYPE_KEY_DATA                   = CATBUS_MSG_DATABASE_GROUP_OFFSET + 7

CATBUS_MSG_TYPE_LINK                       = CATBUS_MSG_LINK_GROUP_OFFSET + 1
CATBUS_MSG_TYPE_LINK_DATA                  = CATBUS_MSG_LINK_GROUP_OFFSET + 2

CATBUS_MSG_TYPE_FILE_OPEN                  = ( 1 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_CONFIRM               = ( 2 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_DATA                  = ( 3 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_GET                   = ( 4 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_CLOSE                 = ( 5 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_DELETE                = ( 6 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_ACK                   = ( 7 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_CHECK                 = ( 8 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_CHECK_RESPONSE        = ( 9 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_LIST                  = ( 10 + CATBUS_MSG_FILE_GROUP_OFFSET )
CATBUS_MSG_TYPE_FILE_LIST_DATA             = ( 11 + CATBUS_MSG_FILE_GROUP_OFFSET )

CATBUS_MSG_FILE_FLAG_READ                  = 0x01
CATBUS_MSG_FILE_FLAG_WRITE                 = 0x02

CATBUS_DISC_FLAG_QUERY_ALL                 = 0x01



class MsgHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="meow"),
                  Uint8Field(_name="msg_type"),
                  Uint64Field(_name="origin_id"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="flags"),
                  Uint8Field(_name="reserved"),
                  CatbusHash(_name="universe"),
                  Uint32Field(_name="transaction_id")]

        super(MsgHeader, self).__init__(_fields=fields, **kwargs)

        self.meow           = CATBUS_MEOW
        self.origin_id      = 0
        self.version        = CATBUS_VERSION
        self.flags          = 0 
        self.msg_type       = 0
        self.reserved       = 0
        self.universe       = 0
        self.transaction_id = random.randint(0, pow(2, 32) - 1)

class ErrorMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint16Field(_name="error_code")]

        super(ErrorMsg, self).__init__(_name="error_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_ERROR

class AnnounceMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  Uint16Field(_name="data_port"),
                  CatbusQuery(_name="query")]

        super(AnnounceMsg, self).__init__(_name="announce_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_ANNOUNCE

class DiscoverMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusQuery(_name="query")]

        super(DiscoverMsg, self).__init__(_name="discover_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_DISCOVER

class LookupHashMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  ArrayField(_name="hashes", _field=CatbusHash)]

        super(LookupHashMsg, self).__init__(_name="lookup_hash_msg", _fields=fields, **kwargs)

        self.count = len(self.hashes)
        self.header.msg_type = CATBUS_MSG_TYPE_LOOKUP_HASH

class ResolvedHashMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  ArrayField(_name="keys", _field=CatbusStringField)]

        super(ResolvedHashMsg, self).__init__(_name="resolved_hash_msg", _fields=fields, **kwargs)

        self.count = len(self.keys)
        self.header.msg_type = CATBUS_MSG_TYPE_RESOLVED_HASH

class GetKeyMetaMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint16Field(_name="page")]

        super(GetKeyMetaMsg, self).__init__(_name="get_key_meta_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_GET_KEY_META

class KeyMetaMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint16Field(_name="page"),
                  Uint16Field(_name="page_count"),
                  Uint8Field(_name="item_count"),
                  ArrayField(_name="meta", _field=CatbusMeta)]

        super(KeyMetaMsg, self).__init__(_name="key_meta_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_KEY_META

class GetKeysMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  ArrayField(_name="hashes", _field=CatbusHash)]

        super(GetKeysMsg, self).__init__(_name="get_keys_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_GET_KEYS
        self.count = len(self.hashes)

class SetKeysMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  CatbusDataArray(_name="data")]

        super(SetKeysMsg, self).__init__(_name="set_keys_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_SET_KEYS
        self.count = len(self.data)

class KeyDataMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="count"),
                  CatbusDataArray(_name="data")]

        super(KeyDataMsg, self).__init__(_name="key_data_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_KEY_DATA
        self.count = len(self.data)

class LinkMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusHash(_name="source_hash"),
                  CatbusHash(_name="dest_hash"),
                  CatbusQuery(_name="query")]

        super(LinkMsg, self).__init__(_name="link_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_LINK

class LinkDataMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  NTPTimestampField(_name='ntp_timestamp'),
                  CatbusQuery(_name="source_query"),
                  CatbusHash(_name="source_hash"),
                  CatbusHash(_name="dest_hash"),
                  Uint16Field(_name="sequence"),
                  Int32Field(_name="data")]

        super(LinkDataMsg, self).__init__(_name="link_data_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_LINK_DATA


class FileOpenMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusStringField(_name='filename')]

        super(FileOpenMsg, self).__init__(_name="file_open_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_OPEN

class FileConfirmMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusStringField(_name='filename'),
                  Uint8Field(_name="status"),
                  Uint32Field(_name="session_id"),
                  Uint16Field(_name="page_size")]

        super(FileConfirmMsg, self).__init__(_name="file_confirm_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_CONFIRM

class FileDataMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  Uint32Field(_name="session_id"),
                  Int32Field(_name="offset"),
                  Uint16Field(_name="len"),
                  RawBinField(_name="data")]

        super(FileDataMsg, self).__init__(_name="file_data_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_DATA

class FileGetMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  Uint32Field(_name="session_id"),
                  Int32Field(_name="offset")]

        super(FileGetMsg, self).__init__(_name="file_get_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_GET

class FileCloseMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  Uint32Field(_name="session_id")]

        super(FileCloseMsg, self).__init__(_name="file_close_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_CLOSE

class FileDeleteMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusStringField(_name='filename')]

        super(FileDeleteMsg, self).__init__(_name="file_delete_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_DELETE

class FileAckMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags")]

        super(FileAckMsg, self).__init__(_name="file_ack_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_ACK

class FileCheckMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusStringField(_name='filename')]

        super(FileCheckMsg, self).__init__(_name="file_check_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_CHECK

class FileCheckResponseMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="flags"),
                  CatbusHash(_name="hash"),
                  Uint32Field(_name="file_len")]

        super(FileCheckResponseMsg, self).__init__(_name="file_check_response_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_CHECK_RESPONSE

class FileListMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint16Field(_name="index")]

        super(FileListMsg, self).__init__(_name="file_list_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_LIST


class FileListDataMsg(StructField):
    def __init__(self, **kwargs):
        fields = [MsgHeader(_name="header"),
                  Uint8Field(_name="filename_len"),
                  Uint16Field(_name="file_count"),
                  Uint16Field(_name="index"),
                  Uint16Field(_name="next_index"),
                  Uint8Field(_name="item_count"),
                  ArrayField(_name="meta", _field=CatbusFileMeta)]

        super(FileListDataMsg, self).__init__(_name="file_list_data_msg", _fields=fields, **kwargs)

        self.header.msg_type = CATBUS_MSG_TYPE_FILE_LIST_DATA


messages = {
    CATBUS_MSG_TYPE_ERROR:                  ErrorMsg,

    CATBUS_MSG_TYPE_ANNOUNCE:               AnnounceMsg,
    CATBUS_MSG_TYPE_DISCOVER:               DiscoverMsg,

    CATBUS_MSG_TYPE_LOOKUP_HASH:            LookupHashMsg,
    CATBUS_MSG_TYPE_RESOLVED_HASH:          ResolvedHashMsg,
    CATBUS_MSG_TYPE_GET_KEY_META:           GetKeyMetaMsg,
    CATBUS_MSG_TYPE_KEY_META:               KeyMetaMsg,
    CATBUS_MSG_TYPE_GET_KEYS:               GetKeysMsg,
    CATBUS_MSG_TYPE_SET_KEYS:               SetKeysMsg,
    CATBUS_MSG_TYPE_KEY_DATA:               KeyDataMsg,

    CATBUS_MSG_TYPE_LINK:                   LinkMsg,
    CATBUS_MSG_TYPE_LINK_DATA:              LinkDataMsg,

    CATBUS_MSG_TYPE_FILE_OPEN:              FileOpenMsg,
    CATBUS_MSG_TYPE_FILE_CONFIRM:           FileConfirmMsg,
    CATBUS_MSG_TYPE_FILE_DATA:              FileDataMsg,
    CATBUS_MSG_TYPE_FILE_GET:               FileGetMsg,
    CATBUS_MSG_TYPE_FILE_CLOSE:             FileCloseMsg, 
    CATBUS_MSG_TYPE_FILE_DELETE:            FileDeleteMsg, 
    CATBUS_MSG_TYPE_FILE_ACK:               FileAckMsg, 
    CATBUS_MSG_TYPE_FILE_CHECK:             FileCheckMsg, 
    CATBUS_MSG_TYPE_FILE_CHECK_RESPONSE:    FileCheckResponseMsg, 

    CATBUS_MSG_TYPE_FILE_LIST:              FileListMsg, 
    CATBUS_MSG_TYPE_FILE_LIST_DATA:         FileListDataMsg, 
}


def deserialize(buf):
    msg_id = ord(buf[CATBUS_MSG_TYPE_OFFSET])
    try:
        return messages[msg_id]().unpack(buf)

    except KeyError:
        raise UnknownMessageException(msg_id)

    except struct.error as e:
        print msg_id
        raise InvalidMessageException(e)


def serialize(msg):
    return msg.pack()


CATBUS_ERROR_OK                             = 0
CATBUS_ERROR_UNKNOWN_MSG                    = 0xfffe
CATBUS_ERROR_PROTOCOL_ERROR                 = 0x0001
CATBUS_ERROR_ALLOC_FAIL                     = 0x0002
CATBUS_ERROR_KEY_NOT_FOUND                  = 0x0003
CATBUS_ERROR_INVALID_TYPE                   = 0x0004
CATBUS_ERROR_READ_ONLY                      = 0x0005

CATBUS_ERROR_FILE_NOT_FOUND                 = 0x0101
CATBUS_ERROR_FILESYSTEM_FULL                = 0x0102
CATBUS_ERROR_FILESYSTEM_BUSY                = 0x0103
CATBUS_ERROR_INVALID_FILE_SESSION           = 0x0104

error_codes = {
    CATBUS_ERROR_OK:                "OK",
    CATBUS_ERROR_UNKNOWN_MSG:       "Unknown message",
    CATBUS_ERROR_PROTOCOL_ERROR:    "Protocol error",
    CATBUS_ERROR_ALLOC_FAIL:        "Alloc fail",
    CATBUS_ERROR_KEY_NOT_FOUND:     "Key not found",
    CATBUS_ERROR_INVALID_TYPE:      "Invalid type",
    CATBUS_ERROR_READ_ONLY:         "Read only",
    CATBUS_ERROR_FILE_NOT_FOUND:    "File not found",
    CATBUS_ERROR_FILESYSTEM_FULL:   "Filesystem full",
    CATBUS_ERROR_FILESYSTEM_BUSY:   "Filesystem busy",
    CATBUS_ERROR_INVALID_FILE_SESSION: "Invalid file session",
}

def lookup_error_msg(error):
    try:
        return error_codes[error]

    except KeyError:
        return "Unknown error"

