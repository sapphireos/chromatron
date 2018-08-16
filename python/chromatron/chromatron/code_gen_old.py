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
from pprint import pprint
import os
import ast
import struct
import sys
import random
from elysianfields import *
from catbus import *
import trig
from copy import copy

VM_ISA_VERSION  = 9

RETURN_VAL_ADDR = 0
RETURN_VAL_NAME = '___return_val'


reserved = ['pixels']

PIX_ATTRS = {
    'hue': 0,
    'sat': 1,
    'val': 2,
    'hs_fade': 3,
    'v_fade': 4,
    'count': 5,
    'size_x': 6,
    'size_y': 7,
    'index': 8,
}

ARRAY_ATTRS = [
    'hue',
    'sat',
    'val',
    'hs_fade',
    'v_fade',
    'is_fading',
]

ARRAY_OBJ_TYPE = 0
PIX_OBJ_TYPE = 1

ARRAY_FUNCS = {'len': 0,
               'min': 1, 
               'max': 2,
               'avg': 3,
               'sum': 4}

SYS_CALLS = {'sys_call_test':   0,
             'yield':           1,
             'delay':           2,
             'start_thread':    10,
             'stop_thread':     11,
             'thread_running':  12}

KV_LINK_FUNCS = ['send', 'receive', 'db']

FILE_MAGIC = 0x20205846 # 'FX  '
PROGRAM_MAGIC = 0x474f5250 # 'PROG'
FUNCTION_MAGIC = 0x434e5546 # 'FUNC'
CODE_MAGIC = 0x45444f43 # 'CODE'
DATA_MAGIC = 0x41544144 # 'DATA'
KEYS_MAGIC = 0x5359454B # 'KEYS'
META_MAGIC = 0x4154454d # 'META'


VM_STRING_LEN = 32

PUBLISHED_VAR_NAME_PREFIX = ''

DATA_LEN = 4


class ProgramHeader(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="file_magic"),
                  Uint32Field(_name="prog_magic"),
                  Uint16Field(_name="isa_version"),
                  CatbusHash(_name="program_name_hash"),
                  Uint16Field(_name="code_len"),
                  Uint16Field(_name="data_len"),
                  Uint16Field(_name="read_keys_len"),
                  Uint16Field(_name="write_keys_len"),
                  Uint16Field(_name="publish_len"),
                  Uint16Field(_name="pix_obj_len"),
                  Uint16Field(_name="link_len"),
                  Uint16Field(_name="db_len"),
                  Uint16Field(_name="padding"),
                  Uint16Field(_name="init_start"),
                  Uint16Field(_name="loop_start")]

        super(ProgramHeader, self).__init__(_name="program_header", _fields=fields, **kwargs)


class PixelArrayObject(StructField):
    def __init__(self, **kwargs):
        fields = [Uint16Field(_name="index"),
                  Uint16Field(_name="count"),
                  Uint16Field(_name="size_x"),
                  Uint16Field(_name="size_y"),
                  BooleanField(_name="reverse"),
                  ArrayField(_name="padding", _length=3, _field=Uint8Field)]

        super(PixelArrayObject, self).__init__(_name="pixel_array", _fields=fields, **kwargs)


class VMPublishVar(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="hash"),
                  Uint8Field(_name="addr"),
                  ArrayField(_name="padding", _length=3, _field=Uint8Field)]

        super(VMPublishVar, self).__init__(_name="vm_publish", _fields=fields, **kwargs)

class Link(StructField):
    def __init__(self, **kwargs):
        fields = [BooleanField(_name="send"),
                  ArrayField(_name="padding", _length=3, _field=Uint8Field),
                  CatbusHash(_name="source_hash"),
                  CatbusHash(_name="dest_hash"),
                  CatbusQuery(_name="query")]

        super(Link, self).__init__(_name="link", _fields=fields, **kwargs)


def string_hash_func(s):
    return catbus_string_hash(s)

class FunctionCallsNotSupported(Exception):
    pass

class SyntaxNotSupported(Exception):
    def __init__(self, message='', line_no=None):
        self.line_no = line_no

        if message == '':
            message = "Line: %d" % (self.line_no)

        super(SyntaxNotSupported, self).__init__(message)

class ArrayOutOfBounds(SyntaxNotSupported):
    pass

class IncorrectNumberOfParameters(SyntaxNotSupported):
    pass

class UndeclaredVariable(SyntaxNotSupported):
    pass

class ReservedKeyword(SyntaxNotSupported):
    pass

class FunctionParameterNameConflict(SyntaxNotSupported):
    pass

class FunctionNotDefined(SyntaxNotSupported):
    pass

class ObjectNotDefined(SyntaxNotSupported):
    pass

class TooManyPublishedVars(SyntaxNotSupported):
    pass

class PublishedVarNameTooLong(SyntaxNotSupported):
    pass

class UnknownInstruction(Exception):
    pass

class UnknownASTNode(Exception):
    pass

class Unknown(Exception):
    pass

class MissingInitFunction(Exception):
    pass

class MissingLoopFunction(Exception):
    pass

class TooManyVars(Exception):
    def __init__(self, message=''):
        super(TooManyVars, self).__init__(message)


class Node(object):
    def __init__(self, line_no=None):
        self.line_no = line_no

class DataNode(Node):
    def __init__(self, name=None, scope='_global', **kwargs):
        super(DataNode, self).__init__(**kwargs)
        self.scope = scope
        self.name = name

    def size(self):
        raise NotImplementedError


class VarNode(DataNode):
    def __init__(self, value, **kwargs):
        super(VarNode, self).__init__(**kwargs)

        if value:
            self.name = value

        self.value = self.name

    def __str__(self):
        return "Var{%s}" % (self.name)

class ArrayVarNode(DataNode):
    def __init__(self, value, array_type=None, **kwargs):
        super(ArrayVarNode, self).__init__(**kwargs)

        if value:
            self.name = value

        self.value = self.name
        self.type = array_type

    def __str__(self):
        return "Array{%s}" % (self.name)

class StringNode(DataNode):
    def __init__(self, value, **kwargs):
        super(StringNode, self).__init__(**kwargs)

        if value:
            self.name = value

        self.value = self.name

    def __str__(self):
        return "String{%s}" % (self.name)

class NumberNode(DataNode):
    def __init__(self, number_type='int32', **kwargs):
        super(NumberNode, self).__init__(**kwargs)

        self.value = self.name
        self.publish = False
        self.type = number_type

    def size(self):
        return 1

    def __str__(self):
        if self.publish:
            return "Number{%s, %s} (%s) [publish]" % (self.name, self.type, self.scope)

        else:
            return "Number{%s, %s} (%s)" % (self.name, self.type, self.scope)

class ConstantNode(DataNode):
    def __init__(self, value=None, **kwargs):
        super(ConstantNode, self).__init__(**kwargs)

        if value != None:
            self.name = value

        self.value = self.name
        self.type = 'int32'

        if isinstance(self.value, float):
            self.value = int(self.value * 65535) # will need to remove this when fixed16 is actually working
            self.name = self.value
            # self.type = 'fixed16'

    def size(self):
        return 1

    def __str__(self):
        return "Const(%s):%s (%s)" % (self.type, self.value, self.scope)

class ObjNode(DataNode):
    def __init__(self, obj, attr, store=False, **kwargs):
        super(ObjNode, self).__init__(**kwargs)
    
        self.name = '%s.%s' % (obj, attr)

        self.obj = obj
        self.attr = attr
        self.store = store

    def __str__(self):
        if self.store:
            return "Obj<Store>:%s" % (self.name)

        else:
            return "Obj<Load>:%s" % (self.name)

class ObjIndexNode(DataNode):
    def __init__(self, obj, attr, x=ConstantNode(65535), y=ConstantNode(65535), store=False, **kwargs):
        super(ObjIndexNode, self).__init__(**kwargs)
    
        self.name = '%s.%s' % (obj, attr)

        self.obj = obj
        self.attr = attr
        self.x = x
        self.y = y
        self.store = store

    def __str__(self):
        if self.store:
            return "ObjIndex<Store>:%s[%s][%s]" % (self.name, self.x, self.y)

        else:
            return "ObjIndex<Load>:%s[%s][%s]" % (self.name, self.x, self.y)

class ArrayDeclareNode(DataNode):
    def __init__(self, dimensions=[], data_type=NumberNode(), **kwargs):
        super(ArrayDeclareNode, self).__init__(**kwargs)
        self.dimensions = dimensions
        self.publish = False
        self.type = data_type

    @property
    def stride(self):
        return self.type.size()

    @property
    def count(self):
        c = 1
        for dim in self.dimensions:
            c *= dim

        return c

    @property
    def depth(self):
        return len(self.dimensions)

    def size(self):
        return self.count * self.stride

    def __str__(self):
        return "ArrayDeclare:%s [%s, %s, depth=%d] (%s)" % (self.name, self.count, self.type, self.depth, self.scope)


class ArrayIndexNode(DataNode):
    def __init__(self, obj, offsets=[], store=False, **kwargs):
        super(ArrayIndexNode, self).__init__(**kwargs)
    
        self.name = '%s' % (obj)

        self.obj = obj
        self.offsets = offsets
        self.store = store

    def __str__(self):
        if self.store:
            return "ArrayIndex<Store>:%s" % (self.name)

        else:
            return "ArrayIndex<Load>:%s" % (self.name)

class RecordDeclareNode(DataNode):
    def __init__(self, fields, **kwargs):
        super(RecordDeclareNode, self).__init__(**kwargs)

        self.fields = fields

    def size(self):
        data_size = 0
        for f in self.fields:
            data_size += f.size()

        return data_size

    def __str__(self):
        return "RecordDeclare:%s [%s] (%s)" % (self.name, len(self.fields), self.scope)

class RecordInstanceNode(DataNode):
    def __init__(self, name, record_type, **kwargs):
        super(RecordInstanceNode, self).__init__(**kwargs)

        self.name = name
        self.type = record_type

    def size(self):
        return self.type.size()

    def __str__(self):
        return "RecordInstance:%s [%s] (%s)" % (self.name, self.type.name, self.scope)


class ParameterNode(DataNode):
    def __init__(self, **kwargs):
        super(ParameterNode, self).__init__(**kwargs)

    def __str__(self):
        return "Param:%s" % (self.name)


class CodeNode(Node):
    pass

class NoOpCodeNode(CodeNode):
    pass

class ModuleNode(CodeNode):
    def __init__(self, body, **kwargs):
        super(ModuleNode, self).__init__(**kwargs)
        self.body = body

class IfNode(CodeNode):
    def __init__(self, test, body, orelse, **kwargs):
        super(IfNode, self).__init__(**kwargs)
        self.test = test
        self.body = body
        self.orelse = orelse

class NotNode(CodeNode):
    def __init__(self, value, **kwargs):
        super(NotNode, self).__init__(**kwargs)
        self.value = value
        
class BinOpNode(CodeNode):
    def __init__(self, op, left, right, **kwargs):
        super(BinOpNode, self).__init__(**kwargs)
        self.op = op
        self.left = left
        self.right = right

class CompareNode(BinOpNode):
    def __init__(self, *args, **kwargs):
        super(CompareNode, self).__init__(*args, **kwargs)

class AssignNode(CodeNode):
    def __init__(self, name, value, **kwargs):
        super(AssignNode, self).__init__(**kwargs)
        self.name = name
        self.value = value

class AugAssignNode(CodeNode):
    def __init__(self, name, value, **kwargs):
        super(AugAssignNode, self).__init__(**kwargs)
        self.name = name
        self.value = value

class AssertNode(CodeNode):
    def __init__(self, test, **kwargs):
        super(AssertNode, self).__init__(**kwargs)
        self.test = test

class WhileLoopNode(CodeNode):
    def __init__(self, test, body, **kwargs):
        super(WhileLoopNode, self).__init__(**kwargs)
        self.test = test
        self.body = body

class ForLoopNode(CodeNode):
    def __init__(self, iterator, target, body, **kwargs):
        super(ForLoopNode, self).__init__(**kwargs)
        self.iterator = iterator
        self.target = target
        self.body = body

class FunctionNode(CodeNode):
    def __init__(self, name, body, params, return_val, **kwargs):
        super(FunctionNode, self).__init__(**kwargs)
        self.name = name
        self.body = body
        self.params = params
        self.return_val = return_val
        self.decorators = []

class CallNode(CodeNode):
    def __init__(self, name, params, **kwargs):
        super(CallNode, self).__init__(**kwargs)
        self.name = name
        self.params = params

class ReturnNode(CodeNode):
    def __init__(self, value, **kwargs):
        super(ReturnNode, self).__init__(**kwargs)
        self.value = value

class BreakNode(CodeNode):
    pass

class ContinueNode(CodeNode):
    pass

class PassNode(CodeNode):
    pass

class PrintNode(CodeNode):
    def __init__(self, target, **kwargs):
        super(PrintNode, self).__init__(**kwargs)
        self.target = target

class PixelArrayNode(Node):
    def __init__(self, index, length, size_x=1, size_y=1, reverse=False, **kwargs):
        super(PixelArrayNode, self).__init__(**kwargs)
        self.name = ''
        self.index = index
        self.length = length
        self.size_x = size_x
        self.size_y = size_y
        self.reverse = reverse
        
    def __str__(self):
        return "PIXARRAY:%s" % (self.name)

class LenNode(CodeNode):
    def __init__(self, value, **kwargs):
        super(LenNode, self).__init__(**kwargs)
        self.value = value

    def __str__(self):
        return "LEN:%s" % (self.value)

class ArrayFuncNode(CodeNode):
    def __init__(self, func, op1, **kwargs):
        super(ArrayFuncNode, self).__init__(**kwargs)
        self.func = func
        self.op1 = op1

    def __str__(self):
        return "ArrayFunc:%s -> %s" % (self.func, self.op1)

class SysCallNode(CodeNode):
    def __init__(self, name, params, **kwargs):
        super(SysCallNode, self).__init__(**kwargs)
        self.name = name
        self.params = params

class SubscriptNode(CodeNode):
    def __init__(self, obj, index, store=False, **kwargs):
        super(SubscriptNode, self).__init__(**kwargs)
        self.obj = obj
        self.index = index
        self.store = store

    def __str__(self):
        if self.store:
            return "SUBSCRIPT:%s[%s] (store)" % (self.obj, self.index)

        else:
            return "SUBSCRIPT:%s[%s] (load)" % (self.obj, self.index)

class AttributeNode(CodeNode):
    def __init__(self, obj, attr, store=False, **kwargs):
        super(AttributeNode, self).__init__(**kwargs)
        self.obj = obj
        self.attr = attr
        self.store = store

    def __str__(self):
        if self.store:
            return "ATTR:%s.%s (store)" % (self.obj, self.attr)

        else:
            return "ATTR:%s.%s (load)" % (self.obj, self.attr)


class ASTWalker(object):
    def __init__(self):
        pass

    def generate(self, node):
        if isinstance(node, ModuleNode):
            for i in node.body:
                self.generate(i)

        elif isinstance(node, FunctionNode):
            for i in node.body:
                self.generate(i)

            self.generate(node.return_val)

        elif isinstance(node, CallNode):
            for param in node.params:
                self.generate(param)

        elif isinstance(node, IfNode):
            self.generate(node.test)

            for i in node.body:
                self.generate(i)

            for i in node.orelse:
                self.generate(i)

        elif isinstance(node, BinOpNode):
            self.generate(node.left)
            self.generate(node.right)

        elif isinstance(node, CompareNode):
            self.generate(node.left)
            self.generate(node.right)

        elif isinstance(node, WhileLoopNode):
            self.generate(node.test)

            for i in node.body:
                self.generate(i)

        elif isinstance(node, ForLoopNode):
            self.generate(node.iterator)

            for i in node.body:
                self.generate(i)

        elif isinstance(node, AssignNode):
            self.generate(node.name)
            self.generate(node.value)

        elif isinstance(node, IndexLoadNode):
            self.generate(node.name)
            self.generate(node.x)
            self.generate(node.y)

        elif isinstance(node, IndexStoreNode):
            self.generate(node.name)
            self.generate(node.x)
            self.generate(node.y)

        elif isinstance(node, BreakNode):
            pass

        elif isinstance(node, ContinueNode):
            pass

        elif isinstance(node, PassNode):
            pass

        elif isinstance(node, PrintNode):
            self.generate(node.target)

        elif isinstance(node, ReturnNode):
            self.generate(node.value)

        elif isinstance(node, AssertNode):
            self.generate(node.test)

        elif isinstance(node, NumberNode):
            pass

        elif isinstance(node, Fixed16Node):
            pass

        elif isinstance(node, ArrayDeclareNode):
            pass

        elif isinstance(node, VarNode):
            pass

        elif isinstance(node, ArrayVarNode):
            pass

        elif isinstance(node, ConstantNode):
            pass

        elif isinstance(node, ConstantFixed16Node):
            pass

        elif isinstance(node, ParameterNode):
            pass

        elif isinstance(node, RecordDeclareNode):
            pass

        elif isinstance(node, basestring):
            pass

        else:
            raise Exception(node)


