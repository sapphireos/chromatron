import pprint
import os
import ast
import sys
from textwrap import dedent

from ir import *


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


class cg1DeclarationBase(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, name="<anon>", type="i32", length=1, **kwargs):
        super(cg1DeclarationBase, self).__init__(**kwargs)

        self.name = name
        self.type = type
        self.length = length

    def build(self, builder):
        return builder.add_local(self.name, self.type, self.length, lineno=self.lineno)

class cg1DeclareVar(cg1DeclarationBase):
    def __init__(self, **kwargs):
        super(cg1DeclareVar, self).__init__(**kwargs)

        assert self.length == 1

class cg1DeclareArray(cg1DeclarationBase):
    def __init__(self, **kwargs):
        super(cg1DeclareArray, self).__init__(**kwargs)

        assert self.length > 0


class cg1DeclareRecord(cg1DeclarationBase):
    def __init__(self, record=None, **kwargs):
        super(cg1DeclareRecord, self).__init__(**kwargs)

        self.type = record.name
        self.record = record

        self.length = self.record.length


class cg1RecordType(cg1Node):
    _fields = ["name", "fields"]

    def __init__(self, name="<anon>", fields={}, **kwargs):
        super(cg1RecordType, self).__init__(**kwargs)

        self.name = name
        self.fields = fields

        self.length = 0

        for field in self.fields.values():
            self.length += field.length

    def build(self, builder):
        fields = {k: {'type':v.type, 'length':v.length} for (k, v) in self.fields.items()}
        return builder.create_record(self.name, fields, lineno=self.lineno)


class cg1Var(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, name="<anon>", **kwargs):
        super(cg1Var, self).__init__(**kwargs)

        self.name = name
        self.type = None
        self.length = 1

    def build(self, builder):
        return builder.get_var(self.name, self.lineno)
    
    def __str__(self):
        return "cg1Var %s %s" % (self.name, self.type)


class cg1ObjVar(cg1Var):
    def __init__(self, *args, **kwargs):
        super(cg1ObjVar, self).__init__(*args, **kwargs)
        self.type = 'obj'

        toks = self.name.split('.')
        self.obj = toks[0]
        self.attr = toks[1]

    def build(self, builder):
        return builder.get_obj_var(self.obj, self.attr, self.lineno)


class cg1VarInt32(cg1Var):
    def __init__(self, *args, **kwargs):
        super(cg1VarInt32, self).__init__(*args, **kwargs)
        self.type = 'i32'

class cg1ConstInt32(cg1VarInt32):
    def build(self, builder):
        return builder.add_const(self.name, self.type, self.length, lineno=self.lineno)
    

class cg1Module(cg1Node):
    _fields = ["name", "body"]

    def __init__(self, name, body, **kwargs):
        super(cg1Module, self).__init__(**kwargs)
        self.name = name
        self.body = body
        self.module = None
        self.ctx = {}

    def build(self, builder=None):
        if builder == None:
            builder = Builder()

        # collect everything at module level that is not part of a function
        startup_code = [a for a in self.body if not isinstance(a, cg1Func)]

        for node in startup_code:
            # assign global vars to table
            # if isinstance(node, cg1DeclareRecord):
                # node.build_global(builder)

            if isinstance(node, cg1DeclarationBase):
                builder.add_global(node.name, node.type, node.length, lineno=node.lineno)

            elif isinstance(node, cg1RecordType):
                # builder.add_type(node.name, node.build(builder), lineno=node.lineno)
                node.build(builder)


        # collect funcs
        funcs = [a for a in self.body if isinstance(a, cg1Func)]

        for code in funcs:
            code.build(builder)

        return builder

class cg1NoOp(cg1CodeNode):
    def build(self, builder):
        return builder.nop(lineno=self.lineno)

class cg1Func(cg1CodeNode):
    _fields = ["name", "params", "body"]

    def __init__(self, name, params, body, **kwargs):
        super(cg1Func, self).__init__(**kwargs)
        self.name = name
        self.params = params
        self.body = body

    def build(self, builder):
        func = builder.func(self.name, lineno=self.lineno)

        for p in self.params:
            arg = builder.add_local(p.name, p.type, p.length, lineno=self.lineno)
            builder.add_func_arg(func, arg)


        for node in self.body:
            node.build(builder)

        # check if we need a default return
        if not isinstance(self.body[-1], cg1Return):
            ret = cg1Return(cg1ConstInt32(0, lineno=self.lineno), lineno=self.lineno)
            ret.build(builder)

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

    def __init__(self, target, params, **kwargs):
        super(cg1Call, self).__init__(**kwargs)
        self.target = target
        self.params = params

    def build(self, builder):
        params = [p.build(builder) for p in self.params]

        return builder.call(self.target, params, lineno=self.lineno)

class cg1Assign(cg1CodeNode):
    _fields = ["target", "value"]

    def __init__(self, target, value, **kwargs):
        super(cg1Assign, self).__init__(**kwargs)
        self.target = target
        self.value = value

    def build(self, builder):
        # check is assigning to an indexed item
        if isinstance(self.target, cg1Subscript):
            target = self.target.build(builder, store=True)

        else:
            target = self.target.build(builder)

        value = self.value.build(builder)

        return builder.assign(target, value, lineno=self.lineno)


class cg1If(cg1CodeNode):
    _fields = ["test", "body", "orelse"]

    def __init__(self, test, body, orelse, **kwargs):
        super(cg1If, self).__init__(**kwargs)
        self.test = test
        self.body = body
        self.orelse = orelse

    def build(self, builder):
        test = self.test.build(builder)

        body_label, else_label = builder.ifelse(test, lineno=self.lineno)

        builder.position_label(body_label)    
        for node in self.body:
            node.build(builder)

        builder.position_label(else_label)
        for node in self.orelse:
            node.build(builder)


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


