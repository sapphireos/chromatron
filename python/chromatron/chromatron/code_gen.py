# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2022  Jeremy Billheimer
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

import pprint
import os
import ast
import sys
from textwrap import dedent
import logging

import colored_traceback
colored_traceback.add_hook()

# from .ir import *
from .ir2 import CompilerFatal, SyntaxError, OptPasses
from .instructions2 import CycleLimitExceeded
from .builder import Builder
from sapphire.common.util import setup_basic_logging


# see http://dev.stephendiehl.com/numpile/
def ast2tree(node, include_attrs=True):
    def _transform(node):
        if isinstance(node, ast.AST):
            fields = ((a, _transform(b))
                      for a, b in ast.iter_fields(node))
            if include_attrs:
                attrs = ((a, _transform(getattr(node, a)))
                         for a in node._attributes
                         if hasattr(node, a))
                return (node.__class__.__name__, dict(fields), dict(attrs))
            return (node.__class__.__name__, dict(fields))
        elif isinstance(node, list):
            return [_transform(x) for x in node]
        elif isinstance(node, str):
            return repr(node)
        return node
    if not isinstance(node, ast.AST):
        raise TypeError('expected AST, got %r' % node.__class__.__name__)
    return _transform(node)


def pformat_ast(node, include_attrs=False, **kws):
    return pprint.pformat(ast2tree(node, include_attrs), **kws)


class cg1Node(ast.AST):
    def __init__(self, lineno=None, **kwargs):
        super(cg1Node, self).__init__(**kwargs)

        assert lineno != None
        self.lineno = lineno

    def build(self, builder, target_type=None):
        raise NotImplementedError(self)

class cg1CodeNode(cg1Node):
    pass

class cg1Import(cg1Node):
    _fields = ["names"]

    def __init__(self, names, **kwargs):
        super(cg1Import, self).__init__(**kwargs)
        self.names = names


class cg1DeclarationBase(cg1Node):
    _fields = ["name", "type", "init_val", "dimensions"]

    def __init__(self, name="<anon>", type="i32", keywords=None, **kwargs):
        super(cg1DeclarationBase, self).__init__(**kwargs)

        self.name = name
        self.type = type
        self.dimensions = []

        if keywords == None:
            keywords = {}
            
        self.keywords = keywords
            
        self.init_val = None
        if 'init_val' in keywords:
            self.init_val = keywords['init_val']

        for key in self.keywords:
            if self.keywords[key] == 'True':
                self.keywords[key] = True

            elif self.keywords[key] == 'False':
                self.keywords[key] = False

    def build(self, builder, is_global=False):    
        try:
            return builder.declare_var(self.name, self.type, [a.name for a in reversed(self.dimensions)], keywords=self.keywords, is_global=is_global, lineno=self.lineno)

        except KeyError:
            var = builder.get_var(self.name, lineno=self.lineno)

            if var.data_type != self.type:
                raise SyntaxError(f'Type mismatch on redeclared var', lineno=self.lineno)
    
class cg1DeclareVar(cg1DeclarationBase):
    def __init__(self, **kwargs):
        super(cg1DeclareVar, self).__init__(**kwargs)

class cg1DeclareStr(cg1DeclarationBase):
    def __init__(self, **kwargs):
        super(cg1DeclareStr, self).__init__(**kwargs)

    def build(self, builder, is_global=False):
        try:
            self.init_val = self.init_val.build(builder)

            if 'init_val' in self.keywords:
                self.keywords['init_val'] = self.init_val

        except AttributeError:
            pass

        return super().build(builder, is_global=is_global)

class cg1StrLiteral(cg1CodeNode):
    _fields = ["s"]

    def __init__(self, s, **kwargs):
        super(cg1StrLiteral, self).__init__(**kwargs)
        self.s = s

    @property
    def name(self):
        return self.s

    def build(self, builder, target_type=None):
        return builder.add_string(self.s, lineno=self.lineno)

class cg1FormatStr(cg1CodeNode):
    _fields = ["s"]

    def __init__(self, s, **kwargs):
        super(cg1FormatStr, self).__init__(**kwargs)
        self.s = s

    @property
    def name(self):
        return self.s

    def build(self, builder, target_type=None):
        return builder.add_string(self.s, lineno=self.lineno)


class cg1DeclareStruct(cg1DeclarationBase):
    def __init__(self, struct=None, **kwargs):
        super(cg1DeclareStruct, self).__init__(**kwargs)

        self.type = struct.name
        self.struct = struct


class cg1Struct(cg1Node):
    _fields = ["name", "fields"]

    def __init__(self, name="<anon>", fields={}, **kwargs):
        super(cg1Struct, self).__init__(**kwargs)

        self.name = name
        self.fields = fields

    def build(self, builder, target_type=None):
        fields = {k: {'type':v.type, 'dimensions':v.dimensions} for (k, v) in list(self.fields.items())}
        return builder.create_struct(self.name, fields, lineno=self.lineno)


class cg1GenericObject(cg1Node):
    _fields = ["name", "kw"]

    def __init__(self, name, kw, **kwargs):
        super(cg1GenericObject, self).__init__(**kwargs)

        self.name = name
        self.kw = kw

    def build(self, builder, target_type=None):
        # should not get here
        assert False
        # builder.generic_object(self.name, args, self.kw, lineno=self.lineno)

