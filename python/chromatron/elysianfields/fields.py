
import socket
import struct
import uuid
import json
import binascii
import inspect
try:
    from builtins import range

except ImportError:
    from builtins import range

from string import printable
from collections import OrderedDict

from copy import deepcopy


class Field(object):
    def __init__(self, _value=None, _name=None, **kwargs):
        self._internal_value = _value

        if _name:
            self._name = _name

        else:
            self._name = self.__class__.__name__

    def toBasic(self):
        return self._internal_value

    def toJSON(self):
        return json.dumps(self.toBasic())

    def __str__(self):
        return str(self._value)

    def get_value(self):
        return self._internal_value

    def set_value(self, value):
        self._internal_value = value

    _value = property(get_value, set_value)

    def size(self):
        raise NotImplementedError

    def unpack(self, buffer):
        raise NotImplementedError

    def pack(self):
        raise NotImplementedError

class ErrorField(Field):
    def __init__(self, _value=None, **kwargs):
        super(ErrorField, self).__init__(_value=_value, **kwargs)

    def unpack(self, buffer):
        return self

    def pack(self):
        return ""


class BooleanField(Field):
    def __init__(self, _value=False, **kwargs):
        super(BooleanField, self).__init__(_value=_value, **kwargs)

    def get_value(self):
        return self._internal_value

    def set_value(self, value):
        try:
            if value.lower() == "false":
                value = False
            elif value.lower() == "true":
                value = True
            elif value.lower() == '0':
                value = False

        except:
            pass

        self._internal_value = bool(value)

    _value = property(get_value, set_value)

    def size(self):
        return struct.calcsize('<?')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<?', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<?', self._value)

class IntegerField(Field):
    def __init__(self, _value=0, **kwargs):
        super(IntegerField, self).__init__(_value=int(_value), **kwargs)

    def get_value(self):
        return self._internal_value

    def set_value(self, value):
        self._internal_value = int(value)

    _value = property(get_value, set_value)

class Int8Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Int8Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<b')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<b', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<b', self._value)

class Uint8Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Uint8Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<B')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<B', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<B', self._value)

class CharField(Field):
    def __init__(self, _value=b'\0', **kwargs):
        super(CharField, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<c')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<c', buffer)[0].decode('ascii')

        return self

    def pack(self):
        return struct.pack('<c', self._value.encode('ascii'))

class Int16Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Int16Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<h')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<h', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<h', self._value)

class Uint16Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Uint16Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<H')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<H', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<H', self._value)

class Int32Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Int32Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<i')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<i', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<i', self._value)

class Uint32Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Uint32Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<I')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<I', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<I', self._value)

class Int64Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Int64Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<q')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<q', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<q', self._value)

class Uint64Field(IntegerField):
    def __init__(self, _value=0, **kwargs):
        super(Uint64Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return struct.calcsize('<Q')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<Q', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<Q', self._value)