class ASTPrinter(object):
    def __init__(self, tree):
        self.level = -1

        self.tree = tree
        self.indent = 4
        self.current_line = 0

    def render(self):
        self.generate(self.tree)

    def echo(self, s):
        if self.current_line == None:
            line_no = '   '

        else:
            line_no = '%3d' % (self.current_line)

        line = '%s %s%s' % (line_no, ' ' * self.indent * self.level, s)

        print line

    def generate(self, node):
        self.level += 1

        try:
            self.current_line = node.line_no

        except AttributeError:
            pass


        if isinstance(node, ModuleNode):
            self.echo('MODULE')

            for i in node.body:
                self.generate(i)

        elif isinstance(node, FunctionNode):
            self.echo('\nFUNCTION %s' % (node.name))

            for i in node.body:
                self.generate(i)

            self.generate(node.return_val)

        elif isinstance(node, CallNode):
            self.echo('CALL %s' % (node.name))

            for param in node.params:
                self.generate(param)

        elif isinstance(node, SysCallNode):
            self.echo('SYSCALL %s' % (node.name))

            for param in node.params:
                self.generate(param)

        elif isinstance(node, NotNode):
            self.echo('NOT')

            self.generate(node.value)

        elif isinstance(node, IfNode):
            self.echo('IF')

            self.generate(node.test)

            for i in node.body:
                self.echo('THEN')
                self.generate(i)

            for i in node.orelse:
                self.echo('ELSE')
                self.generate(i)

        elif isinstance(node, BinOpNode):
            self.echo('BINOP(%s)' % (node.op))

            self.generate(node.left)
            self.generate(node.right)

        elif isinstance(node, CompareNode):
            self.echo('COMPARE(%s)' % (node.op))

            self.generate(node.left)
            self.generate(node.right)

        elif isinstance(node, WhileLoopNode):
            self.echo('WHILE')

            self.generate(node.test)

            for i in node.body:
                self.generate(i)

        elif isinstance(node, ForLoopNode):
            self.echo('FOR %s < %s' % (node.target, node.iterator))

            for i in node.body:
                self.generate(i)

        elif isinstance(node, AssignNode):
            if isinstance(node.name, VarNode):
                self.echo('ASSIGN %s =' % (node.name))

                self.generate(node.value)

            else:
                self.echo('ASSIGN')
                self.generate(node.name)
                self.generate(node.value)

        elif isinstance(node, AugAssignNode):
            if isinstance(node.name, VarNode):
                self.echo('AUGASSIGN %s =' % (node.name))

                self.generate(node.value)

            else:
                self.echo('AUGASSIGN')
                self.generate(node.name)
                self.generate(node.value)

        elif isinstance(node, BreakNode):
            self.echo('BREAK')

        elif isinstance(node, ContinueNode):
            self.echo('CONTINUE')

        elif isinstance(node, PassNode):
            self.echo('PASS')

        elif isinstance(node, PrintNode):
            self.echo('PRINT')
            self.generate(node.target)

        elif isinstance(node, ReturnNode):
            self.echo('RETURN')
            self.generate(node.value)

        elif isinstance(node, AssertNode):
            self.generate(node.test)

        elif isinstance(node, NumberNode):
            self.echo(node)

        elif isinstance(node, ArrayDeclareNode):
            self.echo(node)

        elif isinstance(node, ArrayIndexNode):
            self.echo(node)

        elif isinstance(node, VarNode):
            self.echo('VAR(%s)' % (node.name))

        elif isinstance(node, ArrayVarNode):
            self.echo('ARRAY(%s)' % (node.name))

        elif isinstance(node, ObjNode):
            self.echo(node)

        elif isinstance(node, ObjIndexNode):
            self.echo(node)

        elif isinstance(node, StringNode):
            self.echo('STR(%s)' % (node.name))

        elif isinstance(node, ConstantNode):
            self.echo(node)

        elif isinstance(node, ParameterNode):
            pass

        elif isinstance(node, RecordDeclareNode):
            self.echo(node)

        elif isinstance(node, RecordInstanceNode):
            self.echo(node)

        elif isinstance(node, PixelArrayNode):
            self.echo('PIXARRAY(%s)' % (node.name))

        elif isinstance(node, LenNode):
            self.echo('LEN(%s)' % (node.value))

        elif isinstance(node, basestring):
            pass

        elif isinstance(node, ArrayFuncNode):
            self.echo(node)

        elif node is None:
            self.echo('NONE')

        elif isinstance(node, NoOpCodeNode):
            pass

        elif isinstance(node, AttributeNode):
            self.echo(node)

        elif isinstance(node, SubscriptNode):
            self.echo(node)

        elif isinstance(node, list):
            self.echo('LIST %s' % (str(node)))

        else:
            raise Exception(node)

        self.level -= 1

#
# Pass 0
# Initial pass
# Process source text through Python's compiler.
# Handle import statements.
# Handle record declarations.
#
class CodeGeneratorPass0(object):
    def __init__(self):
        pass

    def generate(self, text):
        tree = ast.parse(text)

        assert isinstance(tree, ast.Module)

        imported_modules = []

        for i in xrange(len(tree.body)):
            node = tree.body[i]
        
            if isinstance(node, ast.Import):
                for file in [a.name for a in node.names]:
                
                    with open(file, 'r') as f:
                        module = self.generate(f.read())

                        imported_modules.append(module)


                # remove imports from module level
                tree.body[i] = None

            elif isinstance(node, ast.Assign):
                if node.value.func.id == 'Record':
                    # replace item with record node

                    tree.body[i] = RecordDeclareNode(node.value.keywords, line_no=node.lineno)
                    tree.body[i].name = node.targets[0].id


        # strip nones
        tree.body = [a for a in tree.body if a != None]

        # rewrite top level module body with contents from imported modules,
        # followed by the original top level body
        imported_modules.append(copy(tree))
        tree.body = []

        for module in imported_modules:
            tree.body.extend(module.body)

        return tree

        
#
# Pass 1
# Translates Python's AST to a simpler form
#
class CodeGeneratorPass1(object):
    def __init__(self):
        self.current_function = "_global"

        self.records = {}
        self.record_types = {}
        self.arrays = {}

    def generate(self, tree):
        if isinstance(tree, ast.Module):
            temp_code = []

            for node in tree.body:
                # this is all code occurring until the first
                # function definition
                if isinstance(node, ast.FunctionDef):

                    # generate code for this node
                    temp_code.append(self.generate(node))

                else:
                    temp_code.append(self.generate(node))

            return ModuleNode(temp_code, line_no=0)

        elif tree == None:
            # None is ok, we get these when imports
            # are stripped out.
            return NoOpCodeNode()

        elif isinstance(tree, ast.Import):
            # this is a syntax error.
            # this would generally be an import that is
            # not at module level
            raise SyntaxNotSupported("Imports can only be at module level.", line_no=tree.lineno)

        # This is somewhat special handling for expressions.
        # We shouldn't get this in a normal program.
        elif isinstance(tree, ast.Expression):
            body_code = []

            return_val = VarNode('___return_val')

            # since this is an expression, we want to store the expression
            # result in the return register.
            body_code.append(AssignNode(return_val, self.generate(tree.body), line_no=0))

            # add return statement so the VM can terminate properly.
            body_code.append(ReturnNode(return_val, line_no=0))

            self.current_function = '___expr'
            
            f = FunctionNode(self.current_function, body_code, [], return_val, line_no=0)

            return ModuleNode([f], line_no=0)

        elif isinstance(tree, ast.FunctionDef):
            last_func = self.current_function
            self.current_function = tree.name

            params = []
            for arg in tree.args.args:
                param = self.generate(arg)
                param.scope = self.current_function
                params.append(param)

            body_code = []
            return_val = ConstantNode(0, line_no=tree.lineno)

            for node in tree.body:
                body_code.append(self.generate(node))

            self.current_function = last_func
            f = FunctionNode(tree.name, body_code, params, return_val, line_no=tree.lineno)

            for dec in tree.decorator_list:
                f.decorators.append(dec.id)

            return f

        elif isinstance(tree, ast.Expr):
            return self.generate(tree.value)

        elif isinstance(tree, ast.Return):
            if tree.value == None:
                return_val = ConstantNode(0, line_no=tree.lineno)

            else:
                return_val = self.generate(tree.value)

            return ReturnNode(return_val, line_no=tree.lineno)

        elif isinstance(tree, ast.Call):
            if tree.func.id == "Number" or tree.func.id == "Fixed16":
                node = NumberNode(scope=self.current_function, line_no=tree.lineno)

                if len(tree.keywords) == 1:
                    if tree.keywords[0].arg == 'publish' and \
                       tree.keywords[0].value.id == 'True':

                       node.publish = True

                if tree.func.id == "Fixed16":
                    node.type = 'fixed16'

                return node
            
            elif tree.func.id == "PixelArray":
                index = self.generate(tree.args[0])
                length = self.generate(tree.args[1])

                keywords = {}
                for keyword in tree.keywords:
                    keywords[keyword.arg] = keyword

                size_x = ConstantNode(1, line_no=tree.lineno)
                if 'size_x' in keywords:
                    size_x = self.generate(keywords['size_x'].value)

                size_y = ConstantNode(1, line_no=tree.lineno)
                if 'size_y' in keywords:
                    size_y = self.generate(keywords['size_y'].value)

                reverse = False
                if 'reverse' in keywords:
                    if keywords['reverse'].value.id.lower() == 'true':
                        reverse = True

                node = PixelArrayNode(index.value, length.value, size_x=size_x.value, size_y=size_y.value, reverse=reverse, line_no=tree.lineno)
                return node

            # check if creating an array
            # this is implemented as a function in Python,
            # but we handle it differently here and use
            # it as a type directly supported by our VM.
            elif tree.func.id == "Array":
                size = tree.args[0].n

                if len(tree.args) < 1:
                    raise SyntaxNotSupported(line_no=tree.lineno)

                dimensions = [n.n for n in tree.args]
                array = ArrayDeclareNode(dimensions, line_no=tree.lineno, scope=self.current_function)

                for kw in tree.keywords:
                    if kw.arg == 'publish' and \
                       kw.value.id == 'True':

                       array.publish = True

                    elif kw.arg == 'type':
                        array.type = self.generate(kw.value)

                return array

            elif tree.func.id == "Record":
                raise SyntaxNotSupported("Records must be declared at module level", line_no=tree.lineno)
                
            # is this creating an instance of a record?
            elif tree.func.id in self.record_types:
                return RecordInstanceNode(tree.func.id, self.record_types[tree.func.id], line_no=tree.lineno)

            elif tree.func.id == "len":
                node = self.generate(tree.args[0])
                return LenNode(node, line_no=tree.lineno)

            elif tree.func.id in ARRAY_FUNCS:
                node = self.generate(tree.args[0])
                return ArrayFuncNode(tree.func.id, node, line_no=tree.lineno)

            params = []

            for arg in tree.args:
                params.append(self.generate(arg))

            if (tree.func.id in SYS_CALLS) or (tree.func.id in KV_LINK_FUNCS):
                return SysCallNode(tree.func.id, params, line_no=tree.lineno)

            return CallNode(tree.func.id, params, line_no=tree.lineno)

        elif isinstance(tree, ast.If):

            # get code for test
            test_code = self.generate(tree.test)

            # get code for body
            body_code = []
            for c in tree.body:
                body_code.append(self.generate(c))

            # get orelse code
            orelse_code = []
            for c in tree.orelse:
                orelse_code.append(self.generate(c))

            return IfNode(test_code, body_code, orelse_code, line_no=tree.lineno)

        elif isinstance(tree, ast.UnaryOp):
            if isinstance(tree.op, ast.Not):
                return NotNode(self.generate(tree.operand), line_no=tree.lineno)

        elif isinstance(tree, ast.BoolOp):
            op = tree.op
            op_code = self.generate(op)

            left = tree.values.pop(0)
            left_code = self.generate(left)

            while len(tree.values) > 0:
                right = tree.values.pop(0)
                right_code = self.generate(right)
                
                result = BinOpNode(op_code, left_code, right_code, line_no=tree.lineno)
                
                left_code = result

            return result

        elif isinstance(tree, ast.BinOp):
            left = self.generate(tree.left)
            right = self.generate(tree.right)
            op_code = self.generate(tree.op)

            return BinOpNode(op_code, left, right, line_no=tree.lineno)

        elif isinstance(tree, ast.Compare):
            if len(tree.ops) > 1:
                raise SyntaxNotSupported(line_no=tree.lineno)

            if len(tree.comparators) > 1:
                raise SyntaxNotSupported(line_no=tree.lineno)(line_no=tree.lineno)

            left = self.generate(tree.left)
            right = self.generate(tree.comparators[0])
            op = self.generate(tree.ops[0])

            return CompareNode(op, left, right, line_no=tree.lineno)

        elif isinstance(tree, ast.Assign):
            target = tree.targets[0]
            dest = self.generate(target)
            value = self.generate(tree.value)

            if isinstance(value, NumberNode) or \
               isinstance(value, PixelArrayNode) or \
               isinstance(value, ArrayDeclareNode) or \
               isinstance(value, RecordInstanceNode) or \
               isinstance(value, RecordDeclareNode):

                value.name = dest.name

                if isinstance(value, ArrayDeclareNode):
                    self.arrays[value.name] = value

                    # # create sub arrays for multidimensional arrays
                    # array_type = value.type
                    # while True:
                    #     if isinstance(array_type, ArrayDeclareNode):
                    #         array_type.name = '%s.%d' % (value.name, array_type.depth)
                    #         self.arrays[array_type.name] = array_type

                    #     else:
                    #         break

                    #     array_type.depth += 1
                    #     array_type = array_type.type

                    #     print array_type

                elif isinstance(value, RecordInstanceNode):
                    self.records[value.name] = value

                return value

            else:
                return AssignNode(dest, value, line_no=tree.lineno)

        elif isinstance(tree, ast.AugAssign):
            try:
                if tree.target.id in self.arrays:
                    left = ArrayVarNode(tree.target.id, line_no=tree.lineno, scope=self.current_function)

                else:
                    left = VarNode(tree.target.id, line_no=tree.lineno, scope=self.current_function)
                    
            except AttributeError:
                left = self.generate(tree.target)

            dest = copy(left)

            if isinstance(left, ObjNode) or isinstance(left, ObjIndexNode) or isinstance(left, ArrayIndexNode):
                left.store = False

            right = self.generate(tree.value)
            
            op_code = self.generate(tree.op)

            return AugAssignNode(dest, BinOpNode(op_code, left, right, line_no=tree.lineno), line_no=tree.lineno)

        elif isinstance(tree, ast.While):
            test = self.generate(tree.test)

            body_code = []

            # now for the loop body code
            for c in tree.body:
                body_code.append(self.generate(c))

            return WhileLoopNode(test, body_code, line_no=tree.lineno)

        elif isinstance(tree, ast.For):
            # Check for an else clause.  Python has an atypical else construct
            # you can use at the end of a for loop.  But it is confusing and
            # rarely used, so we are not going to support it.
            if len(tree.orelse) > 0:
                raise SyntaxNotSupported(line_no=tree.lineno)

            target_dest = self.generate(tree.target)
            iter_name = self.generate(tree.iter)

            body_code = []

            # now for the loop body code
            for c in tree.body:
                body_code.append(self.generate(c))

            return ForLoopNode(iter_name, target_dest, body_code, line_no=tree.lineno)

        elif isinstance(tree, ast.Break):
            return BreakNode(line_no=tree.lineno)

        elif isinstance(tree, ast.Continue):
            return ContinueNode(line_no=tree.lineno)

        elif isinstance(tree, ast.Attribute):    
            # attr = tree.attr
            # obj = self.generate(tree.value)

            # if isinstance(tree.ctx, ast.Store):
            #     store = True

            # else:
            #     store = False

            # return AttributeNode(obj, attr, store, line_no=tree.lineno)

            if isinstance(tree.value, ast.Subscript):
                # check for second array index
                if isinstance(tree.value.value, ast.Subscript):
                    obj = tree.value.value.value.id
                    x = self.generate(tree.value.value.slice.value)
                    y = self.generate(tree.value.slice.value)

                else:
                    obj = tree.value.value.id
                    y = ConstantNode(65535)
                    x = self.generate(tree.value.slice.value)
                
                attr = tree.attr

                if isinstance(tree.ctx, ast.Store):
                    store = True

                else:
                    store = False

                return ObjIndexNode(obj, attr, x, y, store, line_no=tree.lineno)

            else:                
                obj = tree.value.id
                attr = tree.attr

                if isinstance(tree.ctx, ast.Store):
                    store = True

                else:
                    store = False

                return ObjNode(obj, attr, store, line_no=tree.lineno)

        elif isinstance(tree, ast.Subscript):
            try:
                # this is basically a syntax rewrite.
                # the code for ast.Attribute already does what we want here,
                # so we are rearranging the syntax to match and then reusing
                # that code.
                sub = tree
                attr = tree.value

                # check for multiple dimensions - that would be an array access
                if isinstance(attr, ast.Subscript):
                    raise AttributeError

                attr_value = tree.value.value
                ctx = sub.ctx

                sub.value = attr_value

                attr.value = sub
                attr.ctx = ctx

                return self.generate(attr)

            except AttributeError:
                # unless we are indexing an array
                offsets = []

                store = False
                if isinstance(tree.ctx, ast.Store):
                    store = True

                while isinstance(tree, ast.Subscript):
                    index = self.generate(tree.slice.value)
                    offsets.append(index)

                    tree = tree.value

                assert not isinstance(tree, ast.Subscript)

                target = self.generate(tree)

                # reverse list of offsets so they read from left to right
                offsets = list(reversed(offsets))

                node = ArrayIndexNode(target, offsets, store=store, line_no=tree.lineno)

                return node



            # obj = self.generate(tree.value)
            # index = self.generate(tree.slice.value)

            # if isinstance(tree.ctx, ast.Store):
            #     store = True

            # else:
            #     store = False

            # return SubscriptNode(obj, index, store, line_no=tree.lineno)

            # try:
            #     # this is basically a syntax rewrite.
            #     # the code for ast.Attribute already does what we want here,
            #     # so we are rearranging the syntax to match and then reusing
            #     # that code.
            #     sub = tree
            #     attr = tree.value

            #     # check for multiple dimensions - that would be an array access
            #     if isinstance(attr, ast.Subscript):
            #         raise AttributeError

            #     attr_value = tree.value.value
            #     ctx = sub.ctx

            #     sub.value = attr_value

            #     attr.value = sub
            #     attr.ctx = ctx

            #     return self.generate(attr)

            # except AttributeError:
            #     # unless we are indexing an array

            #     offsets = []

            #     store = False
            #     if isinstance(tree.ctx, ast.Store):
            #         store = True


            #     while isinstance(tree, ast.Subscript):
                    
            #         index = self.generate(tree.slice.value)
            #         offsets.append(index)

            #         tree = tree.value

            #     assert not isinstance(tree, ast.Subscript)

            #     target = self.generate(tree)

            #     # reverse list of offsets so they read from left to right
            #     offsets = list(reversed(offsets))

            #     node = ArrayIndexNode(target, offsets, store=store, line_no=tree.lineno)

            #     return node

        elif isinstance(tree, ast.Eq):
            return 'eq'

        elif isinstance(tree, ast.NotEq):
            return 'neq'

        elif isinstance(tree, ast.Gt):
            return 'gt'

        elif isinstance(tree, ast.GtE):
            return 'gte'

        elif isinstance(tree, ast.Lt):
            return 'lt'

        elif isinstance(tree, ast.LtE):
            return 'lte'

        elif isinstance(tree, ast.And):
            return 'logical_and'

        elif isinstance(tree, ast.Or):
            return 'logical_or'

        elif isinstance(tree, ast.Add):
            return 'add'

        elif isinstance(tree, ast.Sub):
            return 'sub'

        elif isinstance(tree, ast.Mult):
            return 'mult'

        elif isinstance(tree, ast.Div):
            return 'div'

        elif isinstance(tree, ast.Mod):
            return 'mod'

        elif isinstance(tree, ast.Pass):
            return PassNode(line_no=tree.lineno)

        elif isinstance(tree, ast.Print):
            try:
                return PrintNode(VarNode(tree.values[0].id, scope=self.current_function), line_no=tree.lineno)

            except AttributeError:
                return PrintNode(self.generate(tree.values[0]), line_no=tree.lineno)

        elif isinstance(tree, ast.Name):
            if tree.id == 'True':
                return ConstantNode(1, line_no=tree.lineno)

            elif tree.id == 'False':
                return ConstantNode(0, line_no=tree.lineno)                

            if tree.id in self.arrays:
                return ArrayVarNode(tree.id, array_type=self.arrays[tree.id], line_no=tree.lineno, scope=self.current_function)

            else:
                return VarNode(tree.id, line_no=tree.lineno, scope=self.current_function)

        elif isinstance(tree, ast.Num):
            return ConstantNode(tree.n, line_no=tree.lineno)

        elif isinstance(tree, ast.Str):
            return StringNode(tree.s)

        elif isinstance(tree, ast.Tuple):
            raise SyntaxNotSupported(line_no=tree.lineno)

        elif isinstance(tree, ast.Assert):
            return AssertNode(self.generate(tree.test), line_no=tree.lineno)

        elif isinstance(tree, ast.List):
            l = []

            for item in tree.elts:
                l.append(self.generate(item))

            return l

        elif isinstance(tree, RecordDeclareNode):
            fields = []

            for kw in tree.fields:
                field = self.generate(kw.value)
                field.name = kw.arg

                fields.append(field)

            tree.fields = fields

            if len(fields) == 0:
                raise SyntaxNotSupported(message='Cannot create empty record', line_no=node.value.lineno)

            self.record_types[tree.name] = tree

            return tree

        else:
            raise UnknownASTNode(tree)