class cg1AugAssign(cg1CodeNode):
    _fields = ["op", "target", "value"]

    def __init__(self, op, target, value, **kwargs):
        super(cg1AugAssign, self).__init__(**kwargs)
        self.op = op
        self.target = target
        self.value = value

    def build(self, builder):
        if isinstance(self.target, cg1Subscript):
            target = self.target.build(builder, store=True)

        else:
            target = self.target.build(builder)

        value = self.value.build(builder)
    
        builder.augassign(self.op, target, value, lineno=self.lineno)
        

class cg1Subscript(cg1CodeNode):
    _fields = ["target", "index"]

    def __init__(self, target, index, **kwargs):
        super(cg1Subscript, self).__init__(**kwargs)
        self.target = target
        self.index = index


    def build(self, builder, store=False):
        index = self.index.build(builder)
        target = self.target.build(builder)

        if store:
            return builder.index(target, index, load=False, lineno=self.lineno)

        else:
            return builder.index(target, index, lineno=self.lineno)


class CodeGenPass1(ast.NodeVisitor):
    def __init__(self):
        self._declarations = {
            'Number': self._handle_Number,
            'Array': self._handle_Array,
            'Record': self._handle_Record,
        }

        self._record_types = {}

    def __call__(self, source):
        # remove leading indentation
        source = dedent(source)

        self._source = source
        self._ast = ast.parse(source)

        return self.visit(self._ast)

    def visit_Module(self, node):
        body = map(self.visit, node.body)
        
        return cg1Module("module", body, lineno=0)

    def visit_FunctionDef(self, node):
        body = map(self.visit, list(node.body))
        params = map(self.visit, node.args.args)
        return cg1Func(node.name, params, body, lineno=node.lineno)

    def visit_Return(self, node):
        return cg1Return(self.visit(node.value), lineno=node.lineno)

    def _handle_Number(self, node):
        return cg1DeclareVar(type="i32", lineno=node.lineno)

    def _handle_Array(self, node):
        length = node.args[0].n
        return cg1DeclareArray(type="i32", length=length, lineno=node.lineno)

    def _handle_Record(self, node):
        fields = {}

        for kw in node.keywords:
            field_name = kw.arg
            field_type = self.visit(kw.value)

            fields[field_name] = field_type

        return cg1RecordType(fields=fields, lineno=node.lineno)

    def visit_Call(self, node):
        if node.func.id in self._declarations:
            return self._declarations[node.func.id](node)

        elif node.func.id in self._record_types:
            record_type = self._record_types[node.func.id]

            return cg1DeclareRecord(record=record_type, lineno=node.lineno)

        else:
            return cg1Call(node.func.id, map(self.visit, node.args), lineno=node.lineno)

    def visit_If(self, node):
        return cg1If(self.visit(node.test), map(self.visit, node.body), map(self.visit, node.orelse), lineno=node.lineno)
    
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

        elif isinstance(value, cg1RecordType):
            value.name = target.name
            self._record_types[value.name] = value

            return value

        else:
            return cg1Assign(target, value, lineno=node.lineno)

    def visit_AugAssign(self, node):
        return cg1AugAssign(self.visit(node.op), self.visit(node.target), self.visit(node.value), lineno=node.lineno)

    def visit_Num(self, node):
        if isinstance(node.n, int):
            return cg1ConstInt32(node.n, lineno=node.lineno)

        elif isinstance(node.n, float):
            # convert to int
            return cg1ConstInt32(int(node.n * 65535), lineno=node.lineno)

        else:
            raise NotImplementedError(node)    
    
    def visit_Name(self, node):
        return cg1VarInt32(node.id, lineno=node.lineno)

    def visit_BinOp(self, node):
        return cg1BinOpNode(self.visit(node.op), self.visit(node.left), self.visit(node.right), lineno=node.lineno)


    def visit_Add(self, node):
        return "add"

    def visit_Sub(self, node):
        return "sub"

    def visit_Mul(self, node):
        return "mul"

    def visit_Div(self, node):
        return "div"

    def visit_Mod(self, node):
        return "mod"

    def visit_Lt(self, node):
        return "lt"

    def visit_Gt(self, node):
        return "gt"

    def visit_Eq(self, node):
        return "eq"

    def visit_Expr(self, node):
        return self.visit(node.value)

    def visit_For(self, node):
        # Check for an else clause.  Python has an atypical else construct
        # you can use at the end of a for loop.  But it is confusing and
        # rarely used, so we are not going to support it.
        assert len(node.orelse) == 0

        return cg1For(self.visit(node.target), self.visit(node.iter), map(self.visit, node.body), lineno=node.lineno)

    def visit_Attribute(self, node):
        name = '%s.%s' % (node.value.id, node.attr)

        return cg1ObjVar(name, lineno=node.lineno)

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
        return cg1Subscript(self.visit(node.value), self.visit(node.slice), lineno=node.lineno)

    def generic_visit(self, node):
        raise NotImplementedError(node)




with open('cg2_test.fx') as f:
    source = f.read()


# with open('rainbow2.fx') as f:
    # source = f.read()

tree = ast.parse(source)

print pformat_ast(tree)

print '\n'

cg1 = CodeGenPass1()

cg1_data = cg1(source)

print pformat_ast(cg1_data)

try:
    builder = cg1_data.build()

    print builder

except SyntaxError as e:
    print e




