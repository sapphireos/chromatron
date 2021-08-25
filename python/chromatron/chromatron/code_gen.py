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

import pprint
import os
import ast
import sys
from textwrap import dedent
import logging

import colored_traceback
colored_traceback.add_hook()

# from .ir import *
from .ir2 import CompilerFatal, SyntaxError
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

    def build(self, builder):
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
        return builder.declare_var(self.name, self.type, [a.name for a in reversed(self.dimensions)], keywords=self.keywords, is_global=is_global, lineno=self.lineno)
    
class cg1DeclareVar(cg1DeclarationBase):
    def __init__(self, **kwargs):
        super(cg1DeclareVar, self).__init__(**kwargs)

class cg1DeclareStr(cg1DeclarationBase):
    def __init__(self, **kwargs):
        super(cg1DeclareStr, self).__init__(**kwargs)

    def build(self, builder, **kwargs):    
        self.keywords['init_val'] = self.keywords['init_val'].build(builder)

        super(cg1DeclareStr, self).build(builder, **kwargs)

# class cg1DeclareArray(cg1DeclarationBase):
#     def __init__(self, dimensions=[1], **kwargs):
#         super(cg1DeclareArray, self).__init__(**kwargs)

#         self.dimensions = dimensions


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

    def build(self, builder):
        fields = {k: {'type':v.type, 'dimensions':v.dimensions} for (k, v) in list(self.fields.items())}
        return builder.create_struct(self.name, fields, lineno=self.lineno)


class cg1GenericObject(cg1Node):
    _fields = ["name", "args", "kw"]

    def __init__(self, name, args, kw, **kwargs):
        super(cg1GenericObject, self).__init__(**kwargs)

        self.name = name
        self.args = args
        self.kw = kw

    def build(self, builder):
        # should not get here
        assert False
        # builder.generic_object(self.name, args, self.kw, lineno=self.lineno)


class cg1Var(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, name="<anon>", datatype=None, **kwargs):
        super(cg1Var, self).__init__(**kwargs)

        self.name = name
        
        self.type = datatype

    def build(self, builder, depth=None):
        return builder.get_var(self.name, self.lineno)
    
    def __str__(self):
        return "cg1Var:%s=%s" % (self.name, self.type)

class cg1Const(cg1Var):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.value = self.name

    def build(self, builder, depth=None):
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
                    args = [a.build(builder) for a in node.value.args]
                    kw = {k: v.build(builder) for k, v in list(node.value.kw.items())}

                    builder.generic_object(node.target.name, node.value.name, args, kw, lineno=node.lineno)

                elif isinstance(node.value, cg1Const):
                    if isinstance(node.value.name, float):
                        datatype = 'f16'

                    else:
                        datatype = 'i32'

                    builder.add_named_const(node.target.name, node.value.name, data_type=datatype, lineno=node.lineno)

                else:
                    raise SyntaxError("Unknown declaration in module body: %s" % (node.target.name), lineno=node.lineno)

            elif isinstance(node, cg1Call):
                if node.target == 'db':
                    builder.db(node.params[0].s, node.params[1].s, node.params[2].name, lineno=node.lineno)

                elif node.target in ['send', 'receive']:
                    src = node.params[1].s
                    dest = node.params[0].s
                    query = [a.s for a in node.params[2].items]
                    try:
                        rate = node.params[3].s
                    
                    except IndexError:
                        rate = 1000

                    try:
                        aggregation = node.params[4].s

                    except IndexError:
                        aggregation = 'any'

                    builder.link(node.target, src, dest, query, aggregation, rate, lineno=node.lineno)

        # collect funcs
        funcs = [a for a in self.body if isinstance(a, cg1Func)]

        for code in funcs:
            code.build(builder)

        return builder.finish_module()

class cg1NoOp(cg1CodeNode):
    def build(self, builder):
        return builder.nop(lineno=self.lineno)

class cg1Func(cg1CodeNode):
    _fields = ["name", "params", "body", "decorators"]

    def __init__(self, name, params, body, decorators=[], **kwargs):
        super(cg1Func, self).__init__(**kwargs)
        self.name = name
        self.params = params
        self.body = body
        self.decorators = decorators

    def build(self, builder):
        func = builder.func(self.name, lineno=self.lineno)

        for p in self.params:
            builder.add_func_arg(func, p.name, p.type, [], lineno=self.lineno)


        for node in self.body:
            node.build(builder)

        # check if we need a default return
        if not isinstance(self.body[-1], cg1Return):
            ret = cg1Return(cg1Const(0, lineno=-1), lineno=-1)
            ret.build(builder)

        # check decorators
        for dec in self.decorators:
            if dec.target == 'schedule':
                builder.schedule(self.name, dec.keywords, lineno=self.lineno)

        builder.finish_func(func)

        return func