class IntermediateNode(Node):
    def __init__(self, level=0, line_no=0, **kwargs):
        super(IntermediateNode, self).__init__(**kwargs)
        self.level = level
        self.indent = '  '
        self.line_no = line_no

    def get_data_nodes(self):
        return []

class DataIR(IntermediateNode):
    def __init__(self, **kwargs):
        super(DataIR, self).__init__(**kwargs)
        self.function = None
        self.addr = -1
        self.publish = False

    def size(self):
        raise NotImplementedError

class VarIR(DataIR):
    def __init__(self, name, publish=False, declared=False, number_type='int32', **kwargs):
        super(VarIR, self).__init__(**kwargs)
        self.name = name
        self.publish = publish
        self.declared = declared
        self.type = number_type

        if self.name in reserved:
            raise ReservedKeyword(self.name)

    def size(self):
        if self.type == 'int32':
            return 1

        raise NotImplementedError

    def __str__(self):
        if self.publish:
            return 'Var(%s, %s)<%s>@%d (publish)' % (self.name, self.type, self.function, self.addr)

        else:
            return 'Var(%s, %s)<%s>@%d' % (self.name, self.type, self.function, self.addr)

class ArrayVarIR(DataIR):
    def __init__(self, name, data_type=None, **kwargs):
        super(ArrayVarIR, self).__init__(**kwargs)
        
        self.name = name
        self.length = -1
        self.type = data_type

    def get_data_nodes(self):
        return [ConstIR(self.length), ConstIR(self.stride)]

    @property
    def stride(self):
        return self.type.size()

    def size(self):
        return self.length * self.stride

    def __str__(self):
        return 'ArrayVar(%s)[%d/%d+%d]<%s>@%d' % (self.name, self.length, self.size(), self.stride, self.function, self.addr)

class RecordVarIR(DataIR):
    def __init__(self, name, record_type=None, **kwargs):
        super(RecordVarIR, self).__init__(**kwargs)
        
        self.name = name
        self.type = record_type

        self.fields = {}
        for f in self.type.fields:
            try:
                self.fields[f.name] = f

            except AttributeError:
                self.fields[f] = self.type.fields[f]

    def size(self):
        return self.type.size()

    def translate_field(self, field):
        i = 0
        for f, v in self.fields.iteritems():
            if f == field:
                return i
            i += v.size()

        raise KeyError(field)
        
    def __str__(self):
        return 'RecordVar(%s)[%s]<%s>@%d' % (self.name, self.type.name, self.function, self.addr)

class ObjIR(DataIR):
    def __init__(self, name, **kwargs):
        super(ObjIR, self).__init__(**kwargs)
        tokens = name.split('.')
        self.obj = tokens[0]
        self.attr = tokens[1]

        self.name = name

        self.size = 0
        
    def __str__(self):
        return 'Obj(%s.%s)<%s>@%d' % (self.obj, self.attr, self.function, self.addr)

class PixelObjIR(ObjIR):
    pass

class TempIR(DataIR):
    def __init__(self, name, number_type='int32',**kwargs):
        super(TempIR, self).__init__(**kwargs)
        self.name = name
        self.type = number_type

    def size(self):
        return 1

    def __str__(self):
        return 'Temp(%s, %s)<%s>@%d' % (self.name, self.type, self.function, self.addr)

class ConstIR(DataIR):
    def __init__(self, name, number_type='int32', **kwargs):
        super(ConstIR, self).__init__(**kwargs)
        self.name = name
        self.type = number_type

    def size(self):
        return 1

    def __str__(self):
        return 'Const(%s, %s)<%s>@%d' % (self.name, self.type, self.function, self.addr)

class StringIR(DataIR):
    def __init__(self, name, **kwargs):
        super(StringIR, self).__init__(**kwargs)
        self.name = name

    def __str__(self):
        return 'Str(%s)@%d' % (self.name, self.addr)

    def to_hash(self):
        return string_hash_func(self.name)

class KeyIR(StringIR):
    def __init__(self, name, **kwargs):
        super(KeyIR, self).__init__(name, **kwargs)

    def __str__(self):
        return 'Key(%s)@%d' % (self.name, self.addr)

class DefineNumberIR(IntermediateNode):
    def __init__(self, name, number_type="int32", **kwargs):
        super(DefineNumberIR, self).__init__(**kwargs)
        self.name = name
        self.publish = False
        self.type = number_type

    def size(self):
        return 1

    def __str__(self):
        if self.publish:
            return '%3d %s DEFINE %s %s (publish)' % (self.line_no, self.indent * self.level, self.type, self.name)

        else:
            return '%3d %s DEFINE %s %s' % (self.line_no, self.indent * self.level, self.type, self.name)

    def get_data_nodes(self):
        return [VarIR(self.name, publish=self.publish, number_type=self.type)]

class UndefineIR(IntermediateNode):
    def __init__(self, name, **kwargs):
        super(UndefineIR, self).__init__(**kwargs)
        self.name = name

    def __str__(self):
        return '%3d %s UNDEFINE %s' % (self.line_no, self.indent * self.level, self.name)

class FunctionIR(IntermediateNode):
    def __init__(self, name, **kwargs):
        super(FunctionIR, self).__init__(**kwargs)
        self.name = name
        
    def __str__(self):    
        return '%3d %s FUNCTION %s' % (self.line_no, self.indent * self.level, self.name)

class EndFunctionIR(IntermediateNode):
    def __init__(self, name, **kwargs):
        super(EndFunctionIR, self).__init__(**kwargs)
        self.name = name

    def __str__(self):
        return '%3d %s ENDFUNCTION %s' % (self.line_no, self.indent * self.level, self.name)


class CallIR(IntermediateNode):
    def __init__(self, name, dest, params, **kwargs):
        super(CallIR, self).__init__(**kwargs)
        self.name = name
        self.params = params
        self.dest = dest

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        self.dest = new_dest

    def get_data_nodes(self):
        # make copy of list so we don't modify it
        data = [a for a in self.params]
        data.append(self.dest)
        return data

    def __str__(self):
        s = '%3d %s CALL %s -> %s' % (self.line_no, self.indent * self.level, self.name, self.dest)

        for param in self.params:
            s += '\n    %s %s' % (self.indent * (self.level + 1), param)

        return s

class SysCallIR(CallIR):
    def __init__(self, *args, **kwargs):
        super(SysCallIR, self).__init__(*args, **kwargs)

    def __str__(self):
        s = '%3d %s SYSCALL %s -> %s' % (self.line_no, self.indent * self.level, self.name, self.dest)

        for param in self.params:
            s += '\n    %s %s' % (self.indent * (self.level + 1), param)

        return s


class ParamIR(IntermediateNode):
    def __init__(self, name, **kwargs):
        super(ParamIR, self).__init__(**kwargs)
        self.name = name

    def __str__(self):
        return '%3d %s PARAM %s' % (self.line_no, self.indent * self.level, self.name)

    def get_data_nodes(self):
        return [self.name]

class ReturnIR(IntermediateNode):
    def __init__(self, name, **kwargs):
        super(ReturnIR, self).__init__(**kwargs)
        self.name = name

    def get_data_nodes(self):
        return [self.name]

    def __str__(self):
        return '%3d %s RETURN %s' % (self.line_no, self.indent * self.level, self.name)


class ExpressionIR(IntermediateNode):
    def __init__(self, dest, ops, **kwargs):
        super(ExpressionIR, self).__init__(**kwargs)
        self.dest = dest
        self.ops = ops

    def flatten(self):
        ops = []

        for op in self.ops:
            if isinstance(op, ExpressionIR):
                ops.extend(op.flatten())

            else:
                ops.append(op)

        return ops

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        for op in self.ops:
            if op.dest == self.dest:
                op.dest = new_dest

        self.dest = new_dest

    def fold_constants(self):
        final_const = None

        for i in xrange(len(self.ops)):
            op = self.ops[i]
            if op.is_constant_op():
                # get constant
                const = op.fold()

                # first, throw this operation away, we don't need it
                self.ops[i] = None # we'll prune the Nones later

                # now we scan ahead, anything using this destination
                # should get replaced by the constant.
                # we'll actually just scan over everything, since
                # registers are not reused at this stage, we won't
                # step on anything.
                const_assigned = False

                for op2 in self.ops:
                    try:
                        if op2.left == op.dest:
                            op2.left = const
                            const_assigned = True

                        if op2.right == op.dest:
                            op2.right = const
                            const_assigned = True

                    except AttributeError:
                        pass

                if not const_assigned:
                    # if we get here, it means there weren't any
                    # places in the list of ops to assign this constant.
                    # this will happen if the expression has no variables
                    # in it.
                    # Ex. a = 1 + 2 + 3
                    # we'll remember this value for later
                    final_const = const

        # remove nones
        self.ops = [_op for _op in self.ops if _op != None]


        # check if all ops were reduced (everything folded)
        if len(self.ops) == 0:
            assert final_const

            self.ops.append(CopyIR(self.dest, final_const, level=self.level, line_no=self.line_no))


    def reduce_registers(self):
        for op in self.ops:
            try:
                # check if *not* a leaf node (at least one operand is a temp register)
                if not op.is_leaf():
                    # if left is a temp reg, use that for output
                    if isinstance(op.left, TempIR):

                        # the TempIR is stored as a reference.
                        # rather than replace the reference and then
                        # have to work through the tree to resolve
                        # all of them, we're just going to change
                        # the name within the TempIR, which will
                        # update everything that uses it.
                        op.dest.name = op.left.name

                    else:
                        op.dest.name = op.right.name

            # not all nodes in the expression will have an is_leaf()
            except AttributeError:
                pass

    def __str__(self):
        s = '%3d %s EXPR -> %s' % (self.line_no, self.indent * self.level, self.dest)

        for op in self.ops:
            s += '\n%s' % (op)

        return s

class NotIR(IntermediateNode):
    def __init__(self, dest, source, **kwargs):
        super(NotIR, self).__init__(**kwargs)

        self.dest = dest
        self.source = source

    def get_data_nodes(self):
        return [self.dest, self.source]

    def is_constant_op(self):
        return isinstance(self.source, ConstIR)

    def __str__(self):
        return '%3d %s NOT %s <- %s' % (self.line_no, self.indent * self.level, self.dest, self.source)


class BinopIR(IntermediateNode):
    def __init__(self, dest, op, left, right, **kwargs):
        super(BinopIR, self).__init__(**kwargs)
        self.dest = dest
        self.op = op
        self.left = left
        self.right = right

    def get_data_nodes(self):
        return [self.dest, self.left, self.right]

    def is_constant_op(self):
        return isinstance(self.left, ConstIR) and isinstance(self.right, ConstIR)

    def fold(self):
        if not self.is_constant_op():
            raise TypeError

        if self.op == 'eq':
            val = self.left.name == self.right.name

        elif self.op == 'neq':
            val = self.left.name != self.right.name

        elif self.op == 'gt':
            val = self.left.name > self.right.name

        elif self.op == 'gte':
            val = self.left.name >= self.right.name

        elif self.op == 'lt':
            val = self.left.name < self.right.name

        elif self.op == 'lte':
            val = self.left.name <= self.right.name

        elif self.op == 'logical_and':
            val = self.left.name and self.right.name

        elif self.op == 'logical_or':
            val = self.left.name or self.right.name

        elif self.op == 'add':
            val = self.left.name + self.right.name

        elif self.op == 'sub':
            val = self.left.name - self.right.name

        elif self.op == 'mult':
            val = self.left.name * self.right.name

        elif self.op == 'div':
            val = self.left.name / self.right.name

        elif self.op == 'mod':
            val = self.left.name % self.right.name

        # make sure we only emit integers
        val = int(val)

        return ConstIR(val, line_no=self.line_no)

    def is_leaf(self):
        if isinstance(self.left, TempIR) or isinstance(self.right, TempIR):
            return False

        return True

    def __str__(self):
        return '%3d %s BINOP %s = %s %s %s' % (self.line_no, self.indent * self.level, self.dest, self.left, self.op, self.right)


class ArrayOpIR(IntermediateNode):
    def __init__(self, dest, op, right, **kwargs):
        super(ArrayOpIR, self).__init__(**kwargs)
        self.dest = dest
        self.op = op
        self.right = right

    def get_data_nodes(self):
        return [self.dest, self.right]

    def __str__(self):
        return '%3d %s ARRAYOP %s %s= %s' % (self.line_no, self.indent * self.level, self.dest, self.op, self.right)