class FloatField(Field):
    def __init__(self, _value=0.0, **kwargs):
        super(FloatField, self).__init__(_value=_value, **kwargs)

    def get_value(self):
        return self._internal_value

    def set_value(self, value):
        self._internal_value = float(value)

    _value = property(get_value, set_value)

    def size(self):
        return struct.calcsize('<f')

    def unpack(self, buffer):
        self._value = struct.unpack_from('<f', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<f', self._value)


class Fixed16Field(FloatField):
    def __init__(self, _value=0.0, **kwargs):
        super(Fixed16Field, self).__init__(_value=_value, **kwargs)

    def unpack(self, buffer):
        value = struct.unpack_from('<i', buffer)[0]

        self._value = value / 65536.0

        return self

    def pack(self):
        value = int(self._value * 65536.0) 

        return struct.pack('<i', value)


class Ipv4Field(Uint32Field):
    def __init__(self, _value=0, **kwargs):
        super(Ipv4Field, self).__init__(_value=_value, **kwargs)

    def get_value(self):
        return socket.inet_ntoa(struct.pack('I', self._internal_value))

    def set_value(self, value):
        self._internal_value = struct.unpack('I',socket.inet_aton(value))[0]

    _value = property(get_value, set_value)

    def unpack(self, buffer):
        self._internal_value = struct.unpack_from('<I', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<I', self._internal_value)

class StringField(Field):
    def __init__(self, _value="", _length=None, **kwargs):

        if _length == None:
            _length = len(_value)
            self._fixed_length = False

        else:
            self._fixed_length = True

        self._length = _length

        # check if a string is given, if not,
        # initialize empty string with length
        if _value == "":
            _value = "\0" * _length

        super(StringField, self).__init__(**kwargs)

        self._value = str(_value)

    def __str__(self):
        return self._value

    def get_value(self):
        assert isinstance(self._internal_value, bytes)

        return self._internal_value.decode('ascii')

    def set_value(self, data):
        self._internal_value = data.encode('ascii', errors='replace') # convert to ascii bytes, unicode will break

        assert isinstance(self._internal_value, bytes)

        if not self._fixed_length:
            self._length = len(self._internal_value)

    _value = property(get_value, set_value)

    def size(self):
        return self._length

    def unpack(self, buf):
        # check if length is not set
        if self._length == 0:
            # scan to null terminator
            s = []
            
            try:
                decoded = buf.decode('ascii')  
            except AttributeError:
                decoded = buf

            for c in decoded:
                if c == '\0':
                    break

                s.append(c)

            # set length, adding one byte for the null terminator
            self._length = len(s) + 1

        else:
            unpack_len = self.size()
            # check if supplied buffer is less than our size.  this is fine, but we need to pad.
            if len(buf) < unpack_len:
                unpack_len = len(buf)

            s = struct.unpack_from('<' + str(unpack_len) + 's', buf)[0].decode('ascii')
            padding_len = self.size() - unpack_len
            s += '\0' * padding_len
    
        self._value = ''.join([c for c in s if c in printable])
        
        return self

    def pack(self):
        return struct.pack('<' + str(self.size()) + 's', self._internal_value)

class String128Field(StringField):
    def __init__(self, _value="", **kwargs):
        super(String128Field, self).__init__(_value=_value, _length=128, **kwargs)

class String32Field(StringField):
    def __init__(self, _value="", **kwargs):
        super(String32Field, self).__init__(_value=_value, _length=32, **kwargs)

class String64Field(StringField):
    def __init__(self, _value="", **kwargs):
        super(String64Field, self).__init__(_value=_value, _length=64, **kwargs)

class String488Field(StringField):
    def __init__(self, _value="", **kwargs):
        super(String488Field, self).__init__(_value=_value, _length=488, **kwargs)

class String512Field(StringField):
    def __init__(self, _value="", **kwargs):
        super(String512Field, self).__init__(_value=_value, _length=512, **kwargs)

class UuidField(StringField):
    def __init__(self, **kwargs):
        kwargs['_length'] = 16
        kwargs['_value'] = '00000000000000000000000000000000'

        super(UuidField, self).__init__(**kwargs)

    def get_value(self):
        return str(uuid.UUID(bytes=self._internal_value))

    def set_value(self, value):
        self._internal_value = uuid.UUID(value).bytes

    _value = property(get_value, set_value)

    def unpack(self, buffer):
        self._internal_value = struct.unpack_from('<' + str(self.size()) + 's', buffer)[0]

        return self

    def pack(self):
        return struct.pack('<' + str(self.size()) + 's', self._internal_value)

class RawBinField(Field):
    def __init__(self, _value="", **kwargs):
        super(RawBinField, self).__init__(_value=_value, **kwargs)

    def __str__(self):
        return str(self.size()) + " bytes"

    def size(self):
        return len(self._value)

    def unpack(self, buffer):
        self._value = buffer

        return self

    def pack(self):
        return self._value

class Mac48Field(StringField):
    def __init__(self, _value="00:00:00:00:00:00", **kwargs):
        super(Mac48Field, self).__init__(_value=_value, **kwargs)

    def __str__(self):
        return self._value

    def size(self):
        return 6

    def unpack(self, buffer):
        # slice and reverse buffer
        buffer = buffer[:self.size()]

        s = ''
        for c in buffer:
            try:
                s += '%02x:' % (int(c))

            except ValueError:    
                raise
                # bad byte (field may be uninitialized)
                s += 'xx:'

        self._value = s[:len(s)-1]

        return self

    def pack(self):
        tokens = self._value.split(':')

        s = bytes([int(token, 16) for token in tokens])
        
        return s


class Mac64Field(Mac48Field):
    def __init__(self, _value="00:00:00:00:00:00:00:00", **kwargs):
        super(Mac64Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return 8

class Key128Field(RawBinField):
    def __init__(self, _value="00000000000000000000000000000000", **kwargs):
        super(Key128Field, self).__init__(_value=_value, **kwargs)

    def size(self):
        return 16

    def get_value(self):
        return self._internal_value

    def set_value(self, value):
        if len(value) == 32:
            self._internal_value = value

        else:
            raise ValueError("Key size must be 16 bytes")

    _value = property(get_value, set_value)

    def unpack(self, buffer):
        self._internal_value = binascii.hexlify(buffer[:16]).decode('ascii')

        return self

    def pack(self):
        return binascii.unhexlify(self._internal_value)

class StructField(Field):
    def __init__(self, _fields=[], **kwargs):
        self._fields = OrderedDict()

        for field in _fields:
            if field._name in kwargs:
                if isinstance(kwargs[field._name], Field):
                    # make sure supplied Field's name is matched
                    # with the name in the struct.
                    kwargs[field._name]._name = field._name
                    field = kwargs[field._name]
                    
                else:
                    field._value = kwargs[field._name]

            self._fields[field._name] = field

        if '_value' in kwargs:
            for k in kwargs['_value']._fields:
                self._fields[k] = kwargs['_value']._fields[k]

            del kwargs['_value']

        super(StructField, self).__init__(_value=self, **kwargs)

    def toBasic(self):
        d = {}

        for field in self._fields:
            d[field] = self._fields[field]._value

        return d

    # deepcopy will break on structfields without this
    def __deepcopy__(self, memo):
        cls = self.__class__
        result = cls.__new__(cls)
        memo[id(self)] = result
        for k, v in list(self.__dict__.items()):
            setattr(result, k, deepcopy(v, memo))
        return result

    def __str__(self):
        s = self._name + ":\n"

        for field in list(self._fields.values()):
            s += " " + field._name + " = " + str(field) + "\n"

        if len(self._fields) == 0:
            s += " Empty"

        return s

    def __getattr__(self, name):
        if name in self._fields:
            return self._fields[name]._value

        else:
            raise KeyError(name)

    def __setattr__(self, name, value):
        if "_fields" in self.__dict__ and name in self.__dict__["_fields"]:
            self.__dict__["_fields"][name]._value = value

        else:
            super(StructField, self).__setattr__(name, value)

    def size(self):
        s = 0

        for field in list(self._fields.values()):
            s += field.size()

        return s

    def unpack(self, buffer):
        for field in list(self._fields.values()):
            field.unpack(buffer)
            buffer = buffer[field.size():]

        return self

    def pack(self):
        s = bytes()

        for field in list(self._fields.values()):
            s += field.pack()

        return s

class ArrayField(Field):
    def __init__(self, _field=None, _length=None, **kwargs):
        super(ArrayField, self).__init__(**kwargs)

        self._field = _field
        self._fields = []

        if _length:
            for i in range(_length):
                self._fields.append(self._field())

    @property
    def _length(self):
        return len(self._fields)

    def split_array(self, array, chunksize):

        n_chunks = len(array) / chunksize

        chunks = []

        for i in range(n_chunks):
            chunks.append(array[(i * chunksize):((i + 1) * chunksize)])

        return chunks

    def toBasic(self):
        a = []

        for field in self._fields:
            a.append(field.toBasic())

        return a

    def __str__(self):
        s = "Array of %d %ss\n" % (len(self._fields), self._field().__class__.__name__)
        s += " %d bytes packed\n  " % (self.size())

        for field in self._fields:
            s += "%s " % (field)

        return s

    def append(self, item):
        assert isinstance(item, self._field)
        self._fields.append(item)

    def __len__(self):
        return len(self._fields)

    def __getitem__(self, key):
        return self._fields[key]._value

    def __setitem__(self, key, value):
        self._fields[key]._value = value

    def get_value(self):
        # return [field._value for field in self._fields]
        return self

    def set_value(self, value):
        self._fields = [self._field(_value=v) for v in value]

    _value = property(get_value, set_value)

    def size(self):
        s = 0

        for field in self._fields:
            s += field.size()

        return s

    def unpack(self, buffer):
        array_len = self._length
        self._fields = []

        count = 0

        while len(buffer) > 0:
            field = self._field()
            self._fields.append(field.unpack(buffer))

            buffer = buffer[field.size():]
            count += 1

            if ( array_len > 0 ) and ( count >= array_len ):
                break

        return self

    def pack(self):
        s = b''

        for field in self._fields:
            s += field.pack()

        return s


class FixedArrayField(ArrayField):
    def __init__(self, _field=None, _length=None, **kwargs):
        super(FixedArrayField, self).__init__(_field, _length, **kwargs)

        self._array_length = _length

    def append(self, item):    
        raise ValueError

    def get_value(self):
        return [field._value for field in self._fields]

    def set_value(self, value):
        fields = [self._field(_value=v) for v in value]

        if len(fields) > self._array_length:
            raise ValueError

        self._fields = fields

        while len(self._fields) < self._array_length:
            self._fields.append(self._field())

    _value = property(get_value, set_value)


class NTPTimestampField(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="seconds"),
                  Uint32Field(_name="fraction")]

        super(NTPTimestampField, self).__init__(_fields=fields, **kwargs)

    def copy(self):
        return NTPTimestampField()