class cg1Return(cg1CodeNode):
    _fields = ["value"]

    def __init__(self, value, **kwargs):
        super(cg1Return, self).__init__(**kwargs)
        self.value = value

    def build(self, builder):
        value = self.value.build(builder)

        return builder.ret(value, lineno=self.lineno)


class cg1Call(cg1CodeNode):
    _fields = ["target", "params"]

    def __init__(self, target, params, keywords={}, **kwargs):
        super(cg1Call, self).__init__(**kwargs)
        self.target = target
        self.params = params
        self.keywords = keywords

    def build(self, builder):
        try:
            params = [p.build(builder) for p in self.params]

        except VariableNotDeclared as e:
            if self.target in THREAD_FUNCS:
                raise SyntaxError("Parameter '%s' not found for function '%s'.  Thread functions require string parameters." % (e.var, self.target), self.lineno)

            else:
                raise

        return builder.call(self.target, params, lineno=self.lineno)

class cg1If(cg1CodeNode):
    _fields = ["test", "body", "orelse"]

    def __init__(self, test, body, orelse, **kwargs):
        super(cg1If, self).__init__(**kwargs)
        self.test = test
        self.body = body
        self.orelse = orelse

    def build(self, builder):
        test = self.test.build(builder)

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

    def build(self, builder):
        left = self.left.build(builder)
        right = self.right.build(builder)

        return builder.binop(self.op, left, right, lineno=self.lineno)

class cg1CompareNode(cg1BinOpNode):
    pass


class cg1UnaryNot(cg1CodeNode):
    _fields = ["value"]

    def __init__(self, value, **kwargs):
        super(cg1UnaryNot, self).__init__(**kwargs)
        self.value = value

    def build(self, builder):
        value = self.value.build(builder)

        return builder.unary_not(value, lineno=self.lineno)

class cg1Tuple(cg1CodeNode):
    _fields = ["items"]

    def __init__(self, items, **kwargs):
        super(cg1Tuple, self).__init__(**kwargs)
        self.items = items

    def build(self, builder):
        return [item.name for item in self.items]

class cg1List(cg1CodeNode):
    _fields = ["items"]

    def __init__(self, items, **kwargs):
        super(cg1List, self).__init__(**kwargs)
        self.items = items

    def build(self, builder):
        return [item.name for item in self.items]
        

class cg1For(cg1CodeNode): 
    _fields = ["target", "iterator", "body"]

    def __init__(self, iterator, stop, body, **kwargs):
        super(cg1For, self).__init__(**kwargs)
        self.iterator = iterator
        self.stop = stop
        self.body = body

    def build(self, builder):
        i_declare = cg1DeclareVar(name=self.iterator.name, lineno=self.lineno)    
        i_declare.build(builder)

        i = self.iterator.build(builder)
        stop = self.stop.build(builder)
        
        top, cont, end = builder.begin_for(i, lineno=self.lineno)

        
        builder.position_label(top)

        for node in self.body:
            node.build(builder)

        builder.position_label(cont)

        builder.end_for(i, stop, top, lineno=self.lineno)

        builder.position_label(end)

class cg1While(cg1CodeNode):
    _fields = ["test", "body"]

    def __init__(self, test, body, **kwargs):
        super(cg1While, self).__init__(**kwargs)
        self.test = test
        self.body = body

    def build(self, builder):
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
    def build(self, builder):
        builder.loop_break(lineno=self.lineno)

class cg1Continue(cg1CodeNode):
    def build(self, builder):
        builder.loop_continue(lineno=self.lineno)


class cg1Assert(cg1CodeNode):
    _fields = ["test"]

    def __init__(self, test, **kwargs):
        super(cg1Assert, self).__init__(**kwargs)
        self.test = test
        
    def build(self, builder):
        test = self.test.build(builder)
        builder.assertion(test, lineno=self.lineno)


class cg1Assign(cg1CodeNode):
    _fields = ["target", "value"]

    def __init__(self, target, value, **kwargs):
        super(cg1Assign, self).__init__(**kwargs)
        self.target = target
        self.value = value

    def build(self, builder):
        target = self.target.build(builder)
        value = self.value.build(builder)

        return builder.assign(target, value, lineno=self.lineno)