class CopyIR(IntermediateNode):
    def __init__(self, dest, src, **kwargs):
        super(CopyIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src

    def get_data_nodes(self):
        return [self.dest, self.src]

    def __str__(self):
        return '%3d %s COPY %s = %s' % (self.line_no, self.indent * self.level, self.dest, self.src)

class ObjectLoadIR(IntermediateNode):
    def __init__(self, dest, src, **kwargs):
        super(ObjectLoadIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        
    def get_data_nodes(self):
        return [self.dest, self.src]

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        self.dest = new_dest

    def __str__(self):
        return '%3d %s OBJ_LOAD %s -> %s' % (self.line_no, self.indent * self.level, self.src, self.dest)

class ObjectStoreIR(IntermediateNode):
    def __init__(self, dest, src, **kwargs):
        super(ObjectStoreIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        
    def get_data_nodes(self):
        return [self.dest, self.src]

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        self.dest = new_dest

    def __str__(self):
        return '%3d %s OBJ_STORE %s <- %s' % (self.line_no, self.indent * self.level, self.dest, self.src)

class ObjectLenIR(IntermediateNode):
    def __init__(self, dest, src, **kwargs):
        super(ObjectLenIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        
    def get_data_nodes(self):
        return [self.dest, self.src]

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        self.dest = new_dest

    def __str__(self):
        return '%3d %s OBJ_LEN %s -> %s' % (self.line_no, self.indent * self.level, self.src, self.dest)

class ArrayFuncIR(IntermediateNode):
    def __init__(self, dest, src, func, **kwargs):
        super(ArrayFuncIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        self.func = func
        
    def get_data_nodes(self):
        return [self.dest, self.src]

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        self.dest = new_dest

    def __str__(self):
        return '%3d %s ARRAYFUNC(%s) %s -> %s' % (self.line_no, self.indent * self.level, self.func, self.src, self.dest)


class IndexLoadIR(IntermediateNode):
    def __init__(self, dest, src, x, y=65535, **kwargs):
        super(IndexLoadIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        self.x = x
        self.y = y

    def get_data_nodes(self):
        nodes = [self.dest, self.src, self.x]

        if self.y != None:
            nodes.append(self.y)

        return nodes

    def is_constant_op(self):
        return False

    def replace_dest(self, new_dest):
        self.dest = new_dest

    def __str__(self):
        if self.y < 0:
            return '%3d %s INDEX_LOAD %s[%s] -> %s' % (self.line_no, self.indent * self.level, self.src, self.x, self.dest)

        else:
            return '%3d %s INDEX_LOAD %s[%s][%s] -> %s' % (self.line_no, self.indent * self.level, self.src, self.x, self.y, self.dest)

class IndexStoreIR(IntermediateNode):
    def __init__(self, dest, src, x, y=65535, **kwargs):
        super(IndexStoreIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        self.x = x
        self.y = y

    def get_data_nodes(self):
        nodes = [self.dest, self.src, self.x]

        if self.y != None:
            nodes.append(self.y)

        return nodes

    def is_constant_op(self):
        return False

    def __str__(self):
        if self.y < 0:
            return '%3d %s INDEX_STORE %s[%s] = %s' % (self.line_no, self.indent * self.level, self.dest, self.x, self.src)

        else:
            return '%3d %s INDEX_STORE %s[%s][%s] = %s' % (self.line_no, self.indent * self.level, self.dest, self.x, self.y, self.src)


class OffsetIR(IntermediateNode):
    def __init__(self, dest, index, length, stride, base=ConstIR(0), **kwargs):
        super(OffsetIR, self).__init__(**kwargs)

        self.dest = dest
        self.length = length
        self.stride = stride
        self.index = index
        self.base = base

    def get_data_nodes(self):
        nodes = [self.dest, self.index, self.base, self.length, self.stride]

        return nodes

    def is_constant_op(self):
        return False

    def __str__(self):
        return '%3d %s OFFSET %s = [%s + (%s %% %s) * %s]' % (self.line_no, self.indent * self.level, self.dest, self.base, self.index, self.length, self.stride)


class LabelIR(IntermediateNode):
    def __init__(self, name, **kwargs):
        super(LabelIR, self).__init__(**kwargs)
        self.name = name

    def __str__(self):
        return '    %s LABEL %s' % (self.indent * self.level, self.name)

class LoopIR(IntermediateNode):
    def __init__(self, top_label, end_label, continue_label, **kwargs):
        super(LoopIR, self).__init__(**kwargs)

        self.top_label = top_label
        self.end_label = end_label
        self.continue_label = continue_label

    def __str__(self):
        return '    %s LOOP %s -> %s (cont: %s)' % (self.indent * self.level, self.top_label, self.end_label, self.continue_label)

class BreakIR(IntermediateNode):
    def __init__(self, **kwargs):
        super(BreakIR, self).__init__(**kwargs)

    def __str__(self):
        return '    %s BREAK' % (self.indent * self.level)

class ContinueIR(IntermediateNode):
    def __init__(self, **kwargs):
        super(ContinueIR, self).__init__(**kwargs)

    def __str__(self):
        return '    %s CONTINUE' % (self.indent * self.level)

class JumpIR(IntermediateNode):
    def __init__(self, target, **kwargs):
        super(JumpIR, self).__init__(**kwargs)
        self.target = target

    def __str__(self):
        return '%3d %s JUMP -> %s' % (self.line_no, self.indent * self.level, self.target.name)

class JumpIfZeroIR(IntermediateNode):
    def __init__(self, src, target, **kwargs):
        super(JumpIfZeroIR, self).__init__(**kwargs)
        self.src = src
        self.target = target

    def get_data_nodes(self):
        return [self.src]

    def __str__(self):
        return '%3d %s JUMP IF ZERO %s -> %s' % (self.line_no, self.indent * self.level, self.src, self.target.name)

class JumpIfGteIR(IntermediateNode):
    def __init__(self, op1, op2, target, **kwargs):
        super(JumpIfGteIR, self).__init__(**kwargs)
        self.op1 = op1
        self.op2 = op2
        self.target = target

    def get_data_nodes(self):
        return [self.op1, self.op2]

    def __str__(self):
        return '%3d %s JUMP IF GTE %s >= %s -> %s' % (self.line_no, self.indent * self.level, self.op1, self.op2, self.target.name)

class JumpIfLessThanWithPreIncIR(IntermediateNode):
    def __init__(self, op1, op2, target, **kwargs):
        super(JumpIfLessThanWithPreIncIR, self).__init__(**kwargs)
        self.op1 = op1
        self.op2 = op2
        self.target = target

    def get_data_nodes(self):
        return [self.op1, self.op2]

    def __str__(self):
        return '%3d %s JUMP IF LESS THAN PRE INC ++%s < %s -> %s' % (self.line_no, self.indent * self.level, self.op1, self.op2, self.target.name)


class PrintIR(IntermediateNode):
    def __init__(self, target, **kwargs):
        super(PrintIR, self).__init__(**kwargs)
        self.target = target

    def get_data_nodes(self):
        return [self.target]

    def __str__(self):
        return '%3d %s PRINT %s' % (self.line_no, self.indent * self.level, self.target)

class AssertIR(IntermediateNode):
    def __init__(self, test, **kwargs):
        super(AssertIR, self).__init__(**kwargs)
        self.test = test

    def get_data_nodes(self):
        return [self.test]

    def __str__(self):
        return '%3d %s ASSERT %s != 0' % (self.line_no, self.indent * self.level, self.test)


class NopIR(IntermediateNode):
    def __init__(self, **kwargs):
        super(NopIR, self).__init__(**kwargs)

    def __str__(self):
        return '%3d %s NOP' % (self.line_no, self.indent * self.level)

class DefineArrayIR(IntermediateNode):
    def __init__(self, name, dimensions, data_type=None, **kwargs):
        super(DefineArrayIR, self).__init__(**kwargs)
        self.name = name
        self.dimensions = dimensions
        self.type = data_type

        self.publish = False

    def get_data_nodes(self):
        nodes = []

        ary = self

        while isinstance(ary, DefineArrayIR):
            nodes.append(ConstIR(ary.stride))
            nodes.append(ConstIR(ary.length))

            ary = ary.type
        
        return nodes

    @property
    def stride(self):
        return self.type.size()

    @property
    def length(self):
        c = 1
        for dim in self.dimensions:
            c *= dim

        return c

    def sub_length(self, depth):
        c = 1

        i = depth + 1

        while i < len(self.dimensions):
            c *= self.dimensions[i]

            i += 1

        return c


    @property
    def depth(self):
        return len(self.dimensions)
    
    def size(self):
        return self.length * self.type.size()

    def __str__(self):
        s = '%3d %s ARRAY %s[%d/%d]' % (self.line_no, self.indent * self.level, self.name, self.length, self.size())
        
        if self.publish:
            s += ' [publish]'
            
        return s

class DefineRecordIR(IntermediateNode):
    def __init__(self, name, record_type, **kwargs):
        super(DefineRecordIR, self).__init__(**kwargs)
        self.name = name
        self.record_type = record_type
        
    def __str__(self):
        s = '%3d %s RECORD(%s) %s' % (self.line_no, self.indent * self.level, self.record_type.name, self.name)
    
        return s

class PixelArrayIR(IntermediateNode):
    def __init__(self, name, index, length, size_x=65535, size_y=65535, reverse=False, addr=0, **kwargs):
        super(PixelArrayIR, self).__init__(**kwargs)
        self.name = name
        self.index = index
        self.length = length
        self.addr = addr
        self.size_x = size_x
        self.size_y = size_y
        self.reverse = reverse
        
    def __str__(self):
        s = '%3d %s PIXARRAY %s(%d) = %d:%d' % (self.line_no, self.indent * self.level, self.name, self.addr, self.index, self.length)

        return s

class LinkIR(IntermediateNode):
    def __init__(self, dest, src, query, send=True, **kwargs):
        super(LinkIR, self).__init__(**kwargs)
        self.dest = dest
        self.src = src
        self.query = query

        self.send = send
        
    def __str__(self):
        query = ''
        for q in self.query:
            query += '%s, ' % (q)

        if self.send:
            s = '%3d %s SEND %s -> %s @ [%s]' % (self.line_no, self.indent * self.level, self.src, self.dest, query)

        else:
            s = '%3d %s RECV %s <- %s @ [%s]' % (self.line_no, self.indent * self.level, self.dest, self.src, query)
    
        return s

class DBIR(IntermediateNode):
    def __init__(self, key, data_type, count=1, **kwargs):
        super(DBIR, self).__init__(**kwargs)
        self.key = key
        self.data_type = data_type
        self.count = count
        
    def __str__(self):

        s = '%3d %s DB %s (%s) len:%s' % (self.line_no, self.indent * self.level, self.key, self.data_type, self.count)
    
        return s


# generate intermediate representation
class CodeGeneratorPass2(object):
    def __init__(self, include_return=True):
        self.level = -1
        self.next_unique = -1
        self.next_label = 0

        self.next_pixel_array_addr = 0
        self.pixel_arrays = {'pixels': PixelArrayIR('pixels', 0, 65535)}
        self.objects = {'pixel_arrays': self.pixel_arrays}
        self.record_types = {}
        self.records = {}
        self.arrays = {}

        self.include_return = include_return

    def get_unique_register(self, line_no=0):
        self.next_unique += 1
        return TempIR('_r%d' % (self.next_unique), line_no=line_no)

    def get_label(self):
        self.next_label += 1
        return LabelIR('L%d' % (self.next_label))

    def generate(self, node, parent=None):
        try:
            self.level += 1

            if isinstance(node, ModuleNode):
                code = []

                for i in node.body:
                    ir = self.generate(i)

                    code.extend(ir)

                return {'code': [c for c in code if c != None and isinstance(c, IntermediateNode)],
                        'objects': self.objects,
                        'arrays': self.arrays,
                        'record_types': self.record_types,
                        'records': self.records}

            elif isinstance(node, FunctionNode):
                f = FunctionIR(node.name, level=self.level, line_no=node.line_no)
                ir = [f]

                params = []

                for i in node.params:
                    reg = self.generate(i)

                    params.append(reg.name)

                    ir.append(ParamIR(reg, level=self.level + 1, line_no=node.line_no))

                for i in node.body:
                    body = self.generate(i)

                    ir.extend(body)

                # check if last IR is a return, if not, add one
                if self.include_return:
                    if not isinstance(ir[-1], ReturnIR):
                        ir.append(ReturnIR(self.generate(ConstantNode(0)), level=self.level + 1, line_no=node.line_no))

                ir.append(EndFunctionIR(node.name, level=self.level, line_no=node.line_no))
                return ir

            elif isinstance(node, ReturnNode):
                code = []

                value = self.generate(node.value)

                # if the return value is code sequence, add to our list
                try:
                    code.extend(value)

                except TypeError:
                    pass

                try:
                    return_var = value[-1].dest

                except TypeError:
                    return_var = value

                code.append(ReturnIR(return_var, level=self.level, line_no=node.line_no))

                return code

            elif isinstance(node, CallNode):
                ir = []

                params = []
                for i in node.params:
                    param_code = self.generate(i)

                    try:
                        ir.extend(param_code)
                        param = param_code[-1].dest

                    except TypeError:
                        param = param_code

                    params.append(param)

                ir.append(CallIR(node.name, self.get_unique_register(line_no=node.line_no), params, level=self.level, line_no=node.line_no))

                return ir

            elif isinstance(node, SysCallNode):
                ir = []

                if node.name in KV_LINK_FUNCS:
                    if node.name == 'db':
                        item = DBIR(node.params[0], node.params[1], node.params[2], level=self.level, line_no=node.line_no)

                    elif node.name == 'send':
                        send = True
                        src = node.params[0]
                        dest = node.params[1]
                        query = node.params[2]

                        item = LinkIR(dest, src, query, send, level=self.level, line_no=node.line_no)

                    elif node.name == 'receive':
                        send = False
                        src = node.params[1]
                        dest = node.params[0]
                        query = node.params[2]

                        item = LinkIR(dest, src, query, send, level=self.level, line_no=node.line_no)

                    else:
                        assert False

                    
                    ir.append(item)

                else:
                    params = []
                    for i in node.params:
                        param_code = self.generate(i)

                        try:
                            ir.extend(param_code)
                            param = param_code[-1].dest

                        except TypeError:
                            param = param_code

                        params.append(param)

                    ir.append(SysCallIR(node.name, self.get_unique_register(line_no=node.line_no), params, level=self.level, line_no=node.line_no))

                return ir

            elif isinstance(node, IfNode):
                ir = []

                test = self.generate(node.test)

                try:
                    ir.extend(test)
                    dest = test[-1].dest

                except TypeError:
                    dest = test

                else_label = self.get_label()
                end_label = self.get_label()

                ir.append(JumpIfZeroIR(dest, else_label, level=self.level, line_no=node.line_no))

                for i in node.body:
                    ir.extend(self.generate(i))

                ir.append(JumpIR(end_label, level=self.level, line_no=node.line_no))

                ir.append(else_label)

                for i in node.orelse:
                    ir.extend(self.generate(i))

                ir.append(end_label)

                return ir

            elif isinstance(node, NotNode):
                ir = []

                value = self.generate(node.value, parent=node)
                if isinstance(value, list):
                    ir.extend(value)
                    dest = value[-1].dest

                else:
                    dest = value

                ir.append(NotIR(self.get_unique_register(line_no=node.line_no), dest, level=self.level, line_no=node.line_no))

                return ir

            elif isinstance(node, BinOpNode):
                ir = []

                left = self.generate(node.left, parent=node)
                right = self.generate(node.right, parent=node)

                if isinstance(left, list):
                    ir.extend(left)
                    left_dest = left[-1].dest

                else:
                    left_dest = left

                if isinstance(right, list):
                    ir.extend(right)
                    right_dest = right[-1].dest

                else:
                    right_dest = right

                ir.append(BinopIR(self.get_unique_register(line_no=node.line_no), node.op, left_dest, right_dest, level=self.level, line_no=node.line_no))

                # are we the top level node in this expression?
                if parent == None:
                    expr = ExpressionIR(ir[-1].dest, ir, level=self.level - 1, line_no=node.line_no)
                    expr.reduce_registers()
                    expr.fold_constants()

                    return [expr]

                else:
                    return ir

            elif isinstance(node, WhileLoopNode):
                ir = []

                top_label = self.get_label()
                end_label = self.get_label()

                ir.append(LoopIR(top_label, end_label, top_label))

                test = self.generate(node.test)

                ir.append(top_label)
                try:
                    ir.extend(test)    
                    test_dest = test[-1].dest

                except TypeError:
                    # test is a single variable, not an expression
                    test_dest = test
                
                ir.append(JumpIfZeroIR(test_dest, end_label, level=self.level, line_no=node.line_no)) 

                for i in node.body:
                    ir.extend(self.generate(i))

                ir.append(JumpIR(top_label, level=self.level, line_no=node.line_no))

                ir.append(end_label)

                return ir

            elif isinstance(node, ForLoopNode):
                ir = []

                top_label = self.get_label()
                continue_label = self.get_label()
                end_label = self.get_label()

                ir.append(LoopIR(top_label, end_label, continue_label))

                count_setup_code = self.generate(node.iterator)

                try:
                    ir.extend(count_setup_code)
                    count_reg = count_setup_code[-1].dest

                except TypeError:
                    count_reg = self.get_unique_register(line_no=node.line_no)

                    # check if iterating over an array
                    if isinstance(count_setup_code, ArrayVarIR):
                        # we actually want the array count, not its base address
                        count_setup_code = ConstIR(count_setup_code.length, line_no=node.line_no)

                    ir.append(CopyIR(count_reg, count_setup_code))

                target = self.generate(node.target)
                ir.append(DefineNumberIR(target.name, level=self.level, line_no=node.line_no))
                ir.append(CopyIR(target, ConstIR(0, line_no=node.line_no), level=self.level, line_no=node.line_no))

                ir.append(JumpIfGteIR(target, count_reg, end_label, level=self.level, line_no=node.line_no))

                ir.append(top_label)

                for i in node.body:
                    ir.extend(self.generate(i))

                ir.append(continue_label)
                ir.append(JumpIfLessThanWithPreIncIR(target, count_reg, top_label, level=self.level, line_no=node.line_no))

                ir.append(UndefineIR(target.name, level=self.level, line_no=node.line_no))
                ir.append(end_label)

                return ir

            elif isinstance(node, AugAssignNode):
                name = self.generate(node.name)

                if (isinstance(name, ObjIR) and (name.obj in self.pixel_arrays) and (name.attr in ARRAY_ATTRS)) or \
                   (isinstance(name, ArrayVarIR)):

                    array_binop = node.value

                    right_code = self.generate(array_binop.right)

                    try:
                        right = right_code[-1].dest

                        ir = right_code

                    except TypeError:
                        right = right_code
                        ir = []

                    ins = ArrayOpIR(name, array_binop.op, right, level=self.level, line_no=node.line_no)

                    ir.append(ins)

                    return ir

                else:                    
                    return self.generate(AssignNode(node.name, node.value, line_no=node.line_no))

            elif isinstance(node, AssignNode):
                name = self.generate(node.name)
                value = self.generate(node.value)

                code = []

                try:
                    code.extend(value)

                except TypeError:
                    pass

                # check if src or dest returned code items
                try:
                    src = value[-1].dest

                except (TypeError, AttributeError):
                    src = value

                try:
                    dest = name[-1]

                except TypeError:
                    dest = name

                # there's a special case for storing to an array,
                # so we intercept here and use the IndexStore instead of
                # Copy
                if isinstance(dest, IndexStoreIR):
                    dest.src = src

                    code.extend(name)
                    return code

                # check if previous codepath has a destination specified,
                # if so, we can skip the copy, as the VM instruction set
                # can usually directly target a destination without
                # an intermediate copy.
                # this only works on Vars, not objects
                elif isinstance(dest, VarIR):
                    try:
                        value[-1].replace_dest(dest)

                        return code

                    except (TypeError, AttributeError):
                        pass

                if isinstance(dest, ObjIR):
                    if (name.obj in self.pixel_arrays) and (name.attr in ARRAY_ATTRS):
                        code.append(ArrayOpIR(dest, 'eq', src, level=self.level, line_no=node.line_no))

                    else:
                        code.append(ObjectStoreIR(dest, src, level=self.level, line_no=node.line_no))

                elif isinstance(dest, ArrayVarIR):
                    code.append(ArrayOpIR(dest, 'eq', src, level=self.level, line_no=node.line_no))

                else:
                    code.append(CopyIR(dest, src, level=self.level, line_no=node.line_no))

                return code

            elif isinstance(node, ObjNode):
                code = []

                if node.obj in self.pixel_arrays:
                    obj = PixelObjIR(node.name, line_no=node.line_no)                    

                elif node.obj in self.records:
                    # look up record definition
                    record = self.records[node.obj]

                    index = ConstIR(record.translate_field(node.attr))

                    if node.store:
                        ir = IndexStoreIR(record, None, index, None, level=self.level, line_no=node.line_no)

                    else:
                        temp = self.get_unique_register(line_no=node.line_no)
                        ir = IndexLoadIR(temp, record, index, None, level=self.level, line_no=node.line_no)

                    return [ir]

                else:
                    obj = ObjIR(node.name, line_no=node.line_no)

                if node.store:
                    return obj

                else:
                    temp = self.get_unique_register(line_no=node.line_no)
                    code.append(ObjectLoadIR(temp, obj, level=self.level, line_no=node.line_no))

                return code                

            elif isinstance(node, ObjIndexNode):
                code = []

                if node.obj in self.pixel_arrays:
                    obj = PixelObjIR(node.name, line_no=node.line_no)                    

                else:
                    obj = ObjIR(node.name, line_no=node.line_no)

                x_code = self.generate(node.x)

                try:
                    x_dest = x_code[-1].dest
                    code.extend(x_code)

                except TypeError:
                    x_dest = x_code                

                y_code = self.generate(node.y)

                try:
                    y_dest = y_code[-1].dest
                    code.extend(y_code)

                except TypeError:
                    y_dest = y_code

                if node.store:
                    code.append(IndexStoreIR(obj, None, x_dest, y_dest, level=self.level, line_no=node.line_no))
                    return code

                else:
                    temp = self.get_unique_register(line_no=node.line_no)
                    code.append(IndexLoadIR(temp, obj, x_dest, y_dest, level=self.level, line_no=node.line_no))

                return code

            elif isinstance(node, BreakNode):
                return [BreakIR(level=self.level, line_no=node.line_no)]

            elif isinstance(node, ContinueNode):
                return [ContinueIR(level=self.level, line_no=node.line_no)]

            elif isinstance(node, PassNode):
                return [NopIR(level=self.level, line_no=node.line_no)]

            elif isinstance(node, PrintNode):
                target = self.generate(node.target)

                code = []

                try:
                    code.extend(target)
                    dest = target[-1].dest

                except TypeError:
                    dest = target

                code.append(PrintIR(dest, level=self.level, line_no=node.line_no))
                return code

            elif isinstance(node, AssertNode):
                test = self.generate(node.test)

                code = []

                try:
                    code.extend(test)
                    dest = test[-1].dest

                except TypeError:
                    dest = test

                code.append(AssertIR(dest, level=self.level, line_no=node.line_no))
                return code

            elif isinstance(node, NumberNode):
                define_ir = DefineNumberIR(node.name, number_type=node.type, level=self.level, line_no=node.line_no)

                define_ir.publish = node.publish

                return [define_ir]

            elif isinstance(node, VarNode):
                return VarIR(node.name, level=self.level, line_no=node.line_no)

            elif isinstance(node, ArrayVarNode):
                ir = ArrayVarIR(node.name, data_type=self.arrays[node.name].type, level=self.level, line_no=node.line_no)

                # update array length from declaration
                ir.length = self.arrays[ir.name].length    

                return ir

            elif isinstance(node, StringNode):
                return StringIR(node.name, level=self.level, line_no=node.line_no)

            elif isinstance(node, ConstantNode):
                return ConstIR(node.name, number_type=node.type, level=self.level, line_no=node.line_no)

            elif isinstance(node, ParameterNode):
                return node

            elif isinstance(node, PixelArrayNode):
                self.next_pixel_array_addr += 1
                ir = PixelArrayIR(node.name, node.index, node.length, size_x=node.size_x, size_y=node.size_y, reverse=node.reverse, addr=self.next_pixel_array_addr, level=self.level, line_no=node.line_no)

                self.pixel_arrays[ir.name] = ir

                return [ir]

            elif isinstance(node, ArrayDeclareNode):
                array_type = self.generate(node.type)
                try:
                    array_type = array_type[0]

                except TypeError:
                    pass

                define_array_ir = DefineArrayIR(node.name, node.dimensions, array_type, level=self.level, line_no=node.line_no)

                if node.publish:
                    define_array_ir.publish = node.publish

                self.arrays[define_array_ir.name] = define_array_ir

                return [define_array_ir]

            elif isinstance(node, RecordDeclareNode):
                self.record_types[node.name] = RecordVarIR(node.name, record_type=node)

                return []

            elif isinstance(node, RecordInstanceNode):
                self.records[node.name] = RecordVarIR(node.name, record_type=node.type)

                return [DefineRecordIR(node.name, node.type, level=self.level, line_no=node.line_no)]

            elif isinstance(node, ArrayIndexNode):
                code = []

                offset_dest = self.get_unique_register(line_no=node.line_no)

                array_type = self.generate(node.obj.type)[0]

                depth = 0

                base = ConstIR(0)
                for offset in node.offsets:
                    offset = self.generate(offset)

                    try:
                        code.extend(offset)
                        offset = offset[-1].dest

                    except TypeError:
                        pass

                    length = ConstIR(array_type.dimensions[depth], level=self.level, line_no=node.line_no)
                    stride = ConstIR(array_type.sub_length(depth), level=self.level, line_no=node.line_no)

                    ir = OffsetIR(offset_dest, offset, length, stride, base, level=self.level, line_no=node.line_no)

                    code.append(ir)

                    base = offset_dest

                    depth += 1

                if node.store:
                    dest_ary = self.generate(node.obj)

                    if array_type.depth != depth:
                        raise SyntaxNotSupported("Vector operations not supported on subarrays", line_no=node.line_no)

                    else:
                        ir = IndexStoreIR(dest_ary, None, offset_dest, None, level=self.level, line_no=node.line_no)

                else:
                    src_ary = self.generate(node.obj)

                    temp_dest = self.get_unique_register(line_no=node.line_no)
                    ir = IndexLoadIR(temp_dest, src_ary, offset_dest, None, level=self.level, line_no=node.line_no)

                code.append(ir)

                return code

            # elif isinstance(node, AttributeNode):
            #     return []

            # elif isinstance(node, SubscriptNode):
            #     code = []
                
            #     # print type(node.obj)

            #     obj = self.generate(node.obj)
            #     index = self.generate(node.index)

            #     print "MEOW"

            #     # print obj, index

            #     base = ConstIR(0)

            #     try:
            #         # obj.pop(-1)
            #         code.extend(obj)

            #         base = obj[-1].dest
            #         obj = obj[-1].target
            #         offset_dest = base

            #         print obj

            #     except (TypeError, AttributeError):
            #         offset_dest = self.get_unique_register(line_no=node.line_no)
                    

            #     ir = OffsetIR(offset_dest, obj, index, base, level=self.level, line_no=node.line_no)

            #     code.append(ir)

            #     # if isinstance(obj, DataIR):
            #     #     if node.store:
            #     #         ir = IndexStoreIR(obj, None, offset_dest, None, level=self.level, line_no=node.line_no)

            #     #     else:
            #     #         temp_dest = self.get_unique_register(line_no=node.line_no)
            #     #         ir = IndexLoadIR(temp_dest, obj, offset_dest, None, level=self.level, line_no=node.line_no)

            #     #     code.append(ir)

            #     for c in code:
            #         print c

            #     # print obj
            #     # print obj.type

            #     return code
                
            elif isinstance(node, basestring):
                return node

            elif isinstance(node, list):
                return node

            elif isinstance(node, LenNode):
                dest = self.get_unique_register(line_no=node.line_no)

                try:
                    obj = ObjIR(node.value.name, line_no=node.line_no)
                    ir = ObjectLenIR(dest, obj, level=self.level, line_no=node.line_no)

                except IndexError:
                    src = self.generate(node.value)
                    ir = ArrayFuncIR(dest, src, 'len', level=self.level, line_no=node.line_no)

                return [ir]

            elif isinstance(node, ArrayFuncNode):
                dest = self.get_unique_register(line_no=node.line_no)
                src = self.generate(node.op1)
                ir = ArrayFuncIR(dest, src, node.func, level=self.level, line_no=node.line_no)

                return [ir]

            elif isinstance(node, NoOpCodeNode):
                return []


            else:
                raise Exception(node)


        finally:
            self.level -= 1

# pull ops from expressionIR nodes, so
# the code stream is fully linearized.
# does a few other misc things too
class CodeGeneratorPass3(object):
    def __init__(self):
        pass

    def generate(self, state, include_loop=True):
        updated_code = []

        code = state['code']

        # zeroth pass, check for loop function
        loop_func = None
        for ir in code:
            if isinstance(ir, FunctionIR):
                if ir.name == 'loop':
                    loop_func = ir
                    break

        # no loop function
        if not loop_func and include_loop:
            # add an empty function
            code.append(FunctionIR('loop'))
            code.append(ReturnIR(ConstIR(0)))
            code.append(EndFunctionIR('loop'))

        # first pass:
        # flatten expression nodes
        for ir in code:
            if isinstance(ir, ExpressionIR):
                updated_code.extend(ir.flatten())

            else:
                updated_code.append(ir)

        # second pass, resolve jumps for breaks and continues
        loop_contexts = []
        i = 0
        for i in xrange(len(updated_code)):
            ir = updated_code[i]

            try:
                current_context = loop_contexts[-1]

            except IndexError:
                current_context = None

            if isinstance(ir, LoopIR):
                loop_contexts.append(ir)

            elif isinstance(ir, LabelIR):
                try:
                    if ir.name == current_context.end_label.name:
                        loop_contexts.pop()

                except AttributeError:
                    pass

            elif isinstance(ir, BreakIR):
                # replace break with jump to end of current loop
                ins = JumpIR(current_context.end_label, level=ir.level, line_no=ir.line_no)
                updated_code[i] = ins

            elif isinstance(ir, ContinueIR):
                # replace continue with jump to the continue label of current loop
                ins = JumpIR(current_context.continue_label, level=ir.level, line_no=ir.line_no)
                updated_code[i] = ins

            i += 1

        # remove the loop markers from code
        updated_code = [_ir for _ir in updated_code if not isinstance(_ir, LoopIR)]


        # third pass, helpers for library calls
        for ir in updated_code:
            if isinstance(ir, CallIR):
                # special handling for some functions
                # rand() takes variable arguments
                if ir.name == 'rand':
                    if len(ir.params) == 0:
                        ir.params.insert(0, ConstIR(0, line_no=ir.line_no))
                        ir.params.insert(1, ConstIR(65535, line_no=ir.line_no))

                    elif len(ir.params) == 1:
                        ir.params.insert(0, ConstIR(0, line_no=ir.line_no))

                    elif len(ir.params) > 2:
                        raise SyntaxNotSupported(message='too many parameters for function', line=ir.line_no)

                elif ir.name == 'halt':
                    assert len(ir.params) == 0

        # fourth pass
        # prefix all local vars with the function's name so
        # we can reuse local var names between functions and
        # guarantee they are unique.
        current_function = '_global'
        params = []

        for ir in updated_code:
            if isinstance(ir, FunctionIR):
                current_function = ir.name

            elif isinstance(ir, ParamIR):
                params.append(ir)

            elif isinstance(ir, EndFunctionIR):
                for param in params:
                    # skip strings, they are always global
                    if isinstance(param, StringIR):
                        continue

                    param.name.name = '%s.%s' % (current_function, param.name.name)

                params = []

            else:
                data_nodes = ir.get_data_nodes()

                for node in data_nodes:
                    # skip strings, they are always global
                    if isinstance(node, StringIR):
                        continue

                    try:
                        if node.name in [a.name.name for a in params]:
                            node.name = '%s.%s' % (current_function, node.name)

                    except AttributeError:
                        pass

        # fifth pass, remove useless index loads
        for i in xrange(len(updated_code)):
            ir = updated_code[i]

            if isinstance(ir, IndexLoadIR):
                if ir.x.name == 65535 and ir.y.name == 65535:
                    # updated_code[i] = None
                    assert False
        
        # remove nones
        updated_code = [_ir for _ir in updated_code if _ir != None]

        state['code'] = updated_code

        return state

# collect registers and data structures
class CodeGeneratorPass4(object):
    def __init__(self):
        self.current_function = '_global'

        self.optimize_register_usage = True

    def generate(self, state):
        register_usage = {}

        code = state['code']

        # first pass, collect registers and the instruction
        # address ranges they are used
        for i in xrange(len(code)):
            ir = code[i]
            for reg in ir.get_data_nodes():
                if not isinstance(reg, TempIR):
                    continue

                if reg.name not in register_usage:
                    # first instance of this register
                    # record start address
                    register_usage[reg.name] = [i, None]

                else:
                    # register already recorded, update
                    # last address
                    register_usage[reg.name][1] = i

        # second pass, assign addresses to registers
        registers = {}
        address_pool = []

        # assign return value at address 0
        ret_val = VarIR(RETURN_VAL_NAME, line_no=0)
        ret_val.addr = 0
        ret_val.function = self.current_function
        registers[ret_val.name] = ret_val

        addr = 1

        # # assign any global registers
        # # this is used for the automaton
        # for reg in global_registers:
        #     registers[reg] = VarIR(reg, number_type=reg.type, line_no=0)
        #     registers[reg].addr = addr
        #     registers[reg].function = self.current_function
        #     addr += 1


        # local_registers = {}

        for i in xrange(len(code)):
            ir = code[i]

            if isinstance(ir, FunctionIR):
                self.current_function = ir.name

                # on new function, reset address pool
                address_pool = []

            for reg in ir.get_data_nodes():
                # if not isinstance(reg, ConstIR):
                    # add to locals.
                    # automaton uses this.
                    # local_registers[reg.name] = reg

                # check type
                if not isinstance(reg, DataIR):
                    raise TypeError(reg, reg.line_no)

                if isinstance(reg, ObjIR):
                    if isinstance(reg, PixelObjIR):
                        # assign object address
                        reg.addr = state['objects']['pixel_arrays'][reg.obj].addr

                    continue

                # check if array
                if isinstance(reg, ArrayVarIR):

                    # assign address
                    reg.addr = registers[reg.name].addr

                    continue

                # only add if we haven't seen this register before
                if reg.name not in registers:
                    reg.function = self.current_function
                    registers[reg.name] = reg
                    reg.line_no = ir.line_no

                    # if temp reg, try and get address from pool
                    try:
                        # check if we have this optimization enabled
                        if not self.optimize_register_usage:
                            raise IndexError

                        if isinstance(reg, TempIR):
                            reg.addr = address_pool.pop()

                        else:
                            raise IndexError

                    except IndexError:
                        reg.addr = addr
                        addr += 1

                # register is already known, but we have a
                # separate instance of it.
                # we'll copy the function and address in,
                # this is easier than trying to replace the
                # actual instance.
                # because instances may differ, we can't change
                # one and expect the others to reflect the change,
                # even though they are technically the same register.
                # bottom line, this is the last place we can set addresses, etc.
                else:
                    reg.function = registers[reg.name].function
                    reg.addr = registers[reg.name].addr
                    reg.type = registers[reg.name].type

                    if not reg.line_no:
                        reg.line_no = ir.line_no

                    if isinstance(reg, TempIR):
                        # check if this is the last time this temp reg 
                        # is used.
                        if i == register_usage[reg.name][1]:
                            # add this address into the address pool
                            if i not in address_pool:
                                address_pool.append(reg.addr)

            if isinstance(ir, DefineNumberIR):
                registers[ir.name].declared = True

            elif isinstance(ir, DefineArrayIR):
                assert ir.name not in registers

                registers[ir.name] = ArrayVarIR(ir.name, line_no=ir.line_no)
                registers[ir.name].function = self.current_function
                registers[ir.name].addr = addr
                registers[ir.name].publish = ir.publish

                registers[ir.name].type = ir.type

                registers[ir.name].length = ir.length

                addr += registers[ir.name].size()

                # add array consts to register set
                for reg in ir.get_data_nodes():
                    if reg.name not in registers:
                        registers[reg.name] = reg
                        reg.addr = addr
                        addr += 1


            elif isinstance(ir, DefineRecordIR):
                assert ir.name not in registers

                registers[ir.name] = RecordVarIR(ir.name, record_type=ir.record_type, line_no=ir.line_no)
                registers[ir.name].function = self.current_function
                registers[ir.name].addr = addr
                    
                addr += registers[ir.name].size()
            


        data_table = {
            'registers': {},
            'read_keys': {},
            'write_keys': {},
            'publish': {},
            # 'local_registers': local_registers
        }

        for reg, data in registers.iteritems():
            if isinstance(data, StringIR):
                addr = data.addr
                # convert string to hash
                data = ConstIR(data.to_hash())
                data.addr = addr

                # mangle name to avoid conflicts
                reg = '__kv.' + reg

            if data.publish:
                data_table['publish'][reg] = data

            data_table['registers'][reg] = data

        state['data'] = data_table
        state['vm_data'] = {'registers': data_table['registers'], 'objects': state['objects']}
        return state


# class CodeGeneratorPassAutomaton4(object):
#     def __init__(self):
#         pass

#     def generate(self, state, condition=True):
#         updated_code = []

#         # code = state['code']

#         # assert isinstance(code[0], FunctionIR)
#         state['automaton_kv'] = {}

#         kv_regs = {}

#         pprint(state)

#         for reg in state['data']['registers'].itervalues():
#             if isinstance(reg, VarIR) and \
#                not reg.declared and \
#                reg.function != '_global':

#                 kv_regs[reg.name] = reg
#                 state['automaton_kv'][reg.name] = reg.addr

#         # for reg, varir in kv_regs.iteritems():
#         #     ins = CallIR('getkey', varir, [KeyIR(reg)])

#         #     print ins
#         #     code.insert(1, ins)


#         return state


class Instruction(object):
    mnemonic = 'NOP'
    opcode = None

    def __str__(self):
        return self.mnemonic

    def assemble(self):
        raise NotImplementedError(self.mnemonic)

    def len(self):
        return len(self.assemble())

# pseudo instruction - does not actually produce an opcode
class Label(Instruction):
    i = 0

    def __init__(self, name=None):
        if name == None:
            name = Label.i
            Label.i += 1

        self.name = name

    def __str__(self):
        return "Label(%s)" % (self.name)

# pseudo instruction - does not actually produce an opcode
class Function(Instruction):
    i = 0

    def __init__(self, name):
        self.name = name
        
    def __str__(self):    
        return "Function(%s)" % (self.name)

# pseudo instruction - does not actually produce an opcode
class Addr(Instruction):
    def __init__(self, addr=None):
        self.addr = addr

    def __str__(self):
        return "Addr(%s)" % (self.addr)

# pseudo instruction - does not actually produce an opcode
class Param(Instruction):
    def __init__(self, param=None):
        self.param = param

    def __str__(self):
        return "Param(%s)" % (self.param)


class Mov(Instruction):
    mnemonic = 'MOV'
    opcode = 0x00

    def __init__(self, dest, src):
        self.dest = dest
        self.src = src

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.src)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.src.addr]

class Clr(Instruction):
    mnemonic = 'CLR'
    opcode = 0x01

    def __init__(self, dest):
        self.dest = dest

    def __str__(self):
        return "%s %s <- 0" % (self.mnemonic, self.dest)

    def assemble(self):
        return [self.opcode, self.dest.addr]

class BinInstruction(Instruction):
    def __init__(self, result, op1, op2):
        super(BinInstruction, self).__init__()
        self.result = result
        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%-16s %16s <- %16s %4s %16s" % (self.mnemonic, self.result, self.op1, self.symbol, self.op2)

    def assemble(self):
        return [self.opcode, self.result.addr, self.op1.addr, self.op2.addr]

class CompareEq(BinInstruction):
    mnemonic = 'COMP_EQ'
    symbol = "=="
    opcode = 0x02

class CompareNeq(BinInstruction):
    mnemonic = 'COMP_NEQ'
    symbol = "!="
    opcode = 0x03

class CompareGt(BinInstruction):
    mnemonic = 'COMP_GT'
    symbol = ">"
    opcode = 0x04

class CompareGtE(BinInstruction):
    mnemonic = 'COMP_GTE'
    symbol = ">="
    opcode = 0x05

class CompareLt(BinInstruction):
    mnemonic = 'COMP_LT'
    symbol = "<"
    opcode = 0x06

class CompareLtE(BinInstruction):
    mnemonic = 'COMP_LTE'
    symbol = "<="
    opcode = 0x07

class And(BinInstruction):
    mnemonic = 'AND'
    symbol = "AND"
    opcode = 0x08

class Or(BinInstruction):
    mnemonic = 'OR'
    symbol = "OR"
    opcode = 0x09

class Add(BinInstruction):
    mnemonic = 'ADD'
    symbol = "+"
    opcode = 0x0A

class Sub(BinInstruction):
    mnemonic = 'SUB'
    symbol = "-"
    opcode = 0x0B

class Mul(BinInstruction):
    mnemonic = 'MUL'
    symbol = "*"
    opcode = 0x0C

class Div(BinInstruction):
    mnemonic = 'DIV'
    symbol = "/"
    opcode = 0x0D

class Mod(BinInstruction):
    mnemonic = 'MOD'
    symbol = "%"
    opcode = 0x0E


class BaseJmp(Instruction):
    mnemonic = 'JMP'

    def __init__(self, label):
        super(BaseJmp, self).__init__()

        self.label = label

    def __str__(self):
        return "%s -> %s" % (self.mnemonic, self.label)

    def assemble(self):
        return [self.opcode, ('label', self.label.name), 0]

class Jmp(BaseJmp):
    opcode = 0x0F

class JmpConditional(BaseJmp):
    def __init__(self, op1, label):
        super(JmpConditional, self).__init__(label)

        self.op1 = op1

    def __str__(self):
        return "%s, %s -> %s" % (self.mnemonic, self.op1, self.label)

    def assemble(self):
        return [self.opcode, self.op1.addr, ('label', self.label.name), 0]


class JmpIfZero(JmpConditional):
    opcode = 0x10
    mnemonic = 'JMP_IF_Z'

class JmpNotZero(JmpConditional):
    opcode = 0x11
    mnemonic = 'JMP_IF_NOT_Z'

class JmpIfZeroPostDec(JmpConditional):
    opcode = 0x12
    mnemonic = 'JMP_IF_Z_DEC'

class JmpIfGte(BaseJmp):
    opcode = 0x13
    mnemonic = 'JMP_IF_GTE'

    def __init__(self, op1, op2, label):
        super(JmpIfGte, self).__init__(label)

        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%s, %s >= %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)

    def assemble(self):
        return [self.opcode, self.op1.addr, self.op2.addr, ('label', self.label.name), 0]


class JmpIfLessThanPreInc(BaseJmp):
    opcode = 0x14
    mnemonic = 'JMP_IF_LESS_PRE_INC'

    def __init__(self, op1, op2, label):
        super(JmpIfLessThanPreInc, self).__init__(label)

        self.op1 = op1
        self.op2 = op2

    def __str__(self):
        return "%s, ++%s < %s -> %s" % (self.mnemonic, self.op1, self.op2, self.label)

    def assemble(self):
        return [self.opcode, self.op1.addr, self.op2.addr, ('label', self.label.name), 0]

class Print(Instruction):
    opcode = 0x15
    mnemonic = 'PRINT'

    def __init__(self, op1):
        super(Print, self).__init__()
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def assemble(self):
        return [self.opcode, self.op1.addr]


class Return(Instruction):
    opcode = 0x16
    mnemonic = 'RET'

    def __init__(self, op1):
        super(Return, self).__init__()
        self.op1 = op1

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.op1)

    def assemble(self):
        return [self.opcode, self.op1.addr]

class Call(Instruction):
    opcode = 0x17
    mnemonic = 'CALL'

    def __init__(self, target):
        super(Call, self).__init__()
        self.target = target

    def __str__(self):
        return "%s %s" % (self.mnemonic, self.target)

    def assemble(self):
        return [self.opcode, ('addr', self.target), 0]

class IndexLoad(Instruction):
    mnemonic = 'IDX_LOAD'
    opcode = 0x18

    def __init__(self, dest, src, index):
        self.dest = dest
        self.src = src

        self.index = index

    def __str__(self):
        return "%s %s <- %s[%s]" % (self.mnemonic, self.dest.name, self.src, self.index)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.src.addr, self.index.addr]


class IndexStore(Instruction):
    mnemonic = 'IDX_STORE'
    opcode = 0x19

    def __init__(self, dest, src, index):
        self.dest = dest
        self.src = src

        self.index = index

    def __str__(self):
        return "%s %s[%s] <- %s" % (self.mnemonic, self.dest.name, self.index, self.src)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.src.addr, self.index.addr]


class OffsetArray(Instruction):
    mnemonic = 'OFFSET_ARRAY'
    opcode = 0x1A

    def __init__(self, dest, base, index, ary_length, ary_stride):
        self.dest = dest
        self.base = base

        self.index = index

        self.ary_length = ary_length
        self.ary_stride = ary_stride

    def __str__(self):
        return "%s %s <- *[%s + %s] (%d/%d)" % (self.mnemonic, self.dest.name, self.base, self.index, self.ary_stride.name, self.ary_length.name)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.base.addr, self.index.addr, self.ary_length.addr, self.ary_stride.addr]


