
from elysianfields import *


class Payload(StructField):

    msg_type_format = None
    msg_type = 0
    fields = []

    def __init__(self, **kwargs):
        fields_copy = deepcopy(self.fields)

        super(Payload, self).__init__(_fields=fields_copy, **kwargs)

        if self.msg_type_format:
            self._local_msg_type_format = deepcopy(self.msg_type_format)
            self._local_msg_type_format._value = self.msg_type

    def size(self):
        field_size = super(Payload, self).size()

        if self.msg_type_format:
            field_size += self._local_msg_type_format.size()

        return field_size

    def unpack(self, buffer):
        if self.msg_type_format:
            # slice past message type if present
            buffer = buffer[self._local_msg_type_format.size():]

        super(Payload, self).unpack(buffer)

        return self

    def pack(self):
        s = ""

        if self.msg_type_format:
            s = self._local_msg_type_format.pack()

        s += super(Payload, self).pack()

        return s


class Protocol(object):

    class NullPayload(Payload):
        msg_type = 0
        fields = []

    msg_type_format = Uint8Field()

    def __init__(self):
        self.messages = self.get_msgs()
        self.__msg_dict = {}

        self._local_msg_type_format = deepcopy(self.msg_type_format)

        for message in self.messages:
            message.msg_type_format = self._local_msg_type_format
            self.__msg_dict[message.msg_type] = message

    def get_msgs(self):
        return [m[1] for m in inspect.getmembers(self)
                    if hasattr(m[1], '__bases__') and Payload in m[1].__bases__]

    def unpack(self, data):
        # get message type from data
        msg_type = self._local_msg_type_format.unpack(data)._value

        # initialize and unpack message
        try:
            msg = self.__msg_dict[msg_type]().unpack(data)

        except KeyError:
            raise

        return msg