class cg1AugAssign(cg1CodeNode):
    _fields = ["op", "target", "value"]

    def __init__(self, op, target, value, **kwargs):
        super(cg1AugAssign, self).__init__(**kwargs)
        self.op = op
        self.target = target
        self.value = value

    def build(self, builder):
        target = self.target.build(builder)

        value = self.value.build(builder)
    
        builder.augassign(self.op, target, value, lineno=self.lineno)


class cg1Attribute(cg1CodeNode):
    _fields = ["obj", "attr", "load"]
    def __init__(self, obj, attr, load=True, **kwargs):
        super(cg1Attribute, self).__init__(**kwargs)

        self.obj = obj
        self.attr = attr
        self.load = load
        
    def build(self, builder, depth=0):
        if depth == 0:
            builder.start_lookup(lineno=self.lineno)

        target = self.obj.build(builder, depth=depth + 1)
        builder.add_lookup(self.attr, is_attr=True, lineno=self.lineno)      

        if depth == 0:
            return builder.finish_lookup(target, is_attr=True, lineno=self.lineno)

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

    def build(self, builder, depth=0):
        if depth == 0:
            builder.start_lookup(lineno=self.lineno)

        target = self.target.build(builder, depth=depth + 1)
        index = self.index.build(builder)

        builder.add_lookup(index, lineno=self.lineno)      

        if depth == 0:
            return builder.finish_lookup(target, load=self.load, lineno=self.lineno)

        return target


class cg1StrLiteral(cg1CodeNode):
    _fields = ["s"]

    def __init__(self, s, **kwargs):
        super(cg1StrLiteral, self).__init__(**kwargs)
        self.s = s

    def build(self, builder):
        return builder.add_string(self.s, lineno=self.lineno)