class ArrayFuncIns(Instruction):
    mnemonic = 'ARRAY_FUNC'
    opcode = 0x1B

    def __init__(self, dest, array, func):
        self.dest = dest
        self.array = array

        self.func = func
        self.func_id = ARRAY_FUNCS[func]

        self.ary_length = ConstIR(array.length)
        self.ary_stride = ConstIR(array.stride)

    def __str__(self):
        return "%s %s <- %s(%s)" % (self.mnemonic, self.dest.name, self.func, self.array.name)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.array.addr, self.func_id, self.ary_length, self.ary_stride]


class LoadToPixArray(Instruction):
    def __init__(self, src, index_x, index_y=0, obj=None):
        self.src = src
        self.obj = obj
        self.index_x = index_x
        self.index_y = index_y

    def __str__(self):
        return "%s %s[%s][%s] <- %s" % (self.mnemonic, self.obj.obj, self.index_x, self.index_y, self.src)

    def assemble(self):
        return [self.opcode, self.src.addr, self.index_x.addr, self.index_y.addr, self.obj.addr]

class LoadToArrayHue(LoadToPixArray):
    mnemonic = 'LTAH'
    opcode = 0x1C
    name = 'hue'

class LoadToArraySat(LoadToPixArray):
    mnemonic = 'LTAS'
    opcode = 0x1D
    name = 'sat'

