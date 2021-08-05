import sys
import os

from .code_gen import parse, compile_text
from random import randint
from copy import copy


TAB = ' ' * 4
# TAB = '---|'

class Selector(object):
	def __init__(self, *args):
		self.items = args

	def select(self):
		return self.items[randint(0, len(self.items) - 1)]()


class Element(object):
	max_vars = 1
	variables = []

	def __init__(self):
		self.depth = 0
		self.code = ''
		self.render_newline = True

	def __str__(self):
		s = f'{TAB * self.depth}{self.get_name()}\n'

		return s

	def get_name(self):
		return str(self.__class__.__name__)

	@property
	def count(self):
		return 1

	def generate(self, *args, **kwargs):
		pass

	def render(self):
		s = f'{TAB * self.depth}{self.code}'

		if self.render_newline:
			s += '\n'

		return s

	def get_var(self):
		try:
			return self.variables[randint(0, self.max_vars - 1)]

		except IndexError:
			new_var = chr(len(self.variables) + 97)
			self.variables.append(new_var)
			return new_var


class Block(Element):
	def __init__(self, *args):
		super().__init__()

		self.elements = []

		if len(args) == 0:
			args = [Block]

		self.selector = Selector(*args)

	def __str__(self):
		s = f'{TAB * self.depth}{self.get_name()}\n'

		for e in self.elements:
			s += f'{e}'

		return s

	@property
	def count(self):
		c = 1

		for e in self.elements:
			c += e.count

		return c

	def add_element(self, element):
		self.elements.append(element)

	def select(self):
		e = self.selector.select()
		e.depth = self.depth + 1

		return e

	def generate(self, max_length=6, max_depth=2, randomize=True):
		if randomize:
			max_length = randint(1, max_length)
			max_depth = randint(1, max_depth)

		if self.depth < max_depth:			
			while self.count < max_length:
				e = self.select()

				e.generate(max_length - self.count, max_depth)

				self.add_element(e)

		if len(self.elements) == 0:
			p = Pass()
			p.depth = self.depth + 1
			self.add_element(p)

	def render(self):
		s = super().render()

		for e in self.elements:
			s += f'{e.render()}'

		return s

class Expr(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.op = '?'
		self.var1 = self.get_var()
		self.var2 = self.get_var()
		self.code = f'{self.var1} {self.op} {self.var2}'
		self.render_newline = False

	def __str__(self):
		# s = f'{TAB * self.depth}Expr({self.var1} {self.op} {self.var2})\n'
		s = f'{self.var1} {self.op} {self.var2}'

		return s

class CompareVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.op = '?'

class ArithVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.op = '#'


class Assign(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var = self.get_var()
		self.expr = ArithVars()
		self.code = f'{self.var} = {self.expr}'

	def __str__(self):
		s = f'{TAB * self.depth}{self.var} = {self.expr}\n'

		return s

class Variable(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var = self.get_var()
		self.code = f'{self.var}'
		self.render_newline = False

	def __str__(self):
		s = f'{TAB * self.depth}{self.var}'

		return s

class DeclareVar(Element):
	def __init__(self, var, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var = var

		self.code = f'{var} = Number()'

	def __str__(self):
		s = f'{TAB * self.depth}{self.var} = Number()\n'

		return s

class Pass(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = 'pass'


class Return(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.expr = Selector(ArithVars, Variable).select()

		self.code = f'return {self.expr.render()}'

	def __str__(self):
		s = f'{TAB * self.depth}{self.get_name()} {self.expr}\n'

		return s


class ControlFlow(Block):
	def __init__(self):
		super().__init__(While, IfBlock, Assign, Return)

		self.expr = Selector(CompareVars, Variable).select()

	def get_name(self):
		return f'{super().get_name()} {self.expr}'

class While(ControlFlow):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = f'while {self.expr.render()}:'	

class IfBlock(Block):
	def __init__(self):
		super().__init__()

		self.if_block = None
		
		self.elifs = []
		self.else_block = None

	def __str__(self):
		s = f'{self.if_block}'

		for e in self.elifs:
			s += f'{e}'

		if self.else_block:
			s += f'{self.else_block}'

		return s


	@property
	def count(self):
		c = 0 
		if self.if_block:
			c += self.if_block.count

		for e in self.elifs:
			c += e.count

		if self.else_block:
			c += self.else_block.count

		return c
	def generate(self, max_length=6, max_depth=2, **kwargs):
		self.if_block = If()
		self.if_block.depth = self.depth
		self.if_block.generate(max_length=max_length, max_depth=max_depth, **kwargs)
		
		while self.count < max_length:
			if randint(0, 2) == 1:
				elif_block = ElIf()
				elif_block.depth = self.depth
				self.elifs.append(elif_block)
				elif_block.generate(max_length=max_length, max_depth=max_depth, **kwargs)

			else:
				break

		if self.count < max_length:	
			if randint(0, 1) == 1:
				else_block = Else()
				else_block.depth = self.depth
				self.else_block = else_block
				else_block.generate(max_length=max_length, max_depth=max_depth, **kwargs)

	def render(self):
		s = self.if_block.render()

		for e in self.elifs:
			s += e.render()

		if self.else_block:
			s += self.else_block.render()

		return s

class If(ControlFlow):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = f'if {self.expr.render()}:'	

class ElIf(ControlFlow):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = f'elif {self.expr.render()}:'	

class Else(ControlFlow):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = f'else:'	

class Func(Block):
	def __init__(self):
		super().__init__(While, IfBlock, Assign, Return)
		
		self.code = 'def func():'

	def generate(self, max_length=24, max_depth=3, max_vars=4):
		Element.max_vars = max_vars

		super().generate(max_length, max_depth, randomize=False)

		for var in self.variables:
			declare = DeclareVar(var)
			declare.depth = self.depth + 1

			self.elements.insert(0, declare)

def main():
	f = Func()
	f.generate()

	# b.add_element()
	# b.add_element()

	# print(f)
	# print(f.count)
	# print(b.depth)
	print(f.render())

if __name__ == '__main__':
	try:
		main()

	except KeyboardInterrupt:
		pass
