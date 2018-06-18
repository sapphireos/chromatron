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

class cg1DeclareVar(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, name="<anon>", type="i32", length=1, **kwargs):
        super(cg1DeclareVar, self).__init__(**kwargs)

        self.name = name
        self.type = type
        self.length = length

    def build(self, builder):
        return builder.add_local(self.name, self.type, self.length, lineno=self.lineno)


class cg1Var(cg1Node):
    _fields = ["name", "type"]

    def __init__(self, name="<anon>", **kwargs):
        super(cg1Var, self).__init__(**kwargs)

        self.name = name
        self.type = None
        self.length = 1

    def build(self, builder):
        return builder.add_local(self.name, self.type, self.length, lineno=self.lineno)
    
    def __str__(self):
        return "cg1Var %s %s" % (self.name, self.type)


class cg1ObjVar(cg1Var):
    def __init__(self, *args, **kwargs):
        super(cg1ObjVar, self).__init__(*args, **kwargs)
        self.type = 'obj'

        toks = self.name.split('.')
        self.obj = toks[0]
        self.attr = toks[1]

    # def lookup(self, ctx):
    #     return ctx['objects'][self.obj][self.attr]

    # def load(self, ctx):
    #     var = self.lookup(ctx)

    #     return ctx['builder'].load(var)

    # def build(self, ctx):
    #     return self.lookup(ctx)

# class cg1LocalVar(cg1Var):
    # pass

class cg1VarInt32(cg1Var):
    def __init__(self, *args, **kwargs):
        super(cg1VarInt32, self).__init__(*args, **kwargs)
        self.type = 'i32'

class cg1ConstInt32(cg1VarInt32):
    pass
    

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
            if isinstance(node, cg1DeclareVar):
                builder.add_global(node.name, node.type, 1, lineno=self.lineno)

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

        func.params = [p.build(builder) for p in self.params]
        
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

    # def build(self, ctx):
    #     test = self.test.build(ctx)

    #     with ctx['builder'].if_else(test) as (then, otherwise):
    #         with then:
    #             for a in self.body:
    #                 a.build(ctx)

    #         with otherwise:
    #             for a in self.orelse:
    #                 a.build(ctx)


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


    # def build(self, ctx):
    #     left = self.left.load(ctx)
    #     right = self.right.load(ctx)

    #     if self.op == "add":
    #         result = ctx['builder'].add(left, right)

    #     elif self.op == "lt":
    #         result = ctx['builder'].icmp_signed("<", left, right)

    #     else:
    #         raise NotImplementedError

    #     return result

class cg1CompareNode(cg1BinOpNode):
    pass
        

class cg1For(cg1CodeNode):
    _fields = ["target", "iter", "body"]

    def __init__(self, target, iter, body, **kwargs):
        super(cg1For, self).__init__(**kwargs)
        self.target = target
        self.iter = iter
        self.body = body

    # def build(self, ctx):
    #     func = ctx['functions'][ctx['func']]
    #     init_block = func.append_basic_block('for.init')
    #     test_block = func.append_basic_block('for.test')
    #     body_block = func.append_basic_block('for.body')
    #     end_block = func.append_basic_block("for.end")

    #     # init iterator local variable and set to 0
    #     ctx['builder'].branch(init_block)
    #     ctx['builder'].position_at_end(init_block)
    #     declare_target = cg1DeclareVar(self.target.name)
    #     target = declare_target.build(ctx)
    #     ctx['builder'].store(cg1ConstInt32(0).build(ctx), target)

        
    #     # set up condition test
    #     ctx['builder'].branch(test_block)
    #     ctx['builder'].position_at_end(test_block)

    #     compare = cg1CompareNode('lt', self.target, self.iter)
    #     cond = compare.build(ctx)
        
    #     # conditional branch
    #     ctx['builder'].cbranch(cond, body_block, end_block)

    #     # loop body
    #     ctx['builder'].position_at_end(body_block)

    #     for a in self.body:
    #         a.build(ctx)

    #     # increment loop counter
    #     inc = ctx['builder'].add(cg1ConstInt32(1).build(ctx), ctx['builder'].load(target))
    #     ctx['builder'].store(inc, target)

    #     # branch back to top of loop
    #     ctx['builder'].branch(test_block)

    #     ctx['builder'].position_at_end(end_block)


class CodeGenPass1(ast.NodeVisitor):
    def __init__(self):
        pass

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

    def visit_Call(self, node):
        if node.func.id == "Number":
            return cg1DeclareVar(type="i32", lineno=node.lineno)

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

        if isinstance(value, cg1DeclareVar):
            value.name = target.name
            return value

        else:
            return cg1Assign(target, value, lineno=node.lineno)

    def visit_AugAssign(self, node):
        binop = cg1BinOpNode(self.visit(node.op), self.visit(node.target), self.visit(node.value))
        
        return cg1Assign(self.visit(node.target), binop, lineno=node.lineno)

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

        return cg1ObjVar(nam, lineno=node.lineno)

    def visit_Pass(self, node):
        return cg1NoOp(lineno=node.lineno)

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


builder = cg1_data.build()

print builder