class LoadToArrayVal(LoadToPixArray):
    mnemonic = 'LTAV'
    opcode = 0x1E
    name = 'val'

class LoadToArrayHSFade(LoadToPixArray):
    mnemonic = 'LTAHSF'
    opcode = 0x33
    name = 'hs_fade'

class LoadToArrayVFade(LoadToPixArray):
    mnemonic = 'LTAVF'
    opcode = 0x34
    name = 'v_fade'

class LoadFromPixArray(Instruction):
    def __init__(self, dest, index_x, index_y=0, obj=None):
        self.dest = dest
        self.obj = obj
        self.index_x = index_x
        self.index_y = index_y

    def __str__(self):
        return "%s %s <- %s.%s[%s][%s]" % (self.mnemonic, self.dest, self.obj.obj, self.name, self.index_x, self.index_y)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.index_x.addr, self.index_y.addr, self.obj.addr]


class LoadFromArrayHue(LoadFromPixArray):
    mnemonic = 'LFAH'
    opcode = 0x1F
    name = 'hue'

class LoadFromArraySat(LoadFromPixArray):
    mnemonic = 'LFAS'
    opcode = 0x20
    name = 'sat'

class LoadFromArrayVal(LoadFromPixArray):
    mnemonic = 'LFAV'
    opcode = 0x21
    name = 'val'

class LoadFromArrayHSFade(LoadFromPixArray):
    mnemonic = 'LFAHSF'
    opcode = 0x31
    name = 'hs_fade'

class LoadFromArrayVFade(LoadFromPixArray):
    mnemonic = 'LFAVF'
    opcode = 0x32
    name = 'v_fade'



class ArrayOpInstruction(Instruction):
    def __init__(self, result, op1):
        super(ArrayOpInstruction, self).__init__()
        self.result = result
        self.op1 = op1

    def __str__(self):
        return "%-16s %16s %1s= %16s" % (self.mnemonic, self.result, self.symbol, self.op1)

    def assemble(self):
        obj_type = 0
        attr = 0
        ary_stride = ConstIR(0)
        ary_length = ConstIR(0)

        if isinstance(self.result, PixelObjIR):
            obj_type = PIX_OBJ_TYPE
            attr = PIX_ATTRS[self.result.attr]

        elif isinstance(self.result, ArrayVarIR):
            obj_type = ARRAY_OBJ_TYPE
            attr = 0
            ary_stride = ConstIR(self.result.stride)
            ary_length = ConstIR(self.result.length)

        # Array op format is:
        # opcode - object type - object address - attribute address - operand
        return [self.opcode, obj_type, self.result.addr, ary_length, ary_stride, attr, self.op1.addr]

class ArrayAdd(ArrayOpInstruction):
    mnemonic = 'ARRAY_ADD'
    symbol = "+"
    opcode = 0x22

class ArraySub(ArrayOpInstruction):
    mnemonic = 'ARRAY_SUB'
    symbol = "-"
    opcode = 0x23

class ArrayMul(ArrayOpInstruction):
    mnemonic = 'ARRAY_MUL'
    symbol = "*"
    opcode = 0x24

class ArrayDiv(ArrayOpInstruction):
    mnemonic = 'ARRAY_DIV'
    symbol = "/"
    opcode = 0x25

class ArrayMod(ArrayOpInstruction):
    mnemonic = 'ARRAY_MOD'
    symbol = "%"
    opcode = 0x26

class ArrayMov(ArrayOpInstruction):
    mnemonic = 'ARRAY_MOV'
    symbol = "="
    opcode = 0x27

class Rand(Instruction):
    mnemonic = 'RAND'
    opcode = 0x28

    def __init__(self, dest, start=0, end=65535):
        self.dest = dest
        self.start = start
        self.end = end

    def __str__(self):
        return "%s %s <- rand(%s, %s)" % (self.mnemonic, self.dest, self.start, self.end)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.start.addr, self.end.addr]

class Assert(Instruction):
    mnemonic = 'ASSERT'
    opcode = 0x29

    def __init__(self, op1):
        self.op1 = op1

    def __str__(self):
        return "%s %s == TRUE" % (self.mnemonic, self.op1)

    def assemble(self):
        return [self.opcode, self.op1.addr]

class Halt(Instruction):
    mnemonic = 'HALT'
    opcode = 0x2A

    def __init__(self):
        pass

    def __str__(self):
        return "%s" % (self.mnemonic)

    def assemble(self):
        return [self.opcode]


class IsFading(Instruction):
    mnemonic = 'IS_FADING'
    opcode = 0x2B

    def __init__(self, dest, index_x=-1, index_y=-1, obj=None):
        self.dest = dest
        self.obj = obj
        self.index_x = index_x
        self.index_y = index_y

    def __str__(self):
        return "%s %s <- %s.is_fading[%s][%s]" % (self.mnemonic, self.dest, self.obj.obj, self.index_x, self.index_y)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.index_x.addr, self.index_y.addr, self.obj.addr]

class LibCall(Instruction):
    opcode = 0x2C
    mnemonic = 'LCALL'

    def __init__(self, target, dest=None, params=[]):
        super(LibCall, self).__init__()
        self.target = target
        self.dest   = dest
        self.params = params

    def __str__(self):
        return "%s %s -> %s" % (self.mnemonic, self.target, self.dest)

    def assemble(self):
        target_hash = catbus_string_hash(self.target)
        data = [self.opcode, 
                (target_hash >> 24) & 0xff, 
                (target_hash >> 16) & 0xff, 
                (target_hash >> 8) & 0xff, 
                (target_hash >> 0) & 0xff,
                self.dest.addr,
                len(self.params)]

        encoded_params = [a.addr for a in self.params]

        data.extend(encoded_params)
        return data

class SysCall(Instruction):
    opcode = 0x2D
    mnemonic = 'SYSCALL'

    def __init__(self, func, dest=None, params=[]):
        super(SysCall, self).__init__()
        self.func = func
        self.func_id = SYS_CALLS[func]
        self.dest   = dest
        self.params = params

    def __str__(self):
        return "%s %s -> %s" % (self.mnemonic, self.func, self.dest)

    def assemble(self):
        
        data = [self.opcode, 
                self.func_id, 
                self.dest.addr,
                len(self.params)]

        encoded_params = [a.addr for a in self.params]

        data.extend(encoded_params)
        return data


class ObjectLoadInstruction(Instruction):
    mnemonic = 'OBJ_LOAD'
    opcode = 0x36

    def __init__(self, result, op1):
        super(ObjectLoadInstruction, self).__init__()
        self.result = result
        self.op1 = op1

    def __str__(self):
        return "%-16s %16s <- %16s" % (self.mnemonic, self.result, self.op1)

    def assemble(self):
        obj_type = 0
        attr = 0

        if isinstance(self.op1, PixelObjIR):
            obj_type = PIX_OBJ_TYPE
            attr = PIX_ATTRS[self.op1.attr]

        else:
            raise ObjectNotDefined(self.op1)

        return [self.opcode, obj_type, attr, self.op1.addr, self.result.addr]


class ObjectStoreInstruction(Instruction):
    mnemonic = 'OBJ_STORE'
    opcode = 0x37

    def __init__(self, result, op1):
        super(ObjectStoreInstruction, self).__init__()
        self.result = result
        self.op1 = op1

    def __str__(self):
        return "%-16s %16s <- %16s" % (self.mnemonic, self.result, self.op1)

    def assemble(self):
        obj_type = 0
        attr = 0

        if isinstance(self.result, PixelObjIR):
            obj_type = PIX_OBJ_TYPE
            attr = PIX_ATTRS[self.result.attr]

        else:
            raise ObjectNotDefined(self.result)

        return [self.opcode, obj_type, attr, self.op1.addr, self.result.addr]

class Not(Instruction):
    mnemonic = 'NOT'
    opcode = 0x38

    def __init__(self, dest, source):
        self.dest = dest
        self.source = source

    def __str__(self):
        return "%s %s <- %s" % (self.mnemonic, self.dest, self.source)

    def assemble(self):
        return [self.opcode, self.dest.addr, self.source.addr]


class DBLoadInstruction(Instruction):
    mnemonic = 'DB_LOAD'
    opcode = 0x39

    def __init__(self, result, op1):
        super(DBLoadInstruction, self).__init__()
        self.result = result
        self.op1 = op1

    def __str__(self):
        return "%-16s %16s <- %16s" % (self.mnemonic, self.result, self.op1)

    def assemble(self):
        kv_hash = string_hash_func(self.op1.attr)

        return [self.opcode, (kv_hash >> 24) & 0xff, (kv_hash >> 16) & 0xff, (kv_hash >> 8) & 0xff, (kv_hash >> 0) & 0xff, self.result.addr]


class DBStoreInstruction(Instruction):
    mnemonic = 'DB_STORE'
    opcode = 0x3A

    def __init__(self, result, op1):
        super(DBStoreInstruction, self).__init__()
        self.result = result
        self.op1 = op1

    def __str__(self):
        return "%-16s %16s <- %16s" % (self.mnemonic, self.result, self.op1)

    def assemble(self):
        kv_hash = string_hash_func(self.result.attr)

        return [self.opcode, (kv_hash >> 24) & 0xff, (kv_hash >> 16) & 0xff, (kv_hash >> 8) & 0xff, (kv_hash >> 0) & 0xff, self.op1.addr]


class DBIndexLoadInstruction(Instruction):
    mnemonic = 'DB_IDX_LOAD'
    opcode = 0x3B

    def __init__(self, result, op1, index):
        super(DBIndexLoadInstruction, self).__init__()
        self.result = result
        self.op1 = op1
        self.index = index

    def __str__(self):
        return "%-16s %16s[%s] <- %16s" % (self.mnemonic, self.result, self.index, self.op1)

    def assemble(self):
        kv_hash = string_hash_func(self.op1.attr)

        return [self.opcode, (kv_hash >> 24) & 0xff, (kv_hash >> 16) & 0xff, (kv_hash >> 8) & 0xff, (kv_hash >> 0) & 0xff, self.result.addr, self.index.addr]


class DBIndexStoreInstruction(Instruction):
    mnemonic = 'DB_IDX_STORE'
    opcode = 0x3C

    def __init__(self, result, op1, index):
        super(DBIndexStoreInstruction, self).__init__()
        self.result = result
        self.op1 = op1
        self.index = index

    def __str__(self):
        return "%-16s %16s[%s] <- %16s" % (self.mnemonic, self.result, self.index, self.op1)

    def assemble(self):
        kv_hash = string_hash_func(self.result.attr)

        return [self.opcode, (kv_hash >> 24) & 0xff, (kv_hash >> 16) & 0xff, (kv_hash >> 8) & 0xff, (kv_hash >> 0) & 0xff, self.op1.addr, self.index.addr]

class DBLenInstruction(Instruction):
    mnemonic = 'DB_LEN'
    opcode = 0x3D

    def __init__(self, result, op1):
        super(DBLenInstruction, self).__init__()
        self.result = result
        self.op1 = op1

    def __str__(self):
        return "%-16s %16s <- %16s" % (self.mnemonic, self.result, self.op1)

    def assemble(self):
        kv_hash = string_hash_func(self.op1.attr)

        return [self.opcode, (kv_hash >> 24) & 0xff, (kv_hash >> 16) & 0xff, (kv_hash >> 8) & 0xff, (kv_hash >> 0) & 0xff, self.result.addr]



conditional_jumps = [
    JmpIfZero,
]

bin_ops = [
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    CompareEq,
    CompareNeq,
    CompareGt,
    CompareGtE,
    CompareLt,
    CompareLtE,
    And,
    Or,
]

array_ops = [
    ArrayMov,
    ArrayAdd,
    ArraySub,
    ArrayMul,
    ArrayDiv,
    ArrayMod,
]

class CodeGeneratorPass5(object):
    def __init__(self, state):
        self.code = {}

        self.current_function = ''

        registers = state['data']['registers']
        self.read_keys = state['data']['read_keys']
        self.write_keys = state['data']['write_keys']

        self.registers = {}
        for reg, data in registers.iteritems():
            self.registers[reg] = data

        self.arrays = state['arrays']
        self.records = state['records']

        self.functions = {}

        self.ret_val = self.registers[RETURN_VAL_NAME]

    def append_code(self, ins):
        self.code[self.current_function].append(ins)

    def generate(self, state, result=None, context=[], from_array=False):
        ir_code = state['code']
        self.code = {}
        state['links'] = []
        state['db'] = []

        # first loop, get functions
        for ir in ir_code:
            if isinstance(ir, FunctionIR):
                self.code[ir.name] = []
                self.current_function = ir.name
                self.functions[ir.name] = []

            # set up function parameters
            elif isinstance(ir, ParamIR):
                self.functions[self.current_function].append(ir.name)

        # second loop, get everything else
        for ir in ir_code:
            if isinstance(ir, DefineNumberIR):
                pass

            elif isinstance(ir, UndefineIR):
                pass

            elif isinstance(ir, FunctionIR):
                self.current_function = ir.name
                ins = Function(ir.name)

                self.append_code(ins)

            elif isinstance(ir, SysCallIR):
                assert ir.name in SYS_CALLS

                ins = SysCall(ir.name, ir.dest, ir.params)

                self.append_code(ins)


            elif isinstance(ir, CallIR):
                if ir.name == 'halt':
                    ins = Halt()
                    self.append_code(ins)

                elif ir.name == 'rand':
                    ins = Rand(ir.dest, ir.params[0], ir.params[1])
                    self.append_code(ins)


                else:
                    # check if call is for a function defined in the script
                    try:
                        # check parameter length
                        param_len = len(self.functions[ir.name])
                        if len(ir.params) != param_len:
                            raise IncorrectNumberOfParameters('%s line: %d' % (ir.name, ir.line_no))

                        # move parameters
                        for i in xrange(param_len):
                            self.append_code(Mov(self.functions[ir.name][i], ir.params[i]))

                        ins = Call(ir.name)
                        self.append_code(ins)
                        self.append_code(Mov(ir.dest, self.ret_val))

                    except KeyError:
                        # function not found in script, so we'll make this a library call
                        ins = LibCall(ir.name, ir.dest, ir.params)

                        self.append_code(ins)


            elif isinstance(ir, ParamIR):
                pass

            elif isinstance(ir, ReturnIR):
                ins = Return(ir.name)
                self.append_code(ins)

            elif isinstance(ir, ExpressionIR):
                raise ValueError(ir)

            elif isinstance(ir, NotIR):
                ins = Not(ir.dest, ir.source)

                self.append_code(ins)

            elif isinstance(ir, BinopIR):
                ops = {
                    'eq': CompareEq,
                    'neq': CompareNeq,
                    'gt': CompareGt,
                    'gte': CompareGtE,
                    'lt': CompareLt,
                    'lte': CompareLtE,
                    'logical_and': And,
                    'logical_or': Or,
                    'add': Add,
                    'sub': Sub,
                    'mult': Mul,
                    'div': Div,
                    'mod': Mod,
                }

                assert not isinstance(ir.dest, ObjIR)
                    
                ins = ops[ir.op](ir.dest, ir.left, ir.right)

                self.append_code(ins)

            elif isinstance(ir, ArrayOpIR):
                ary_ops = {
                    'add': ArrayAdd,
                    'sub': ArraySub,
                    'mult': ArrayMul,
                    'div': ArrayDiv,
                    'mod': ArrayMod,
                    'eq': ArrayMov,
                }   
                
                ins = ary_ops[ir.op](ir.dest, ir.right)

                self.append_code(ins)

            elif isinstance(ir, CopyIR):
                self.append_code(Mov(ir.dest, ir.src))
            
            elif isinstance(ir, ObjectLoadIR):
                if ir.src.attr == 'is_fading':
                    Const65535 = state['data']['registers'][65535]
                    ins = IsFading(ir.dest, index_x=Const65535, index_y=Const65535, obj=ir.src)

                elif ir.src.obj == 'db':
                    ins = DBLoadInstruction(ir.dest, ir.src)

                    # add to read keys
                    self.read_keys[ins.op1.attr] = string_hash_func(ins.op1.attr)

                # elif ir.dest.obj in self.records:
                    # raise NotImplementedError

                else:
                    ins = ObjectLoadInstruction(ir.dest, ir.src)

                self.append_code(ins)

            elif isinstance(ir, ObjectStoreIR):
                if ir.dest.obj == 'db':
                    ins = DBStoreInstruction(ir.dest, ir.src)

                    # add to write keys
                    self.write_keys[ins.result.attr] = string_hash_func(ins.result.attr)

                # elif ir.dest.obj in self.records:
                #     # look up record
                #     record = self.registers[ir.dest.obj]

                #     register_addr = record.addr + record.translate_field(ir.dest.attr)

                #     print type(record.fields[ir.dest.attr])
                    

                    # raise NotImplementedError

                else:
                    ins = ObjectStoreInstruction(ir.dest, ir.src)

                self.append_code(ins)

            elif isinstance(ir, ObjectLenIR):
                if ir.src.obj == 'db':
                    ins = DBLenInstruction(ir.dest, ir.src)

                    # add to read keys
                    self.read_keys[ins.op1.attr] = string_hash_func(ins.op1.attr)
                
                self.append_code(ins)

            elif isinstance(ir, IndexLoadIR):
                index_load_irs = {
                    'hue': LoadFromArrayHue,
                    'sat': LoadFromArraySat,
                    'val': LoadFromArrayVal,
                    'hs_fade': LoadFromArrayHSFade,
                    'v_fade': LoadFromArrayVFade,
                    'is_fading': IsFading,
                }

                try:
                    ins = index_load_irs[ir.src.attr](ir.dest, ir.x, ir.y, obj=ir.src)

                except KeyError:
                    if ir.src.obj == 'db':
                        ins = DBIndexLoadInstruction(ir.dest, ir.src, ir.x)

                        # add to read keys
                        self.read_keys[ins.op1.attr] = string_hash_func(ins.op1.attr)

                    else:
                        raise SyntaxNotSupported(line_no=ir.line_no)

                except AttributeError:
                    assert ir.y < 0

                    # array indexing lands here
                    ins = IndexLoad(ir.dest, ir.src, ir.x)

                self.append_code(ins)

            elif isinstance(ir, IndexStoreIR):
                index_store_irs = {
                    'hue': LoadToArrayHue,
                    'sat': LoadToArraySat,
                    'val': LoadToArrayVal,
                    'hs_fade': LoadToArrayHSFade,
                    'v_fade': LoadToArrayVFade,
                }

                try:
                    ins = index_store_irs[ir.dest.attr](ir.src, ir.x, ir.y, obj=ir.dest)

                except KeyError:
                    if ir.dest.obj == 'db':
                        ins = DBIndexStoreInstruction(ir.dest, ir.src, ir.x)

                        # add to write keys
                        self.write_keys[ins.result.attr] = string_hash_func(ins.result.attr)

                    elif ir.dest.obj in self.records:
                        raise NotImplementedError

                    else:
                        raise SyntaxNotSupported(line_no=ir.line_no)
                
                except AttributeError:
                    assert ir.y < 0

                    # array indexing lands here
                    ins = IndexStore(ir.dest, ir.src, ir.x)

                self.append_code(ins)

            elif isinstance(ir, LabelIR):
                ins = Label(ir.name)
                self.append_code(ins)

            elif isinstance(ir, JumpIR):
                ins = Jmp(ir.target)
                self.append_code(ins)

            elif isinstance(ir, JumpIfZeroIR):
                ins = JmpIfZero(ir.src, ir.target)
                self.append_code(ins)

            elif isinstance(ir, JumpIfGteIR):
                ins = JmpIfGte(ir.op1, ir.op2, ir.target)
                self.append_code(ins)

            elif isinstance(ir, JumpIfLessThanWithPreIncIR):
                ins = JmpIfLessThanPreInc(ir.op1, ir.op2, ir.target)
                self.append_code(ins)

            elif isinstance(ir, PrintIR):
                ins = Print(ir.target)
                self.append_code(ins)

            elif isinstance(ir, AssertIR):
                ins = Assert(ir.test)
                self.append_code(ins)

            elif isinstance(ir, NopIR):
                pass

            elif isinstance(ir, EndFunctionIR):
                pass

            elif isinstance(ir, PixelArrayIR):
                pass

            elif isinstance(ir, DefineArrayIR):
                pass

            elif isinstance(ir, DefineRecordIR):
                pass

            elif isinstance(ir, ArrayFuncIR):
                ins = ArrayFuncIns(ir.dest, ir.src, ir.func)
                self.append_code(ins)

            elif isinstance(ir, OffsetIR):
                ary_length = self.registers[ir.length.name]
                ary_stride = self.registers[ir.stride.name]

                ins = OffsetArray(ir.dest, ir.base, ir.index, ary_length, ary_stride)

                self.append_code(ins)

            elif isinstance(ir, LinkIR):
                state['links'].append(ir)

            elif isinstance(ir, DBIR):
                state['db'].append(ir)

            else:
                raise Unknown(ir)

        state['code'] = self.code
        state['vm_code'] = self.code

        return state


