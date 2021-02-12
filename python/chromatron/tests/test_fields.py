import unittest

import uuid

from elysianfields import *


class FieldTests(unittest.TestCase):
    def test_boolean(self):
        a = BooleanField()
        self.assertEqual(a.size(), 1)

        self.assertEqual(a._value, False)

        a._value = 'False'
        self.assertEqual(a._value, False)
    
        a._value = 'True'
        self.assertEqual(a._value, True)

        a._value = '0'
        self.assertEqual(a._value, False)

        a._value = '1'
        self.assertEqual(a._value, True)

        for val in [True, False]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = BooleanField().unpack(packed)

            self.assertEqual(b._value, a._value)        

    def test_int8(self):
        a = Int8Field()
        self.assertEqual(a.size(), 1)

        for val in [-128, -1, 0, 1, 127]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Int8Field().unpack(packed)

            self.assertEqual(b._value, a._value)        

    def test_uint8(self):
        a = Uint8Field()
        self.assertEqual(a.size(), 1)

        for val in [0, 1, 255]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Uint8Field().unpack(packed)

            self.assertEqual(b._value, a._value)        

    def test_char(self):
        a = CharField()
        self.assertEqual(a.size(), 1)

        for val in ['a', 'b', 'c']:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = CharField().unpack(packed)

            self.assertEqual(b._value, a._value)   

    def test_int16(self):
        a = Int16Field()
        self.assertEqual(a.size(), 2)

        for val in [-32768, -1, 0, 1, 32767]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Int16Field().unpack(packed)

            self.assertEqual(b._value, a._value)        

    def test_uint16(self):
        a = Uint16Field()
        self.assertEqual(a.size(), 2)

        for val in [0, 1, 65535]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Uint16Field().unpack(packed)

            self.assertEqual(b._value, a._value)   

    def test_int32(self):
        a = Int32Field()
        self.assertEqual(a.size(), 4)

        for val in [-2147483648, -1, 0, 1, 2147483647]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Int32Field().unpack(packed)

            self.assertEqual(b._value, a._value)        

    def test_uint32(self):
        a = Uint32Field()
        self.assertEqual(a.size(), 4)

        for val in [0, 1, 4294967295]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Uint32Field().unpack(packed)

            self.assertEqual(b._value, a._value)   

    def test_int64(self):
        a = Int64Field()
        self.assertEqual(a.size(), 8)

        for val in [-9223372036854775808, -1, 0, 1, 9223372036854775807]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Int64Field().unpack(packed)

            self.assertEqual(b._value, a._value)        

    def test_uint64(self):
        a = Uint64Field()
        self.assertEqual(a.size(), 8)

        for val in [0, 1, 18446744073709551615]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Uint64Field().unpack(packed)

            self.assertEqual(b._value, a._value)   

    def test_float(self):
        a = FloatField()
        self.assertEqual(a.size(), 4)

        for val in [-3.3999999521443642e+38, 0, 3.3999999521443642e+38]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = FloatField().unpack(packed)

            self.assertEqual(b._value, a._value)   

    def test_fixed16(self):
        a = Fixed16Field()
        self.assertEqual(a.size(), 4)

        for val in [-30000.122985839844, 0, 30000.122985839844]:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Fixed16Field().unpack(packed)

            self.assertEqual(b._value, a._value)   


    def test_ipv4(self):
        a = Ipv4Field()
        self.assertEqual(a.size(), 4)

        for val in ['0.0.0.0', '127.0.0.1', '255.255.255.255']:
            a._value = val
            self.assertEqual(a._value, val)

            packed = a.pack()
            self.assertEqual(len(packed), a.size())        

            b = Ipv4Field().unpack(packed)

            self.assertEqual(b._value, a._value)  

    def test_string(self):
        a = StringField()
        test_string = "test_string"
        a._value = test_string

        self.assertEqual(a.size(), len(test_string))
        self.assertEqual(a.__str__(), test_string)

        packed = a.pack()
        self.assertEqual(packed, test_string.encode('ascii'))        

        b = StringField().unpack(packed)
        self.assertEqual(b._value, a._value)

    def test_string_null_terminator(self):
        a = StringField()
        test_string = "test_string" + '\0'
        
        a.unpack(test_string)

        self.assertEqual(a.size(), len(test_string) - 1)

    def test_string32(self):
        a = String32Field()
        test_string = "test_string"
        a._value = test_string

        self.assertEqual(a.size(), 32)

        packed = a.pack()
        self.assertEqual(len(packed), 32)

        # these will not equal, because the packed string is 0 padded
        self.assertNotEqual(packed, test_string)

        b = String32Field().unpack(packed)
        self.assertEqual(b.size(), 32)        

        self.assertEqual(b._value, a._value)

    def test_string64(self):
        a = String64Field()
        test_string = "test_string"
        a._value = test_string

        self.assertEqual(a.size(), 64)

        packed = a.pack()
        self.assertEqual(len(packed), 64)

        # these will not equal, because the packed string is 0 padded
        self.assertNotEqual(packed, test_string)

        b = String64Field().unpack(packed)
        self.assertEqual(b.size(), 64)        

        self.assertEqual(b._value, a._value)

    def test_string128(self):
        a = String128Field()
        test_string = "test_string"
        a._value = test_string

        self.assertEqual(a.size(), 128)

        packed = a.pack()
        self.assertEqual(len(packed), 128)

        # these will not equal, because the packed string is 0 padded
        self.assertNotEqual(packed, test_string)

        b = String128Field().unpack(packed)
        self.assertEqual(b.size(), 128)        

        self.assertEqual(b._value, a._value)

    def test_string512(self):
        a = String512Field()
        test_string = "test_string"
        a._value = test_string

        self.assertEqual(a.size(), 512)

        packed = a.pack()
        self.assertEqual(len(packed), 512)

        # these will not equal, because the packed string is 0 padded
        self.assertNotEqual(packed, test_string)

        b = String512Field().unpack(packed)
        self.assertEqual(b.size(), 512)        

        self.assertEqual(b._value, a._value)

    def test_uuid(self):
        a = UuidField()
        uuid_string = '123e4567-e89b-12d3-a456-426655440000'
        a._value = uuid_string

        self.assertEqual(a.size(), 16)

        self.assertEqual(a._value, uuid_string)

        packed = a.pack()
        self.assertEqual(len(packed), 16)            

        b = UuidField().unpack(packed)
        self.assertEqual(b.size(), 16)        

        self.assertEqual(b._value, a._value)

    def test_mac48(self):
        a = Mac48Field()
        test_string = '01:23:45:67:89:01'
        a._value = test_string

        self.assertEqual(a.size(), 6)
        self.assertEqual(a._value, test_string)
        self.assertEqual(a.__str__(), test_string)

        packed = a.pack()
        self.assertEqual(len(packed), 6)            

        b = Mac48Field().unpack(packed)
        self.assertEqual(b.size(), 6)        

        self.assertEqual(b._value, a._value)

    def test_mac64(self):
        a = Mac64Field()
        test_string = '01:23:45:67:89:01:22:33'
        a._value = test_string

        self.assertEqual(a.size(), 8)
        self.assertEqual(a._value, test_string)
        self.assertEqual(a.__str__(), test_string)

        packed = a.pack()
        self.assertEqual(len(packed), 8)            

        b = Mac64Field().unpack(packed)
        self.assertEqual(b.size(), 8)        

        self.assertEqual(b._value, a._value)

    def test_key128(self):
        a = Key128Field()
        test_string = '01020304050607080102030405060708'
        a._value = test_string
        
        self.assertEqual(a.size(), 16)
        self.assertEqual(a._value, test_string)
        self.assertEqual(a.__str__(), '16 bytes')

        packed = a.pack()
        self.assertEqual(len(packed), 16)            

        b = Key128Field().unpack(packed)
        self.assertEqual(b.size(), 16)        

        self.assertEqual(b._value, a._value)

    def test_struct(self):
        vals = {'a': 15, 'b': 32000, 'c': 'test_string', 'd': -100000}

        fields = [Uint8Field(_name="a"),
                  Uint16Field(_name="b"),
                  String32Field(_name="c"),
                  Int32Field(_name="d")]

        a = StructField(_fields=fields)
        a.a = vals['a']
        a.b = vals['b']
        a.c = vals['c']
        a.d = vals['d']
    
        self.assertEqual(a.size(), 39)

        self.assertEqual(a.a, vals['a'])
        self.assertEqual(a.b, vals['b'])
        self.assertEqual(a.c, vals['c'])
        self.assertEqual(a.d, vals['d'])
        
        packed = a.pack()
        self.assertEqual(len(packed), 39)            

        b = StructField(_fields=fields).unpack(packed)
        self.assertEqual(b.size(), 39)        

        # these will return different objects, so they will not be equal
        self.assertNotEqual(b._value, a._value)

        self.assertEqual(b.a, vals['a'])
        self.assertEqual(b.b, vals['b'])
        self.assertEqual(b.c, vals['c'])
        self.assertEqual(b.d, vals['d'])
        
        a = StructField(_fields=fields, 
                        a=vals['a'], 
                        b=vals['b'], 
                        c=vals['c'], 
                        d=vals['d'])

        self.assertEqual(a.size(), 39)

        self.assertEqual(a.a, vals['a'])
        self.assertEqual(a.b, vals['b'])
        self.assertEqual(a.c, vals['c'])
        self.assertEqual(a.d, vals['d'])


        a = StructField(_fields=fields, 
                        a=Uint8Field(vals['a']), 
                        b=Uint16Field(vals['b']), 
                        c=String32Field(vals['c']), 
                        d=Int32Field(vals['d']))

        self.assertEqual(a.size(), 39)

        self.assertEqual(a.a, vals['a'])
        self.assertEqual(a.b, vals['b'])
        self.assertEqual(a.c, vals['c'])
        self.assertEqual(a.d, vals['d'])

        
        b = StructField(_fields=fields, _value=a)
        self.assertEqual(b.size(), 39)

        self.assertEqual(b.a, vals['a'])
        self.assertEqual(b.b, vals['b'])
        self.assertEqual(b.c, vals['c'])
        self.assertEqual(b.d, vals['d'])

        
    def test_array(self):
        a = ArrayField(_field=Uint8Field)
        self.assertEqual(a.size(), 0)
        
        test_vals = [1,2,3]

        a._value = test_vals

        self.assertEqual(a.size(), 3)
        self.assertEqual(a._value[0], test_vals[0])
        self.assertEqual(a._value[1], test_vals[1])
        self.assertEqual(a._value[2], test_vals[2])
        self.assertNotEqual(id(a._value), id(test_vals))

        total = 0
        for i in a:
            total += i

        self.assertEqual(total, 6)

        a[0] = 2
        a[1] = 3
        a[2] = 4

        total = 0
        for i in a:
            total += i

        self.assertEqual(total, 9)

        self.assertEqual(len(a), 3)

        packed = a.pack()
        self.assertEqual(len(packed), 3)            

        b = ArrayField(_field=Uint8Field).unpack(packed)
        self.assertEqual(b.size(), 3)

        self.assertEqual(b._value[0], a._value[0])
        self.assertEqual(b._value[1], a._value[1])
        self.assertEqual(b._value[2], a._value[2])

        a.append(Uint8Field(4))
        self.assertEqual(len(a), 4)
        self.assertEqual(a[3], 4)


    def test_fixedarray(self):
        a = FixedArrayField(_field=Uint8Field, _length=4)
        self.assertEqual(a.size(), 4)

        try:
            a._value = [1,3,4,5,5]

        except ValueError:
            pass

        self.assertEqual(a.size(), 4)

        try:
            a.append(5)

        except ValueError:
            pass

        self.assertEqual(a.size(), 4)

        a._value = [1,3,4]

        self.assertEqual(a.size(), 4)
        self.assertEqual(a[3], 0)
        self.assertEqual(a._value, [1,3,4,0])

    def test_struct_array(self):
        vals = {'a': [1,2,3,4]}

        fields = [ArrayField(_name="a", _field=Uint8Field)]

        a = StructField(_fields=fields)
        a.a = vals['a']
        
        self.assertEqual(a.size(), 4)

        self.assertEqual(a.a[0], vals['a'][0])
        self.assertEqual(a.a[1], vals['a'][1])
        self.assertEqual(a.a[2], vals['a'][2])
        self.assertEqual(a.a[3], vals['a'][3])
        
        packed = a.pack()
        self.assertEqual(len(packed), 4)   

        b = StructField(_fields=fields).unpack(packed)
        self.assertEqual(b.size(), 4)        

        # these will return different objects, so they will not be equal
        self.assertNotEqual(b._value, a._value)

        self.assertEqual(b.a[0], vals['a'][0])
        self.assertEqual(b.a[1], vals['a'][1])
        self.assertEqual(b.a[2], vals['a'][2])
        self.assertEqual(b.a[3], vals['a'][3])
            
        # try modifying internal array 
        a.a[0] = 9
        
        self.assertEqual(a.a[0], 9)
        self.assertEqual(a.a[1], vals['a'][1])
        self.assertEqual(a.a[2], vals['a'][2])
        self.assertEqual(a.a[3], vals['a'][3])
        
        packed = a.pack()
        b = StructField(_fields=fields).unpack(packed)

        self.assertEqual(b.a[0], 9)
        self.assertEqual(b.a[1], vals['a'][1])
        self.assertEqual(b.a[2], vals['a'][2])
        self.assertEqual(b.a[3], vals['a'][3])
            
        # try conversion from string to target type
        a.a[0] = '8'    
        self.assertEqual(a.a[0], 8)

        # try the whole array
        a.a = [9,8,7,6]
        self.assertEqual(a.a[0], 9)
        self.assertEqual(a.a[1], 8)
        self.assertEqual(a.a[2], 7)
        self.assertEqual(a.a[3], 6)

        # now try strings
        a.a = ['3', '4', '5', '6']
        self.assertEqual(a.a[0], 3)
        self.assertEqual(a.a[1], 4)
        self.assertEqual(a.a[2], 5)
        self.assertEqual(a.a[3], 6)