class CodeGenPass1(ast.NodeVisitor):
    def __init__(self):
        self._declarations = {
            'Number': self._handle_Number,
            'Fixed16': self._handle_Fixed16,
            'String': self._handle_String,
            # 'Array': self._handle_Array,
            'Struct': self._handle_Struct,
            'PixelArray': self.create_GenericObject,
            'Palette': self.create_GenericObject,
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
        
        self.in_func = False

        return cg1Func(node.name, params, body, decorators, lineno=node.lineno)

    def visit_Return(self, node):
        if node.value == None:
            value = cg1ConstInt32(0, lineno=node.lineno)

        else:
            value = self.visit(node.value)

        return cg1Return(value, lineno=node.lineno)

    def _handle_Number(self, node):
        keywords = {}
        for kw in node.keywords:
            keywords[kw.arg] = kw.value.value

        if len(node.args) > 0:
            keywords['init_val'] = node.args[0].n

        return cg1DeclareVar(type="i32", keywords=keywords, lineno=node.lineno)

    def _handle_Fixed16(self, node):
        keywords = {}
        for kw in node.keywords:
            keywords[kw.arg] = kw.value.value

        if len(node.args) > 0:
            keywords['init_val'] = int(node.args[0].n * 65536) # convert to fixed16

        return cg1DeclareVar(type="f16", keywords=keywords, lineno=node.lineno)

    def _handle_String(self, node):
        keywords = {}
        for kw in node.keywords:
            keywords[kw.arg] = kw.value.value

        if len(node.args) == 0:
            keywords['length'] = 1
            keywords['init_val'] = cg1StrLiteral('\0', lineno=node.lineno)

        else:
            if isinstance(node.args[0], ast.Str):
                keywords['length'] = len(node.args[0].s)
                keywords['init_val'] = self.visit(node.args[0])

            else:
                keywords['length'] = node.args[0].n
                keywords['init_val'] = cg1StrLiteral('\0' * keywords['length'], lineno=node.lineno)

        return cg1DeclareStr(type="str", keywords=keywords, lineno=node.lineno)

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

    def create_GenericObject(self, node):
        args = [self.visit(a) for a in node.args]
        kwargs = {}

        for kw in node.keywords:
            field_name = kw.arg
            field_type = self.visit(kw.value)

            kwargs[field_name] = field_type

        return cg1GenericObject(node.func.id, args, kwargs, lineno=node.lineno)

    def visit_Call(self, node):
        if node.func.id in self._declarations:
            return self._declarations[node.func.id](node)

        elif node.func.id in self._struct_types:
            struct_type = self._struct_types[node.func.id]

            return cg1DeclareStruct(struct=struct_type, lineno=node.lineno)

        elif self.in_func:
            kwargs = {}
            for kw in map(self.visit, node.keywords):
                kwargs.update(kw)

            args = list(map(self.visit, node.args))
            return cg1Call(node.func.id, args, kwargs, lineno=node.lineno)

        else:
            # function call at module level
            return cg1Call(node.func.id, list(map(self.visit, node.args)), lineno=node.lineno)

    def visit_Yield(self, node):
        return cg1Call('yield',[], lineno=node.lineno)
        
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
        arg_type = node.annotation.id

        datatype = None
        if arg_type == 'Number':
            datatype = 'i32'

        elif arg_type == 'Fixed16':
            datatype = 'f16'

        return cg1Var(node.arg, datatype, lineno=node.lineno)

    def visit_BinOp(self, node):
        return cg1BinOpNode(self.visit(node.op), self.visit(node.left), self.visit(node.right), lineno=node.lineno)

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

    def generic_visit(self, node):
        raise NotImplementedError(node)

def parse(source):
    return ast.parse(source)

def compile_text(source, debug_print=False, summarize=False, script_name=''):
    import colored_traceback
    colored_traceback.add_hook()
    
    tree = parse(source)

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
        ir_program.analyze()

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

    # if isinstance(e, CompilerFatal):
        # raise e

    # save IR to file
    with open(f'{script_name}.fxir', 'w') as f:
        try:
            f.write(str(ir_program))

            if ins_program:
                f.write(str(ins_program))

        except AttributeError:
            if e:
                raise e

            raise

    # if debug_print:
    #     print(ir_program)
        
    #     if ins_program:
    #         print(ins_program)

    if e:
        raise e

    if ins_program:
        for func in ins_program.funcs.values():
            ret_val = func.run()
            print(f'VM returned: {ret_val}')

    return ins_program

    
    
    builder.allocate()
    builder.generate_instructions()

    if debug_print:
        builder.print_instructions()
        builder.print_data_table()
        # builder.print_control_flow()

    builder.assemble()
    builder.generate_binary()

    if debug_print:
        builder.print_func_addrs()

    if debug_print or summarize:
        print("VM ISA:  %d" % (VM_ISA_VERSION))
        print("Program name: %s Hash: 0x%08x" % (builder.script_name, builder.header.program_name_hash))
        print("Stream length:   %d bytes"   % (len(builder.stream)))
        print("Code length:     %d bytes"   % (builder.header.code_len))
        print("Data length:     %d bytes"   % (builder.header.data_len))
        print("Functions:       %d"         % (len(builder.funcs)))
        print("Read keys:       %d"         % (len(builder.read_keys)))
        print("Write keys:      %d"         % (len(builder.write_keys)))
        print("Published vars:  %d"         % (builder.published_var_count))
        print("Pixel arrays:    %d"         % (len(builder.pixel_arrays)))
        print("Links:           %d"         % (len(builder.links)))
        print("DB entries:      %d"         % (len(builder.db_entries)))
        print("Cron entries:    %d"         % (len(builder.cron_tab)))
        print("Stream hash:     0x%08x"     % (builder.stream_hash))

    return builder


def compile_script(path, debug_print=False):
    script_name = os.path.split(path)[1]

    logging.info(f'Compiling {script_name}')

    with open(path) as f:
        return compile_text(f.read(), script_name=script_name, debug_print=debug_print)


def main():
    path = sys.argv[1]
    script_name = os.path.split(path)[1]

    setup_basic_logging(show_thread=False)

    logging.info(f'Compiling: {script_name}')

    try:
        program = compile_script(path, debug_print=True)
       
        return

        try:
            output_path = sys.argv[2]
            with open(output_path, 'w+') as f:
                f.write(program.stream)

        except IndexError:
            pass

    except IOError:
        # path was a directory

        # compile and summarize all files
        stats = {}

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
                    builder = compile_text(text, summarize=False)

                    stats[fpath] = {'code': builder.header.code_len,
                                    'data': builder.header.data_len,
                                    'stream': len(builder.stream)}


                except SyntaxError as e:
                    print("SyntaxError:", e)

                except Exception as e:
                    print("Exception:", e)

        print('')
        for param in ['code', 'data', 'stream']:
            highest = 0
            name = ''
            for script in stats:
                if stats[script][param] > highest:
                    highest = stats[script][param]
                    name = script

            print("Largest %8s size: %32s %5d bytes" % (param, name, highest))


profile = False
if __name__ == '__main__':

    if profile:
        import yappi
        yappi.start()

    main()

    if profile:            # try:

        yappi.get_func_stats().print_all()