# Process labels and jumps
class CodeGeneratorPass6(object):
    def __init__(self, state):
        self.registers = state['data']['registers']
        self.code = state['code']
        self.state = state

    def generate(self):
        code_stage1 = []
        labels = {}
        functions = {}

        # stage 1:
        # generate byte stream.
        # create mapping of labels to code indexes and strip labels
        # from code stream.
        # also do any final lookups for registers added at the instruction generation stage
        for func in self.code:
            assert isinstance(self.code[func][0], Function)

            for ins in self.code[func]:
                if isinstance(ins, Function):
                    functions[func] = len(code_stage1)

                elif isinstance(ins, Label):
                    # set label to point to next entry in code list
                    labels[ins.name] = len(code_stage1)

                else:
                    assembled_code = ins.assemble()

                    for i in xrange(len(assembled_code)):
                        byte = assembled_code[i]

                        if isinstance(byte, DataIR):
                            # get address of this register
                            addr = self.registers[byte.name].addr

                            # replace with address
                            assembled_code[i] = addr

                    code_stage1.extend(assembled_code)


        code_stage2 = []

        # stage 2:
        # map label and function address placeholders to actual addresses
        i = 0
        while i < len(code_stage1):
            try:
                bytecode = code_stage1[i]

                if bytecode[0] == 'addr':
                    func_addr = functions[bytecode[1]]
                    code_stage2.append(func_addr & 0xff)
                    code_stage2.append(func_addr >> 8)
                    i += 1

                elif bytecode[0] == 'label':
                    label_addr = labels[bytecode[1]]
                    code_stage2.append(label_addr & 0xff)
                    code_stage2.append(label_addr >> 8)
                    i += 1

            except TypeError:
                code_stage2.append(code_stage1[i])

            i += 1

        assert len(code_stage1) == len(code_stage2)

        self.state['code'] = code_stage2
        self.state['functions'] = functions

        # stage 3:
        # check for any vars which are actually a function name.
        # in that case, replace them with a const with that function addr.
        for func in functions:
            if func in self.registers:
                new_const = ConstIR(functions[func])
                new_const.addr = self.registers[func].addr
                self.state['data']['registers'][func] = new_const

        return self.state



# Final binary image - FX VM
class CodeGeneratorPass7(object):
    def __init__(self, state, script_name=''):
        self.functions = state['functions']
        self.registers = state['data']
        self.code = state['code']
        self.state = state
        self.script_name = script_name

    def generate(self):
        stream = ''

        meta_names = []

        # sort registers by address
        sorted_regs = sorted(self.registers['registers'].values(), key=lambda a: a.addr)

        # remove duplicates
        regs = []
        used_addrs = []
        for reg in sorted_regs:
            assert reg.addr >= 0

            if reg.addr not in used_addrs:
                used_addrs.append(reg.addr)
                regs.append(reg)

        data_len = 0
        for reg in regs:
            data_len += reg.size()

        data_len *= DATA_LEN
        code_len = len(self.code)

        # set up padding so data start will be on 32 bit boundary
        padding_len = 4 - (code_len % 4)
        code_len += padding_len

        # set up pixel objects
        pix_objects = []
        for pix in sorted(self.state['objects']['pixel_arrays'].itervalues(), key=lambda a: a.addr):
            pix_objects.append(
                PixelArrayObject(
                    index=pix.index, 
                    count=pix.length, 
                    size_x=pix.size_x,
                    size_y=pix.size_y,
                    reverse=pix.reverse
                )
            )

        packed_pix_objects = ''
        for pix in pix_objects:
            packed_pix_objects += pix.pack()

        # also translate again for VM
        pix_objects_dict = {}
        for name, pix in self.state['objects']['pixel_arrays'].iteritems():
            pix_objects_dict[name] = PixelArrayObject(
                                        index=pix.index, 
                                        count=pix.length, 
                                        size_x=pix.size_x,
                                        size_y=pix.size_y,
                                        reverse=pix.reverse
                                    )

        self.state['objects']['pixel_arrays'] = pix_objects_dict

        # set up read keys
        packed_read_keys = ''
        for key, hashed_key in self.state['data']['read_keys'].iteritems():
            packed_read_keys += struct.pack('<L', hashed_key)

        # set up write keys
        packed_write_keys = ''
        for key, hashed_key in self.state['data']['write_keys'].iteritems():
            packed_write_keys += struct.pack('<L', hashed_key)

        # set up published registers
        packed_publish = ''
        for reg in self.state['data']['publish'].itervalues():
            packed_publish += VMPublishVar(hash=catbus_string_hash(PUBLISHED_VAR_NAME_PREFIX + reg.name), addr=reg.addr).pack()

            meta_names.append(PUBLISHED_VAR_NAME_PREFIX + reg.name)

        # set up links
        packed_links = ''
        for link in self.state['links']:
            source_hash = catbus_string_hash(link.src.name)
            dest_hash = catbus_string_hash(link.dest.name)
            query = [catbus_string_hash(a.name) for a in link.query]

            meta_names.append(link.src.name)
            meta_names.append(link.dest.name)
            for q in link.query:
                meta_names.append(q.name)

            packed_links += Link(send=link.send,
                                 source_hash=source_hash, 
                                 dest_hash=dest_hash, 
                                 query=query).pack()

        # set up published registers
        packed_db = ''
        for entry in self.state['db']:
            packed_db += CatbusMeta(
                            hash=catbus_string_hash(entry.key.name), 
                            type=entry.data_type.name,
                            array_len=entry.count.name - 1).pack()

            meta_names.append(entry.key.name)

        # build program header
        header = ProgramHeader(
                    file_magic=FILE_MAGIC,
                    prog_magic=PROGRAM_MAGIC,
                    isa_version=VM_ISA_VERSION,
                    program_name_hash=catbus_string_hash(self.script_name),
                    code_len=code_len,
                    data_len=data_len,
                    pix_obj_len=len(packed_pix_objects),
                    read_keys_len=len(packed_read_keys),
                    write_keys_len=len(packed_write_keys),
                    publish_len=len(packed_publish),
                    link_len=len(packed_links),
                    db_len=len(packed_db),
                    init_start=self.functions['init'],
                    loop_start=self.functions['loop'])

        stream += header.pack()
        stream += packed_read_keys
        stream += packed_write_keys
        stream += packed_publish
        stream += packed_pix_objects
        stream += packed_links
        stream += packed_db

        # add code stream
        stream += struct.pack('<L', CODE_MAGIC)

        for b in self.code:
            stream += struct.pack('<B', b)

        # add padding if necessary to make sure data is 32 bit aligned
        stream += '\0' * padding_len

        # ensure padding is correct
        assert len(stream) % 4 == 0

        # add data table
        stream += struct.pack('<L', DATA_MAGIC)

        for reg in regs:
            if isinstance(reg, ConstIR):
                stream += struct.pack('<l', int(reg.name)) # int32

            else:
                for i in xrange(reg.size()):
                    stream += struct.pack('<l', 0) # int32


        # create hash of stream
        stream_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', stream_hash)


        # but wait, that's not all!
        # now we're going attach meta data.
        # first, prepend 32 bit (signed) program length to stream.
        # this makes it trivial to read the VM image length from the file.
        # this also gives us the offset into the file where the meta data begins.
        # note all strings will be padded to the VM_STRING_LEN.
        prog_len = len(stream)
        stream = struct.pack('<l', prog_len) + stream

        meta_data = ''

        # marker for meta
        meta_data += struct.pack('<L', META_MAGIC)

        # first string is script name
        # note the conversion with str(), this is because from a CLI
        # we might end up with unicode, when we really, really need ASCII.
        padded_string = str(self.script_name)
        # pad to string len
        padded_string += '\0' * (VM_STRING_LEN - len(padded_string))

        meta_data += padded_string

        # next, attach names of stuff
        for s in meta_names:
            padded_string = s

            if len(padded_string) > VM_STRING_LEN:
                raise SyntaxNotSupported("%s exceeds 32 characters" % (padded_string))

            padded_string += '\0' * (VM_STRING_LEN - len(padded_string))
            meta_data += padded_string

        # add meta data to end of stream
        stream += meta_data

        file_hash = catbus_string_hash(stream)
        stream += struct.pack('<L', file_hash)

        self.state.update(
               {'stream': stream,
                'file_hash': file_hash,
                'data_len': data_len,
                'code_len': code_len})

        return self.state

# # Automaton final pass
# # This does not create the binary, it just finishes up
# # what the VM will be working with.
# class CodeGeneratorPassAutomaton7(object):
#     def __init__(self, state):
#         self.functions = state['functions']
#         self.registers = state['data']
#         self.code = state['code']
#         self.state = state

#     def generate(self):

#         # sort registers by address
#         sorted_regs = sorted(self.registers['registers'].values(), key=lambda a: a.addr)

#         # remove duplicates
#         regs = []
#         used_addrs = []
#         for reg in sorted_regs:
#             if reg.addr not in used_addrs:
#                 used_addrs.append(reg.addr)
#                 regs.append(reg)

#         data_len = len(regs)
#         code_len = len(self.code)

#         image_len = data_len + code_len
    
#         # add code stream
#         code_stream = self.code
        
#         # add data table
#         data_stream = []
#         for reg in regs:
#             if isinstance(reg, ConstIR):
#                 data_stream.append(int(reg.name))

#             else:
#                 data_stream.append(0)

#         # delete stuff the automaton doesn't need
#         del self.state['vm_code']
#         del self.state['vm_data']
#         del self.state['objects']
#         del self.state['code']

#         self.state.update(
#                {'code_stream': code_stream,
#                 'data_stream': data_stream,
#                 'data_len': data_len,
#                 'code_len': code_len})

#         return self.state