class cg1ForwardVar(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, var, **kwargs):
        super(cg1ForwardVar, self).__init__(**kwargs)

        self.var = var
        
    def build(self, builder, lookup_depth=None, attr_depth=None, target_type=None):
        pass

    def __str__(self):
        return "cg1ForwardVar:%s=%s" % (self.var.name, self.var.type)

class cg1Var(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, name="<anon>", datatype=None, dimensions=[], **kwargs):
        super(cg1Var, self).__init__(**kwargs)

        self.name = name
        self.type = datatype
        self.dimensions = dimensions

    def build(self, builder, lookup_depth=None, attr_depth=None, target_type=None):
        try:
            return builder.get_var(self.name, self.lineno)

        except KeyError:
            raise SyntaxError(f'Variable {self.name} not declared', lineno=self.lineno)
            
            # return cg1ForwardVar(self, lineno=self.lineno)

    def __str__(self):
        return "cg1Var:%s=%s" % (self.name, self.type)

class cg1Const(cg1Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.value = self.name

    def build(self, builder, lookup_depth=None, attr_depth=None, target_type=None):
        return builder.add_const(self.value, lineno=self.lineno)

class cg1Module(cg1Node):
    _fields = ["name", "body"]

    def __init__(self, name, body, **kwargs):
        super(cg1Module, self).__init__(**kwargs)
        self.name = name
        self.body = body
        self.module = None
        self.ctx = {}

    def build(self, builder=None, script_name='', source=[]):
        if builder == None:
            builder = Builder(script_name=script_name, source=source)

        # collect everything at module level that is not part of a function
        startup_code = [a for a in self.body if not isinstance(a, cg1Func)]

        for node in startup_code:
            if isinstance(node, cg1Import):
                # load imported files and pass to builder
                for file in node.names:
                    with open(file, 'r') as f:
                        cg1 = CodeGenPass1()
                        module = cg1(f.read()).build(builder)

            # assign global vars to table
            elif isinstance(node, cg1DeclarationBase):
                node.build(builder, is_global=True)

            elif isinstance(node, cg1Struct):
                node.build(builder)

            elif isinstance(node, cg1Assign):
                if isinstance(node.value, cg1GenericObject):
                    # kw = {k: v.build(builder) for k, v in list(node.value.kw.items())}
                    kw = {k: v.name for k, v in list(node.value.kw.items())}

                    builder.generic_object(node.target.name, node.value.name, is_global=True, kw=kw, lineno=node.lineno)

                elif isinstance(node.value, cg1Const):
                    if isinstance(node.value.name, float):
                        datatype = 'f16'

                    else:
                        datatype = 'i32'

                    builder.add_named_const(node.target.name, node.value.name, data_type=datatype, lineno=node.lineno)

                elif node.value.target == 'Array':
                    raise SyntaxError("Legacy declaration of Array is no longer supported: %s" % (node.target.name), lineno=node.lineno)

                else:
                    raise SyntaxError("Unknown declaration in module body: %s" % (node.target.name), lineno=node.lineno)

            elif isinstance(node, cg1Call):
                if node.target == 'db':
                    builder.db(node.params[0].s, node.params[1].s, node.params[2].name, lineno=node.lineno)

                elif node.target in ['sync']:
                    src = node.params[0].s
                    
                    try:
                        query = [a.s for a in node.params[1].items]

                    except AttributeError:
                        raise SyntaxError("Improper syntax for sync function", lineno=node.lineno)

                    try:
                        rate = int(node.params[2].s)

                    except AttributeError:
                        rate = int(node.params[2].name)
                    
                    except IndexError:
                        rate = 1000

                    aggregation = 'any'
                    dest = src

                    if 'tag' in node.keywords:
                        tag = node.keywords['tag'].s

                    else:
                        tag = None

                    builder.link(node.target, src, dest, query, aggregation, rate, tag, lineno=node.lineno)

                elif node.target in ['send', 'receive']:
                    src = node.params[1].s
                    dest = node.params[0].s
                    query = [a.s for a in node.params[2].items]
                    try:
                        rate = int(node.params[3].s)

                    except AttributeError:
                        rate = int(node.params[3].name)
                    
                    except IndexError:
                        rate = 1000

                    try:
                        aggregation = node.params[4].s

                    except IndexError:
                        aggregation = 'any'

                    if 'tag' in node.keywords:
                        tag = node.keywords['tag'].s

                    else:
                        tag = None

                    builder.link(node.target, src, dest, query, aggregation, rate, tag, lineno=node.lineno)

        # collect funcs
        funcs = [a for a in self.body if isinstance(a, cg1Func)]

        # declare funcs:
        for func in funcs:
            builder.declare_func(func.name, lineno=func.lineno)

        # build funcs:
        for func in funcs:
            func.build(builder)

        return builder.finish_module()

class cg1NoOp(cg1CodeNode):
    def build(self, builder, target_type=None):
        return builder.nop(lineno=self.lineno)

class cg1Func(cg1CodeNode):
    _fields = ["name", "params", "body", "decorators"]

    def __init__(self, name, params, body, returns=None, decorators=[], **kwargs):
        super(cg1Func, self).__init__(**kwargs)
        self.name = name
        self.params = params
        self.body = body
        self.returns = returns
        self.decorators = decorators

    def build(self, builder, target_type=None):
        func = builder.func(self.name, returns=self.returns, lineno=self.lineno)

        for p in self.params:
            builder.add_func_arg(func, p.name, p.type, p.dimensions, lineno=self.lineno)

        for node in self.body:
            node.build(builder)

        # check if we need a default return
        if not isinstance(self.body[-1], cg1Return):
            ret = cg1Return(cg1Const(0, lineno=-1), lineno=-1)
            ret.build(builder)

        # check decorators
        for dec in self.decorators:
            if dec.target == 'schedule':
                builder.schedule(self.name, {k: v.value for k, v in dec.keywords.items()}, lineno=self.lineno)

        builder.finish_func(func)

        return func


class cg1Return(cg1CodeNode):
    _fields = ["value"]

    def __init__(self, value, **kwargs):
        super(cg1Return, self).__init__(**kwargs)
        self.value = value

    def build(self, builder, target_type=None):
        value = self.value.build(builder, target_type=builder.current_func.ret_type)

        return builder.ret(value, lineno=self.lineno)


class cg1Call(cg1CodeNode):
    _fields = ["target", "params"]

    def __init__(self, target, params, keywords={}, **kwargs):
        super(cg1Call, self).__init__(**kwargs)
        self.target = target
        self.params = params
        self.keywords = keywords

    def build(self, builder, target_type=None):
        # try:
        params = [p.build(builder) for p in self.params]

        # except VariableNotDeclared as e:
        #     if self.target in THREAD_FUNCS:
        #         raise SyntaxError("Parameter '%s' not found for function '%s'.  Thread functions require string parameters." % (e.var, self.target), self.lineno)

        #     else:
        #         raise

        try:
            target = self.target.build(builder)

        except AttributeError:
            target = self.target

        return builder.call(target, params, lineno=self.lineno)

class cg1If(cg1CodeNode):
    _fields = ["test", "body", "orelse"]

    def __init__(self, test, body, orelse, **kwargs):
        super(cg1If, self).__init__(**kwargs)
        self.test = test
        self.body = body
        self.orelse = orelse

    def build(self, builder, target_type=None):
        test = self.test.build(builder, target_type='i32')

        body_label, else_label, end_label = builder.ifelse(test, lineno=self.lineno)

        builder.position_label(body_label)    
        for node in self.body:
            node.build(builder)

        # jump to end
        builder.end_if(end_label, lineno=self.lineno)
        
        else_block = builder.do_else(lineno=self.lineno)
        builder.position_label(else_label)
        for node in self.orelse:
            node.build(builder)
        
        builder.end_ifelse(end_label, lineno=self.lineno)
        

class cg1BinOpNode(cg1CodeNode):
    _fields = ["op", "left", "right"]

    def __init__(self, op, left, right, **kwargs):
        super(cg1BinOpNode, self).__init__(**kwargs)
        self.op = op
        self.left = left
        self.right = right

    def build(self, builder, target_type=None):
        left = self.left.build(builder, target_type=target_type)

        if isinstance(self.right, cg1Tuple):
            right = []

            for item in self.right.items:
                right.append(item.build(builder, target_type=target_type))

        else:
            right = self.right.build(builder, target_type=target_type)

        return builder.binop(self.op, left, right, target_type=target_type, lineno=self.lineno)

class cg1CompareNode(cg1BinOpNode):
    pass


class cg1UnaryNot(cg1CodeNode):
    _fields = ["value"]

    def __init__(self, value, **kwargs):
        super(cg1UnaryNot, self).__init__(**kwargs)
        self.value = value

    def build(self, builder, target_type=None):
        value = self.value.build(builder)

        return builder.unary_not(value, lineno=self.lineno)

class cg1Tuple(cg1CodeNode):
    _fields = ["items"]

    def __init__(self, items, **kwargs):
        super(cg1Tuple, self).__init__(**kwargs)
        self.items = items

    def build(self, builder, target_type=None):
        return [item.name for item in self.items]

class cg1List(cg1CodeNode):
    _fields = ["items"]

    def __init__(self, items, **kwargs):
        super(cg1List, self).__init__(**kwargs)
        self.items = items

    def build(self, builder, target_type=None):
        return [item.name for item in self.items]
        

class cg1For(cg1CodeNode): 
    _fields = ["target", "iterator", "body"]

    def __init__(self, iterator, stop, body, **kwargs):
        super(cg1For, self).__init__(**kwargs)
        self.iterator = iterator
        self.stop = stop
        self.body = body

    def build(self, builder, target_type=None):
        i_declare = cg1DeclareVar(name=self.iterator.name, lineno=self.lineno)    
        i_declare.build(builder)

        i = self.iterator.build(builder)
        stop = self.stop.build(builder)

        builder.begin_for(i, stop, lineno=self.lineno)

        builder.test_for_preheader(i, stop, lineno=self.lineno)

        builder.for_header(i, stop, lineno=self.lineno)

        for node in self.body:
            node.build(builder)

        builder.end_for(i, stop, lineno=self.lineno)

class cg1While(cg1CodeNode):
    _fields = ["test", "body"]

    def __init__(self, test, body, **kwargs):
        super(cg1While, self).__init__(**kwargs)
        self.test = test
        self.body = body

    def build(self, builder, target_type=None):
        builder.begin_while(lineno=self.lineno)

        test = self.test.build(builder)
        builder.test_while_preheader(test, lineno=self.lineno)

        builder.while_header(test, lineno=self.lineno)

        test = self.test.build(builder)
        builder.test_while(test, lineno=self.lineno)

        for node in self.body:
            node.build(builder)
        
        builder.end_while(lineno=self.lineno)


class cg1Break(cg1CodeNode):
    def build(self, builder, target_type=None):
        builder.loop_break(lineno=self.lineno)

class cg1Continue(cg1CodeNode):
    def build(self, builder, target_type=None):
        builder.loop_continue(lineno=self.lineno)


class cg1Assert(cg1CodeNode):
    _fields = ["test"]

    def __init__(self, test, **kwargs):
        super(cg1Assert, self).__init__(**kwargs)
        self.test = test
        
    def build(self, builder, target_type=None):
        test = self.test.build(builder)
        builder.assertion(test, lineno=self.lineno)


class cg1Assign(cg1CodeNode):
    _fields = ["target", "value"]

    def __init__(self, target, value, **kwargs):
        super(cg1Assign, self).__init__(**kwargs)
        self.target = target
        self.value = value

    def build(self, builder, target_type=None):
        target = self.target.build(builder)
        value = self.value.build(builder, target_type=target.data_type)

        if value == 'print':
            raise SyntaxError('print() does not return a value', lineno=self.lineno)

        return builder.assign(target, value, lineno=self.lineno)


class cg1AugAssign(cg1CodeNode):
    _fields = ["op", "target", "value"]

    def __init__(self, op, target, value, **kwargs):
        super(cg1AugAssign, self).__init__(**kwargs)
        self.op = op
        self.target = target
        self.value = value

    def build(self, builder, target_type=None):
        target = self.target.build(builder)

        value = self.value.build(builder, target_type=target.data_type)

        if value == 'print':
            raise SyntaxError('print() does not return a value', lineno=self.lineno)
    
        builder.augassign(self.op, target, value, lineno=self.lineno)


class cg1Attribute(cg1CodeNode):
    _fields = ["obj", "attr", "load"]
    def __init__(self, obj, attr, load=True, **kwargs):
        super(cg1Attribute, self).__init__(**kwargs)

        self.obj = obj
        self.attr = attr
        self.load = load
        
    def build(self, builder, lookup_depth=0, attr_depth=0, target_type=None):
        if attr_depth == 0:
            builder.start_attr(lineno=self.lineno)

        target = self.obj.build(builder, lookup_depth=lookup_depth, attr_depth=attr_depth + 1)
        builder.add_attr(self.attr, target=target, lineno=self.lineno)      

        if attr_depth == 0:
            return builder.finish_attr(target, lineno=self.lineno)

        return target


class cg1Subscript(cg1CodeNode):
    _fields = ["target", "index", "load"]

    def __init__(self, target, index, load=True, **kwargs):
        super(cg1Subscript, self).__init__(**kwargs)
        self.target = target
        self.index = index
        self.load = load

    def to_list(self):
        l = [self.index]
        target = self.target

        while isinstance(target, cg1Subscript):
            l.append(target.index)

            target = target.target


        return target, list(reversed(l))

    def build(self, builder, lookup_depth=0, attr_depth=0, target_type=None):
        if lookup_depth == 0:
            builder.start_lookup(lineno=self.lineno)

        target = self.target.build(builder, lookup_depth=lookup_depth + 1, attr_depth=attr_depth)
        index = self.index.build(builder)

        builder.add_lookup(index, lineno=self.lineno)      

        if lookup_depth == 0:
            return builder.finish_lookup(target, lineno=self.lineno)

        return target

class CodeGenPass1(ast.NodeVisitor):
    def __init__(self):
        self._declarations = {
            'Number': self._handle_Number,
            'Fixed16': self._handle_Fixed16,
            'String': self._handle_String,
            'StringBuf': self._handle_StringBuf,
            'Function': self._handle_Function,
            # 'Array': self._handle_Array,
            'Struct': self._handle_Struct,
            'PixelArray': self._handle_PixelArray,
            # 'Palette': self.create_GenericObject,
        }

        self._struct_types = {}

        self.in_func = False

    def __call__(self, source):
        # remove leading indentation
        source = dedent(source)

        self._source = source
        self._ast = ast.parse(source)

        return self.visit(self._ast)

    def visit_Module(self, node):
        body = list(map(self.visit, node.body))
        
        return cg1Module("module", body, lineno=0)

    def visit_FunctionDef(self, node):
        self.in_func = True
        body = list(map(self.visit, list(node.body)))
        params = list(map(self.visit, node.args.args))

        decorators = [self.visit(a) for a in node.decorator_list]
        
        if node.returns:
            returns = self.visit(node.returns).name
        
        else:
            returns = None

        self.in_func = False

        return cg1Func(node.name, params, body, returns, decorators, lineno=node.lineno)

    def visit_Return(self, node):
        if node.value == None:
            value = cg1Const(0, lineno=node.lineno)

        else:
            value = self.visit(node.value)

        return cg1Return(value, lineno=node.lineno)

    def _handle_Number(self, node):
        keywords = {}
        for kw in node.keywords:
            keywords[kw.arg] = kw.value.value

        if len(node.args) > 0:
            if len(node.args) > 1:
                keywords['init_val'] = [node.args[i].n for i in range(len(node.args))]
            else:
                keywords['init_val'] = node.args[0].n

        return cg1DeclareVar(type="i32", keywords=keywords, lineno=node.lineno)

    def _handle_Fixed16(self, node):
        keywords = {}
        for kw in node.keywords:
            keywords[kw.arg] = kw.value.value

        if len(node.args) > 0:
            if len(node.args) > 1:
                keywords['init_val'] = [int(node.args[i].n * 65536) for i in range(len(node.args))]
            else:
                keywords['init_val'] = int(node.args[0].n * 65536) # convert to fixed16

        return cg1DeclareVar(type="f16", keywords=keywords, lineno=node.lineno)

    def _handle_Function(self, node):
        keywords = {}
        for kw in node.keywords:
            keywords[kw.arg] = kw.value.value

        if len(node.args) > 0:
            keywords['init_val'] = node.args[0].id

        return cg1DeclareVar(type="funcref", keywords=keywords, lineno=node.lineno)

    def _handle_String(self, node):
        keywords = {}
        for kw in node.keywords:
            if kw.arg == 'length':
                kw.arg = 'strlen'
            keywords[kw.arg] = kw.value.value

        if len(node.args) == 0:
            # uninitialized
            # this is a basic string reference
            return cg1DeclareStr(type='strref', keywords=keywords, lineno=node.lineno)

        else:
            if isinstance(node.args[0], ast.Str):
                init_val = node.args[0].s
                keywords['init_val'] = cg1StrLiteral(init_val, lineno=node.lineno)
                return cg1DeclareStr(type='strref', keywords=keywords, lineno=node.lineno)

            else:
                length = node.args[0].n
                keywords['strlen'] = length

                raise SyntaxError(f'String reference cannot be declared with buffer size.  Did you mean StringBuf({length})?', lineno=node.lineno)

    def _handle_StringBuf(self, node):
        keywords = {}
        for kw in node.keywords:
            if kw.arg == 'length':
                kw.arg = 'strlen'
            keywords[kw.arg] = kw.value.value

        if len(node.args) == 0:
            raise SyntaxError(f'StringBuf must be declared with buffer size or initializer', lineno=node.lineno)

        else:
            if isinstance(node.args[0], ast.Str):
                init_val = node.args[0].s
                keywords['init_val'] = cg1StrLiteral(init_val, lineno=node.lineno)

            else:
                length = node.args[0].n
                keywords['strlen'] = length

            return cg1DeclareStr(type='strbuf', keywords=keywords, lineno=node.lineno)


        # if len(node.args) > 0 or len(node.keywords) > 0:
        #     keywords = {}
        #     for kw in node.keywords:
        #         keywords[kw.arg] = kw.value.value

        #     if len(node.args) > 0:
        #         if isinstance(node.args[0], ast.Str):
        #             init_val = node.args[0].s
        #             keywords['init_val'] = cg1StrLiteral(init_val, lineno=node.lineno)

        #         else:
        #             length = node.args[0].n
        #             keywords['length'] = length

        #     return cg1DeclareStr(type="str", keywords=keywords, lineno=node.lineno)

        # else:
        #     return cg1DeclareStr(type="str", lineno=node.lineno)
    
    # def _handle_Array(self, node):
    #     dims = [a.n for a in node.args]

    #     data_type = 'i32'
    #     keywords = {}
        
    #     for kw in node.keywords:
    #         if kw.arg == 'type':
    #             try:
    #                 data_type = kw.value.func.id

    #             except AttributeError:
    #                 data_type = kw.value.id

    #             break

    #     if data_type == 'Number':
    #         data_type = 'i32'

    #     elif data_type == 'Fixed16':
    #         data_type = 'f16'

    #     elif data_type == 'String':
    #         data_type = 'str'
        
    #     for kw in node.keywords:
    #         if kw.arg == 'type':
    #             continue
                
    #         if kw.arg == 'init_val':
    #             if data_type == 'f16':
    #                 keywords[kw.arg] = [int(a.n * 65536) for a in kw.value.elts] # convert to fixed16

    #             else:
    #                 keywords[kw.arg] = [a.n for a in kw.value.elts]

    #         else:
    #             keywords[kw.arg] = kw.value.id

    #     return cg1DeclareArray(type=data_type, dimensions=dims, keywords=keywords, lineno=node.lineno)

    def _handle_Struct(self, node):
        fields = {}

        for kw in node.keywords:
            field_name = kw.arg
            field_type = self.visit(kw.value)

            if isinstance(field_type, cg1Subscript):
                field_type, l = field_type.to_list()
                
                if isinstance(field_type, cg1DeclarationBase):
                    field_type.dimensions = l

            fields[field_name] = field_type

        return cg1Struct(fields=fields, lineno=node.lineno)

    def _handle_PixelArray(self, node):
        if len(node.args) > 0 or len(node.keywords) > 0:
            if self.in_func:
                raise SyntaxError(f'{node.func.id} cannot be created from within a function.', lineno=node.lineno)

            args = [self.visit(a) for a in node.args]
            kwargs = {}

            for kw in node.keywords:
                field_name = kw.arg
                field_type = self.visit(kw.value)

                kwargs[field_name] = field_type

            if len(node.args) >= 1:
                kwargs['index'] = args[0]

            if len(node.args) >= 2:
                kwargs['count'] = args[1]

            return cg1GenericObject(node.func.id, kwargs, lineno=node.lineno)

            # return self.create_GenericObject(node)

        # return cg1DeclareVar(type="pixref", lineno=node.lineno)
        return cg1DeclareVar(type="PixelArray", lineno=node.lineno)

    # def create_GenericObject(self, node):
    #     args = [self.visit(a) for a in node.args]
    #     kwargs = {}

    #     for kw in node.keywords:
    #         field_name = kw.arg
    #         field_type = self.visit(kw.value)

    #         kwargs[field_name] = field_type

    #     return cg1GenericObject(node.func.id, args, kwargs, lineno=node.lineno)

    def visit_Call(self, node):
        if isinstance(node.func, ast.Subscript):
            func = self.visit(node.func)
            
        else:
            ref = None
            func = node.func.id

        if func in self._declarations:
            return self._declarations[func](node)

        elif func in self._struct_types:
            struct_type = self._struct_types[func]

            return cg1DeclareStruct(struct=struct_type, lineno=node.lineno)

        elif self.in_func:
            kwargs = {}
            for kw in map(self.visit, node.keywords):
                kwargs.update(kw)

            args = list(map(self.visit, node.args))
            return cg1Call(func, args, kwargs, lineno=node.lineno)

        else:
            kwargs = {}
            for kw in map(self.visit, node.keywords):
                kwargs.update(kw)

            # function call at module level
            return cg1Call(func, list(map(self.visit, node.args)), kwargs, lineno=node.lineno)

    def visit_Yield(self, node):
        return cg1Call('yield', [], lineno=node.lineno)
        
    def visit_keyword(self, node):
        return {node.arg: self.visit(node.value)}

    def visit_List(self, node):
        return cg1List([self.visit(e) for e in node.elts], lineno=node.lineno)

    def visit_If(self, node):
        return cg1If(self.visit(node.test), list(map(self.visit, node.body)), list(map(self.visit, node.orelse)), lineno=node.lineno)
    
    def visit_Compare(self, node):
        assert len(node.ops) <= 1

        left = self.visit(node.left)
        right = self.visit(node.comparators[0])
        op = self.visit(node.ops[0])

        return cg1CompareNode(op, left, right, lineno=node.lineno)

    def visit_Assign(self, node):
        assert len(node.targets) == 1

        target = self.visit(node.targets[0])
        value = self.visit(node.value)

        if isinstance(value, cg1DeclarationBase):
            value.name = target.name
            return value

        elif isinstance(value, cg1Subscript):
            new_value, l = value.to_list()
            
            if isinstance(new_value, cg1DeclarationBase):
                new_value.dimensions = l
                new_value.name = target.name
                return new_value

            return cg1Assign(target, value, lineno=node.lineno)

        elif isinstance(value, cg1Struct):
            value.name = target.name
            self._struct_types[value.name] = value

            return value

        else:
            return cg1Assign(target, value, lineno=node.lineno)

    def visit_AugAssign(self, node):
        return cg1AugAssign(self.visit(node.op), self.visit(node.target), self.visit(node.value), lineno=node.lineno)

    def visit_Constant(self, node):
        if isinstance(node.value, str):
            return cg1StrLiteral(node.value, lineno=node.lineno)

        else:
            return cg1Const(node.value, lineno=node.lineno)

    def visit_Name(self, node):
        return cg1Var(node.id, lineno=node.lineno)

    def visit_NameConstant(self, node):
        return cg1Var(node.value, lineno=node.lineno)

    def visit_arg(self, node):
        if isinstance(node.annotation, ast.Subscript):
            temp = self.visit(node.annotation)
            
            def get_dimensions(s, dimensions=[]):
                dimensions.append(s.index.name)

                if isinstance(s.target, cg1Subscript):
                    return get_dimensions(s.target, dimensions)

                return s.target.name, dimensions

            data_type, dimensions = get_dimensions(temp)

        else:
            dimensions = []
                
            if node.annotation is not None:
                data_type = node.annotation.id

            else:
                data_type = 'i32'
        
        return cg1Var(node.arg, data_type, dimensions, lineno=node.lineno)

    def visit_BinOp(self, node):
        op = self.visit(node.op)
        left = self.visit(node.left)
        right = self.visit(node.right)

        return cg1BinOpNode(op, left, right, lineno=node.lineno)

    def visit_BoolOp(self, node):
        op = self.visit(node.op)

        left = self.visit(node.values.pop(0))

        while len(node.values) > 0:
            right = self.visit(node.values.pop(0))
            
            result = cg1BinOpNode(op, left, right, lineno=node.lineno)
            left = result

        return result

    def visit_UnaryOp(self, node):
        if isinstance(node.op, ast.USub) and isinstance(node.operand, ast.Constant):
            # this can happen if we have a constant which is negative.
            # thanks, Python 3, for making this unncessarily complicated.

            # get constant, with negation
            return cg1Const(-1 * node.operand.value, lineno=node.lineno)

        assert isinstance(node.op, ast.Not)

        return cg1UnaryNot(self.visit(node.operand), lineno=node.lineno)

    def visit_Add(self, node):
        return "add"

    def visit_Sub(self, node):
        return "sub"

    def visit_Mult(self, node):
        return "mul"

    def visit_Div(self, node):
        return "div"

    def visit_FloorDiv(self, node):
        return "div"

    def visit_Mod(self, node):
        return "mod"

    def visit_Lt(self, node):
        return "lt"

    def visit_LtE(self, node):
        return "lte"

    def visit_Gt(self, node):
        return "gt"

    def visit_GtE(self, node):
        return "gte"

    def visit_Eq(self, node):
        return "eq"

    def visit_NotEq(self, node):
        return "neq"

    def visit_And(self, node):
        return "logical_and"

    def visit_Or(self, node):
        return "logical_or"

    def visit_In(self, node):
        return "in"

    def visit_Expr(self, node):
        return self.visit(node.value)

    def visit_For(self, node):
        # Check for an else clause.  Python has an atypical else construct
        # you can use at the end of a loop.  But it is confusing and
        # rarely used, so we are not going to support it.
        assert len(node.orelse) == 0

        return cg1For(self.visit(node.target), self.visit(node.iter), list(map(self.visit, node.body)), lineno=node.lineno)

    def visit_While(self, node):
        # Check for an else clause.  Python has an atypical else construct
        # you can use at the end of a loop.  But it is confusing and
        # rarely used, so we are not going to support it.
        assert len(node.orelse) == 0

        return cg1While(self.visit(node.test), list(map(self.visit, node.body)), lineno=node.lineno)

    def visit_Attribute(self, node):
        load = True
        if isinstance(node.ctx, ast.Store):
            load = False

        value = self.visit(node.value)

        return cg1Attribute(value, node.attr, load=load, lineno=node.lineno)

    def visit_Pass(self, node):
        return cg1NoOp(lineno=node.lineno)

    def visit_Break(self, node):
        return cg1Break(lineno=node.lineno)

    def visit_Continue(self, node):
        return cg1Continue(lineno=node.lineno)

    def visit_Assert(self, node):
        return cg1Assert(self.visit(node.test), lineno=node.lineno)

    def visit_Index(self, node):
        return self.visit(node.value)

    def visit_Subscript(self, node):
        load = True
        if isinstance(node.ctx, ast.Store):
            load = False

        return cg1Subscript(self.visit(node.value), self.visit(node.slice), load=load, lineno=node.lineno)

    def visit_Import(self, node):
        return cg1Import([a.name for a in node.names], lineno=node.lineno)

    def visit_Tuple(self, node):
        items = [self.visit(a) for a in node.elts]
        return cg1Tuple(items, lineno=node.lineno)        

    # def visit_FormattedValue(self, node):
    #     # print(node)
    #     # print(dir(node))
    #     # print(node.value)
    #     print(node.format_spec)

    #     return self.visit(node.value)

    def visit_JoinedStr(self, node):
        raise SyntaxError(f'f-string syntax not supported.', lineno=node.lineno)

        # for val in node.values:
        #     visited = self.visit(val)

        #     if isinstance(visited, cg1StrLiteral):
        #         print(visited.s, len(visited.s))

        #     else:
        #         print(visited)

        # return cg1FormatStr('meow', lineno=node.lineno)

    def generic_visit(self, node):
        raise NotImplementedError(node)


def parse(source):
    return ast.parse(source)

def compile_text(source, debug_print=False, summarize=False, script_name='', opt_passes=OptPasses.SSA):
    import colored_traceback
    colored_traceback.add_hook()
    
    # tree = parse(source)
    # if debug_print:
    #     print("Python AST:")
    #     print(pformat_ast(tree))

    cg1 = CodeGenPass1()
    cg1_data = cg1(source)

    if debug_print:
        print("CG Pass 1:")
        print(pformat_ast(cg1_data))


    ir_program = cg1_data.build(script_name=script_name, source=source)

    e = None
    try:
        ir_program.analyze(opt_passes=opt_passes)

    except Exception as exc:
        e = exc
        # raise

    # generate instructions
    ins_program = None
    try:
        if not e:
            ins_program = ir_program.generate()

    except Exception as exc:
        e = exc    

    if isinstance(e, CompilerFatal):
        raise e

    # save IR to file
    debug_filename = f'{script_name}_{opt_passes}.fxir'
    with open(debug_filename, 'w') as f:
        try:
            f.write(str(ir_program))

            if ins_program:
                f.write(str(ins_program))

        except AttributeError:
            if e:
                raise e

            raise

    # return

    # if debug_print:
    #     print(ir_program)
        
    #     if ins_program:
    #         print(ins_program)

    if e:
        raise e

    return ins_program
    
    
    # builder.allocate()
    # builder.generate_instructions()

    # if debug_print:
    #     builder.print_instructions()
    #     builder.print_data_table()
    #     # builder.print_control_flow()

    # builder.assemble()
    # builder.generate_binary()

    # if debug_print:
    #     builder.print_func_addrs()

    # if debug_print or summarize:
    #     print("VM ISA:  %d" % (VM_ISA_VERSION))
    #     print("Program name: %s Hash: 0x%08x" % (builder.script_name, builder.header.program_name_hash))
    #     print("Stream length:   %d bytes"   % (len(builder.stream)))
    #     print("Code length:     %d bytes"   % (builder.header.code_len))
    #     print("Data length:     %d bytes"   % (builder.header.data_len))
    #     print("Functions:       %d"         % (len(builder.funcs)))
    #     print("Read keys:       %d"         % (len(builder.read_keys)))
    #     print("Write keys:      %d"         % (len(builder.write_keys)))
    #     print("Published vars:  %d"         % (builder.published_var_count))
    #     print("Pixel arrays:    %d"         % (len(builder.pixel_arrays)))
    #     print("Links:           %d"         % (len(builder.links)))
    #     print("DB entries:      %d"         % (len(builder.db_entries)))
    #     print("Cron entries:    %d"         % (len(builder.cron_tab)))
    #     print("Stream hash:     0x%08x"     % (builder.stream_hash))

    # return builder


def compile_script(path, debug_print=False, opt_passes=OptPasses.SSA):
    script_name = os.path.split(path)[1]

    logging.info(f'Compiling {script_name}')

    with open(path) as f:
        return compile_text(f.read(), script_name=script_name, debug_print=debug_print, opt_passes=opt_passes)

def run_script(path, debug_print=False, opt_passes=OptPasses.SSA):
    ins_program = compile_script(path, debug_print=debug_print, opt_passes=opt_passes)

    # for func in ins_program.funcs:
    func = ins_program.init_func
    try:
        ret_val = func.run()
        print(f'VM returned: {ret_val}')

    except CycleLimitExceeded:
        print(f'!!! Cycle limit exceeded !!!')

    pprint.pprint(ins_program.gfx_data)
    pprint.pprint(ins_program.dump_globals())
    pprint.pprint(ins_program.db)

    image = ins_program.assemble()
    stream = image.render()
    print(image.header)
    
    for func_name, func_info in image.function_table.items():
        print(func_name)
        print(func_info)

    print(f'prog len: {image.prog_len} image len: {image.image_len}')


    return stream


OPT_LEVELS = {
    'none': [OptPasses.NONE],
    'ls_sched': [OptPasses.LS_SCHED],
    'ssa': [OptPasses.SSA],
    'gvn': [OptPasses.GVN],
    'loop': [OptPasses.LOOP],
    'all': [OptPasses.SSA, OptPasses.GVN, OptPasses.LOOP, OptPasses.LS_SCHED],
}

# OPT_LEVELS['default'] = OPT_LEVELS['all']
# OPT_LEVELS['default'] = [OptPasses.SSA, OptPasses.GVN, OptPasses.LOOP]
# OPT_LEVELS['default'] = [OptPasses.SSA, OptPasses.LS_SCHED, OptPasses.LOOP]
# OPT_LEVELS['default'] = [OptPasses.SSA, OptPasses.LS_SCHED]
# OPT_LEVELS['default'] = [OptPasses.SSA, OptPasses.LOOP]

# OPT_LEVELS['default'] = [OptPasses.SSA, OptPasses.GVN]
OPT_LEVELS['default'] = [OptPasses.SSA]


def main():
    path = sys.argv[1]
    script_name = os.path.split(path)[1]

    setup_basic_logging(show_thread=False)

    logging.info(f'Compiling: {script_name}')

    # try:
    #     opt_levels = sys.argv[2:]

    # except IndexError:
    #     opt_levels = ['default']

    opt_passes = OPT_LEVELS['default']


    logging.info(f'Optimization passes: {opt_passes}')


    try:
        program = run_script(path, debug_print=True, opt_passes=opt_passes)
        # program = compile_script(path, debug_print=True, opt_passes=opt_level)
        # program.simulate()
        
        # pprint.pprint(program.gfx_data)
        # pprint.pprint(program.dump_globals())

        try:
            output_path = os.path.join(sys.argv[2], script_name + 'b')
            with open(output_path, 'wb+') as f:
                f.write(program)

        except IndexError:
            pass

    except IOError:
        # path was a directory

        # compile and summarize all files
        stats = {}

        success = 0
        errors = 0
        error_files = []

        for fpath in os.listdir(path):
            fname, ext = os.path.splitext(fpath)

            if ext != '.fx':
                continue

            with open(os.path.join(path, fpath)) as f:
                print('\n*********************************')
                print(fpath)
                print('---------------------------------')
                text = f.read()
                try:
                    program = compile_text(text, summarize=False)
                    image = program.assemble()
                    image.render()

                    stats[fpath] = {'code': image.header.code_len,
                                    'local_data': image.header.local_data_len,
                                    'global_data': image.header.global_data_len,
                                    'constant': image.header.constant_len,
                                    'stream': len(image.stream)}

                    success += 1

                except SyntaxError as e:
                    errors += 1
                    # print("SyntaxError:", e)
                    raise

                except Exception as e:
                    errors += 1
                    error_files.append(fpath)
                    print("Exception:", e)
                    # raise

        print('')
        for param in ['code', 'local_data', 'global_data', 'constant', 'stream']:
            highest = 0
            name = ''
            for script in stats:
                if stats[script][param] > highest:
                    highest = stats[script][param]
                    name = script

            print("Largest %16s size: %32s %5d bytes" % (param, name, highest))

        print(f'\nTotal: {success + errors} Success: {success} Errors: {errors}')

        if errors > 0:
            print(f'\nFiles with errors:')
            for file in error_files:
                print(f'\t{file}')

profile = False
if __name__ == '__main__':

    if profile:
        import yappi
        yappi.start()

    main()

    if profile:            # try:

        yappi.get_func_stats().print_all()