class VM(object):
    def __init__(self, code, data, pix_size_x=4, pix_size_y=4):
        self.registers = data['registers']
        self.code = code
        self.cycle = 0

        self.enable_debug_print = False

        self.debug_print("VM")

        self.current_function = '_global'

        # init KV database
        self.kv = {}

        self.pix_count = pix_size_x * pix_size_y
        self.width = pix_size_x
        self.height = pix_size_y

        self.kv['pix_size_x'] = pix_size_x
        self.kv['pix_size_y'] = pix_size_y
        self.kv['pix_count'] = self.pix_count
        self.kv['kv_test_array'] = [0] * 8
        self.kv['kv_test_key'] = 0

        self.hue        = [0 for i in xrange(self.pix_count)]
        self.sat        = [0 for i in xrange(self.pix_count)]
        self.val        = [0 for i in xrange(self.pix_count)]
        self.hs_fade    = [0 for i in xrange(self.pix_count)]
        self.v_fade     = [0 for i in xrange(self.pix_count)]

        self.gfx_data   = {'hue': self.hue,
                           'sat': self.sat,
                           'val': self.val,
                           'hs_fade': self.hs_fade,
                           'v_fade': self.v_fade}

        self.objects = {PIX_OBJ_TYPE: data['objects']['pixel_arrays']}
        
        self.objects[PIX_OBJ_TYPE]['pixels'].index  = 0
        self.objects[PIX_OBJ_TYPE]['pixels'].length = self.pix_count
        self.objects[PIX_OBJ_TYPE]['pixels'].size_x = self.width
        self.objects[PIX_OBJ_TYPE]['pixels'].size_y = self.height


        # initialize data table
        data_size = sum([a.size() for a in self.registers.values()])
        self.memory = [0] * data_size

        for reg, data in self.registers.iteritems():
            if isinstance(data, ConstIR):
                self.memory[data.addr] = data.name
        
        # generate label lookup
        self.labels = {}

        for func in code:

            i = 0
            for ins in code[func]:
                if isinstance(ins, Label):
                    self.labels[ins.name] = i

                i += 1

    def dump_registers(self):
        regs = {}
        for reg, data in self.registers.iteritems():
            if isinstance(data, ArrayVarIR):
                regs[reg] = []

                for i in xrange(data.size()):
                    regs[reg].append(self.memory[data.addr + i])

            else:
                regs[reg] = self.memory[data.addr]

        return regs

    def dump_hsv(self):
        return {'hue': self.hue, 
                'sat': self.sat, 
                'val': self.val,
                'hs_fade': self.hs_fade,
                'v_fade': self.v_fade}

    def debug_print(self, s):
        if self.enable_debug_print:
            print s

    def get_var(self, name):
        return self.memory[name]

    def set_var(self, name, value):
        if isinstance(value, bool):
            value = int(value)

        self.memory[name] = value


    def calc_index(self, x, y):
        if y == 65535:
            i = x % self.pix_count

        else:
            x %= self.width
            y %= self.height

            i = x + (y * self.width)

        return i

    def run_once(self):
        self.cycle = 0

        self.init()
        self.loop()

    def init(self):
        self.run_func('init')

    def loop(self):
        self.run_func('loop')

    def run_func(self, func):
        self.current_function = func

        if self.enable_debug_print:
            self.debug_print("<------------ FUNC %s() ------------>" % (func))

        pc = 0

        while True:
            self.cycle += 1

            # prevent infinite loop
            # if self.cycle > 100000:
                # break

            ins = self.code[func][pc]
            # print ins

            if self.enable_debug_print:
                s = "Cycle: %3d PC: %3d %s" % (self.cycle, pc, ins)
                self.debug_print(s)

            pc += 1

            if isinstance(ins, Function):
                pass

            elif isinstance(ins, Label):
                pass

            elif isinstance(ins, Mov):
                self.memory[ins.dest.addr] = self.memory[ins.src.addr]

            elif isinstance(ins, Clr):
                self.memory[ins.dest.addr] = 0

            elif isinstance(ins, ArrayFuncIns):
                result = 0
                if ins.func == 'len':
                    result = ins.ary_length.name

                elif ins.func == 'max':
                    addr = ins.array.addr
                    result = self.memory[addr]
                    i = 1

                    while i < ins.ary_length.name:
                        addr += ins.ary_stride.name

                        if self.memory[addr] > result:
                            result = self.memory[addr]

                        i += 1

                elif ins.func == 'min':
                    addr = ins.array.addr
                    result = self.memory[addr]
                    i = 1

                    while i < ins.ary_length.name:
                        addr += ins.ary_stride.name

                        if self.memory[addr] < result:
                            result = self.memory[addr]

                        i += 1

                elif ins.func == 'avg':
                    addr = ins.array.addr
                    result = self.memory[addr]
                    i = 1

                    while i < ins.ary_length.name:
                        addr += ins.ary_stride.name
                        result += self.memory[addr]

                        i += 1

                    result /= ins.ary_length.name

                elif ins.func == 'sum':
                    addr = ins.array.addr
                    result = self.memory[addr]
                    i = 1

                    while i < ins.ary_length.name:
                        addr += ins.ary_stride.name
                        result += self.memory[addr]

                        i += 1

                else:
                    assert False

                self.memory[ins.dest.addr] = result

            elif isinstance(ins, OffsetArray):
                index = self.memory[ins.index.addr]
                index %= self.memory[ins.ary_length.addr]
                index *= self.memory[ins.ary_stride.addr]

                base = self.memory[ins.base.addr]
                index += base

                self.memory[ins.dest.addr] = index

            elif isinstance(ins, IndexStore):
                index = self.memory[ins.index.addr]
                base = ins.dest.addr

                self.memory[index + base] = self.memory[ins.src.addr]
            
            elif isinstance(ins, IndexLoad):
                index = self.memory[ins.index.addr]
                base = ins.src.addr

                self.memory[ins.dest.addr] = self.memory[index + base]

            elif isinstance(ins, LoadToArrayHue):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                # wraparound to 16 bit range.
                # this makes it easy to run a circular rainbow
                a = self.memory[ins.src.addr] % 65536

                self.hue[self.calc_index(index_x, index_y)] = a

            elif isinstance(ins, LoadToArraySat):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                # clamp to our 16 bit range.
                # we will essentially saturate at 0 or 65535,
                # but will not wraparound
                a = self.memory[ins.src.addr]

                if a > 65535:
                    a = 65535

                elif a < 0:
                    a = 0

                self.sat[self.calc_index(index_x, index_y)] = a

            elif isinstance(ins, LoadToArrayVal):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                
                # clamp to our 16 bit range.
                # we will essentially saturate at 0 or 65535,
                # but will not wraparound
                a = self.memory[ins.src.addr]

                if a > 65535:
                    a = 65535

                elif a < 0:
                    a = 0

                self.val[self.calc_index(index_x, index_y)] = a

            elif isinstance(ins, LoadToArrayHSFade):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                
                # clamp to our 16 bit range.
                # we will essentially saturate at 0 or 65535,
                # but will not wraparound
                a = self.memory[ins.src.addr]

                if a > 65535:
                    a = 65535

                elif a < 0:
                    a = 0

                self.hs_fade[self.calc_index(index_x, index_y)] = a

            elif isinstance(ins, LoadToArrayVFade):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                
                # clamp to our 16 bit range.
                # we will essentially saturate at 0 or 65535,
                # but will not wraparound
                a = self.memory[ins.src.addr]

                if a > 65535:
                    a = 65535

                elif a < 0:
                    a = 0

                self.v_fade[self.calc_index(index_x, index_y)] = a

            elif isinstance(ins, LoadFromArrayHue):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                self.memory[ins.dest.addr] = self.hue[self.calc_index(index_x, index_y)]

            elif isinstance(ins, LoadFromArraySat):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                self.memory[ins.dest.addr] = self.sat[self.calc_index(index_x, index_y)]

            elif isinstance(ins, LoadFromArrayVal):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                self.memory[ins.dest.addr] = self.val[self.calc_index(index_x, index_y)]

            elif isinstance(ins, LoadFromArrayHSFade):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                self.memory[ins.dest.addr] = self.hs_fade[self.calc_index(index_x, index_y)]

            elif isinstance(ins, LoadFromArrayVFade):
                index_x = self.memory[ins.index_x.addr]
                index_y = self.memory[ins.index_y.addr]

                self.memory[ins.dest.addr] = self.v_fade[self.calc_index(index_x, index_y)]

            elif isinstance(ins, Jmp):
                # JUMP!
                pc = self.labels[ins.label.name]
                continue

            elif isinstance(ins, JmpIfZeroPostDec):
                if self.memory[ins.op1.addr] == 0:

                    # JUMP!
                    pc = self.labels[ins.label.name]

                    continue

                # decrement
                self.memory[ins.op1.addr] = self.memory[ins.op1.addr] - 1

            elif isinstance(ins, JmpIfZero):
                if self.memory[ins.op1.addr] == 0:

                    # JUMP!
                    pc = self.labels[ins.label.name]

                    continue

            elif isinstance(ins, JmpIfGte):
                if self.memory[ins.op1.addr] >= self.memory[ins.op2.addr]:

                    # JUMP!
                    pc = self.labels[ins.label.name]

                    continue

            # elif isinstance(ins, JmpNotZero):
            #     if self.get_var(ins.op1.addr) != 0:
            #
            #         # JUMP!
            #         pc = self.labels[ins.label.name]
            #
            #         continue

            # elif isinstance(ins, JmpWithInc):
            #     a = self.get_var(ins.op1.addr)
            #     a += 1
            #     self.set_var(ins.op1.addr, a)
            #
            #     # JUMP!
            #     pc = self.labels[ins.label.name]
            #     continue

            elif isinstance(ins, JmpIfLessThanPreInc):
                a = self.memory[ins.op1.addr]
                a += 1
                self.memory[ins.op1.addr] = a

                if self.memory[ins.op1.addr] < self.memory[ins.op2.addr]:

                    # JUMP!
                    pc = self.labels[ins.label.name]

                    continue

            elif isinstance(ins, Not):
                if self.memory[ins.source.addr] == 0:
                    self.memory[ins.dest.addr] = 1

                else:
                    self.memory[ins.dest.addr] = 0

            elif isinstance(ins, Add):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] + self.memory[ins.op2.addr]

            elif isinstance(ins, Sub):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] - self.memory[ins.op2.addr]

            elif isinstance(ins, Mul):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] * self.memory[ins.op2.addr]

            elif isinstance(ins, Div):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] / self.memory[ins.op2.addr]

            elif isinstance(ins, Mod):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] % self.memory[ins.op2.addr]

            elif isinstance(ins, CompareEq):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] == self.memory[ins.op2.addr]

            elif isinstance(ins, CompareNeq):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] != self.memory[ins.op2.addr]

            elif isinstance(ins, CompareGt):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] > self.memory[ins.op2.addr]

            elif isinstance(ins, CompareGtE):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] >= self.memory[ins.op2.addr]

            elif isinstance(ins, CompareLt):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] < self.memory[ins.op2.addr]

            elif isinstance(ins, CompareLtE):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] <= self.memory[ins.op2.addr]

            elif isinstance(ins, And):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] and self.memory[ins.op2.addr]

            elif isinstance(ins, Or):
                self.memory[ins.result.addr] = self.memory[ins.op1.addr] or self.memory[ins.op2.addr]

            elif isinstance(ins, Rand):
                start = self.memory[ins.start.addr]
                end = self.memory[ins.end.addr]

                val = random.randint(start, end)
                self.memory[ins.dest.addr] = val

            elif isinstance(ins, Return):
                # load return value to global data
                self.memory[RETURN_VAL_ADDR] = self.memory[ins.op1.addr]

                break

            elif isinstance(ins, Print):
                print "%s %s = %s" % (ins.mnemonic, ins.op1, self.memory[ins.op1.addr])

            elif isinstance(ins, Call):
                last_func = self.current_function
                self.run_func(ins.target)
                self.current_function = last_func

            elif isinstance(ins, ArrayMov):
                if isinstance(ins.result, PixelObjIR):
                    src = self.memory[ins.op1.addr]

                    # look up pixel object
                    obj = self.objects[PIX_OBJ_TYPE][ins.result.obj]

                    # look up array
                    ary = self.gfx_data[ins.result.attr]

                    for i in xrange(obj.length):
                        index = i + obj.index

                        index %= self.pix_count

                        if ins.result.attr == 'hue':
                            src %= 65536

                        else:
                            if src > 65535:
                                src = 65535

                            elif src < 0:
                                src = 0
                        
                        ary[index] = src

                else:
                    # look up array
                    ary = self.registers[ins.result.name]

                    data = self.memory[ins.op1.addr]

                    addr = ins.result.addr

                    for i in xrange(ary.length):
                        self.memory[addr] = data

                        addr += ary.stride


            elif isinstance(ins, ArrayAdd):
                if isinstance(ins.result, PixelObjIR):
                    src = self.memory[ins.op1.addr]

                    # look up pixel object
                    obj = self.objects[PIX_OBJ_TYPE][ins.result.obj]

                    # look up array
                    ary = self.gfx_data[ins.result.attr]

                    for i in xrange(obj.length):
                        index = i + obj.index

                        index %= self.pix_count

                        a = ary[index]

                        a += src

                        if ins.result.attr == 'hue':
                            a %= 65536

                        else:
                            if a > 65535:
                                a = 65535

                            elif a < 0:
                                a = 0
                        
                        ary[index] = a

                else:
                    # look up array
                    ary = self.registers[ins.result.name]

                    data = self.memory[ins.op1.addr]

                    addr = ins.result.addr

                    for i in xrange(ary.length):
                        self.memory[addr] += data

                        addr += ary.stride


            elif isinstance(ins, ArraySub):
                if isinstance(ins.result, PixelObjIR):
                    src = self.memory[ins.op1.addr]

                    # look up pixel object
                    obj = self.objects[PIX_OBJ_TYPE][ins.result.obj]

                    # look up array
                    ary = self.gfx_data[ins.result.attr]

                    for i in xrange(obj.length):
                        index = i + obj.index

                        index %= self.pix_count

                        a = ary[index]

                        a -= src

                        if ins.result.attr == 'hue':
                            a %= 65536

                        else:
                            if a > 65535:
                                a = 65535

                            elif a < 0:
                                a = 0
                        
                        ary[index] = a

                else:
                    # look up array
                    ary = self.registers[ins.result.name]

                    data = self.memory[ins.op1.addr]

                    addr = ins.result.addr

                    for i in xrange(ary.length):
                        self.memory[addr] -= data

                        addr += ary.stride

            elif isinstance(ins, ArrayMul):
                if isinstance(ins.result, PixelObjIR):
                    src = self.memory[ins.op1.addr]

                    # look up pixel object
                    obj = self.objects[PIX_OBJ_TYPE][ins.result.obj]

                    # look up array
                    ary = self.gfx_data[ins.result.attr]

                    for i in xrange(obj.length):
                        index = i + obj.index

                        index %= self.pix_count

                        a = ary[index]

                        a *= src

                        if ins.result.attr == 'hue':
                            a %= 65536

                        else:
                            if a > 65535:
                                a = 65535

                            elif a < 0:
                                a = 0
                        
                        ary[index] = a

                else:
                    # look up array
                    ary = self.registers[ins.result.name]

                    data = self.memory[ins.op1.addr]

                    addr = ins.result.addr

                    for i in xrange(ary.length):
                        self.memory[addr] *= data

                        addr += ary.stride

            elif isinstance(ins, ArrayDiv):
                if isinstance(ins.result, PixelObjIR):
                    src = self.memory[ins.op1.addr]

                    # look up pixel object
                    obj = self.objects[PIX_OBJ_TYPE][ins.result.obj]

                    # look up array
                    ary = self.gfx_data[ins.result.attr]

                    for i in xrange(obj.length):
                        index = i + obj.index

                        index %= self.pix_count

                        a = ary[index]

                        a /= src

                        if ins.result.attr == 'hue':
                            a %= 65536

                        else:
                            if a > 65535:
                                a = 65535

                            elif a < 0:
                                a = 0
                        
                        ary[index] = a

                else:
                    # look up array
                    ary = self.registers[ins.result.name]

                    data = self.memory[ins.op1.addr]

                    addr = ins.result.addr

                    for i in xrange(ary.length):
                        self.memory[addr] /= data

                        addr += ary.stride

            elif isinstance(ins, ArrayMod):
                if isinstance(ins.result, PixelObjIR):
                    src = self.memory[ins.op1.addr]

                    # look up pixel object
                    obj = self.objects[PIX_OBJ_TYPE][ins.result.obj]

                    # look up array
                    ary = self.gfx_data[ins.result.attr]

                    for i in xrange(obj.length):
                        index = i + obj.index

                        index %= self.pix_count

                        a = ary[index]

                        a %= src

                        if ins.result.attr == 'hue':
                            a %= 65536

                        else:
                            if a > 65535:
                                a = 65535

                            elif a < 0:
                                a = 0
                        
                        ary[index] = a

                else:
                    # look up array
                    ary = self.registers[ins.result.name]

                    data = self.memory[ins.op1.addr]

                    addr = ins.result.addr

                    for i in xrange(ary.length):
                        self.memory[addr] %= data

                        addr += ary.stride

            elif isinstance(ins, Assert):
                if not self.memory[ins.op1.addr]:
                    print 'ASSERT'
                    print pc
                    print ins
                    print func
                    pprint(self.dump_registers())

                assert self.memory[ins.op1.addr]

            elif isinstance(ins, Halt):
                return 'halt'

            elif isinstance(ins, ObjectLoadInstruction):
                if isinstance(ins.op1, PixelObjIR):
                    obj = self.objects[PIX_OBJ_TYPE][ins.op1.obj]
                    
                    self.memory[ins.result.addr] = obj.toBasic()[ins.op1.attr]

                else:
                    raise UnknownInstruction(ins)    

            elif isinstance(ins, ObjectStoreInstruction):
                raise UnknownInstruction(ins)

            elif isinstance(ins, DBLoadInstruction):
                self.memory[ins.result.addr] = self.kv[ins.op1.attr]

            elif isinstance(ins, DBStoreInstruction):
                self.kv[ins.result.attr] = self.memory[ins.op1.addr]

            elif isinstance(ins, DBLenInstruction):
                try:
                    self.memory[ins.result.addr] = len(self.kv[ins.op1.attr])

                except TypeError:
                    self.memory[ins.result.addr] = 1
                
            elif isinstance(ins, DBIndexStoreInstruction):
                index = self.memory[ins.index.addr] % len(self.kv[ins.result.attr])
                self.kv[ins.result.attr][index] = self.memory[ins.op1.addr]

            elif isinstance(ins, DBIndexLoadInstruction):
                index = self.memory[ins.index.addr] % len(self.kv[ins.op1.attr])
                self.memory[ins.result.addr] = self.kv[ins.op1.attr][index]

            else:
                raise UnknownInstruction(ins)

        if self.enable_debug_print:
            self.debug_print("<------------ RETURN %s() ------------>" % (func))


def compile_text(text, debug_print=False, script_name=''):
    cg0 = CodeGeneratorPass0()

    tree = cg0.generate(text)

    if debug_print:
        import astpp
        print 'AST'
        print astpp.dump(tree)

    cg1 = CodeGeneratorPass1()
    state1 = cg1.generate(tree)

    if debug_print:
        print ''
        print ''
        print 'PASS 1'
        printer = ASTPrinter(state1)
        printer.render()


    cg2 = CodeGeneratorPass2()
    state2 = cg2.generate(state1)

    if debug_print:
        print ''
        print ''
        print 'PASS 2'
        for i in state2['code']:
            print i

        print ''
        print 'Objects:'

        for i in state2['objects']:
            print i

        print ''
        print 'Records:'

        for i in state2['records'].values():
            print i

    cg3 = CodeGeneratorPass3()
    state3 = cg3.generate(state2)

    if debug_print:
        print ''
        print ''
        print 'PASS 3'
        for i in state3['code']:
            print i

    cg4 = CodeGeneratorPass4()
    state4 = cg4.generate(state3)

    if debug_print:
        print ''
        print ''
        print 'PASS 4'
        print 'Registers:'
        for k, v in state4['data']['registers'].iteritems():
            print '%3d %32s %s' % (v.line_no, k, v)

    cg5 = CodeGeneratorPass5(state4)
    state5 = cg5.generate(state4)

    if debug_print:
        print ''
        print ''
        print 'PASS 5'
        for func in state5['code']:
            print func
            for ins in state5['code'][func]:
                print '    ', ins

    cg6 = CodeGeneratorPass6(state5)
    state6 = cg6.generate()

    if debug_print:
        print ''
        print ''
        print 'PASS 6'
        print state6['functions']

        addr = 0
        for i in state6['code']:
            try:
                print addr, hex(i)

            except TypeError:
                print addr, i

            addr += 1

        print ''
        print 'Registers:'
        for k, v in state6['data']['registers'].iteritems():
            print '%3d %32s %s' % (v.line_no, k, v)


    cg7 = CodeGeneratorPass7(state6, script_name=script_name)
    state7 = cg7.generate()

    if debug_print:
        print ''
        print ''
        print 'PASS 7'
        pprint(state7)

        print "VM ISA: %d" % (VM_ISA_VERSION)

    return state7

# def compile_automaton_text(text, debug_print=False, script_name='', condition=True, local_vars=[]):
#     if condition:
#         tree = ast.parse(text, mode='eval')

#     else:
#         tree = ast.parse(text)

#     if debug_print:
#         import astpp
#         print 'AST'
#         print astpp.dump(tree)

#     cg1 = CodeGeneratorPass1()
#     state1 = cg1.generate(tree)

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 1'
#         printer = ASTPrinter(state1)
#         printer.render()


#     cg2 = CodeGeneratorPass2()

#     state2 = cg2.generate(state1)

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 2'
#         for i in state2['code']:
#             print i

#         print ''
#         print 'Objects:'

#         for i in state2['objects']:
#             print i

#     cg3 = CodeGeneratorPass3()
#     state3 = cg3.generate(state2, include_loop=False)

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 3'
#         for i in state3['code']:
#             print i

#     cg4 = CodeGeneratorPass4()
#     state4 = cg4.generate(state3, global_registers=local_vars)

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 4'
#         print 'Registers:'
#         for k, v in state4['data']['registers'].iteritems():
#             print '%3d %32s %s' % (v.line_no, k, v)
        
#         # print 'Strings:'
#         # for k, v in state4['strings'].iteritems():
#         #     print '%3d %32s %s' % (v.line_no, k, v)
#         #
#         # print 'Keys:'
#         # for k, v in state4['keys'].iteritems():
#         #     print '%3d %32s %s' % (v.line_no, k, v)

#     # cg_auto4 = CodeGeneratorPassAutomaton4()
#     # state_auto4 = cg_auto4.generate(state4, condition=condition)

#     # if debug_print:
#     #     print ''
#     #     print ''
#     #     print 'PASS AUTOMATON 4'
#     #     for i in state_auto4['code']:
#     #         print i


#     cg5 = CodeGeneratorPass5(state4)
#     state5 = cg5.generate(state4)

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 5'
#         for func in state5['code']:
#             print func
#             for ins in state5['code'][func]:
#                 print '    ', ins

#     cg6 = CodeGeneratorPass6(state5)
#     state6 = cg6.generate()

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 6'
#         print state6['functions']

#         addr = 0
#         for i in state6['code']:
#             print addr, hex(i)
#             addr += 1

#     # return state6

#     cg7 = CodeGeneratorPassAutomaton7(state6)
#     state7 = cg7.generate()

#     if debug_print:
#         print ''
#         print ''
#         print 'PASS 7'
#         pprint(state7)

#         print "VM ISA: %d" % (VM_ISA_VERSION)

#     return state7


# # returns address to registers mapping
# def get_register_lookup(bin_data):
#     prog_len = struct.unpack('<l', bin_data[0:4])[0]
#     bin_data = bin_data[4:]

#     meta = bin_data[prog_len:]

#     # strip CRC
#     meta = meta[:-2]

#     meta_magic = struct.unpack('<L', meta[0:4])[0]
#     assert meta_magic == META_MAGIC
#     meta = meta[4:]

#     reg_names = [a for a in meta.split('\0')][:-1] # remove last entry, which is quirk of Python's split method
#     reg_names = reg_names[1:] # remove first entry, which is script name
#     addr = 0
#     registers = {}

#     for reg in reg_names:
#         registers[addr] = reg
#         addr += 1

#     return registers

def compile_script(path, debug_print=False):
    script_name = os.path.split(path)[1]

    with open(path) as f:
        return compile_text(f.read(), script_name=script_name, debug_print=debug_print)

def run_code(text, debug_print=False):
    code = compile_text(text, debug_print=debug_print)

    vm = VM(code['vm_code'], code['vm_data'], debug_print=True)
    vm.run_once()


def run_script(path, debug_print=False):
    code = compile_script(path, debug_print=debug_print)

    vm = VM(code['vm_code'], code['vm_data'])
    vm.run_once()


if __name__ == '__main__':

    path = sys.argv[1]
    script_name = os.path.split(path)[1]

    with open(path) as f:
        text = f.read()

    stream = compile_text(text, debug_print=True, script_name=script_name)['stream']

    try:
        output_path = sys.argv[2]
        with open(output_path, 'w+') as f:
            f.write(stream)

    except IndexError:
        pass





