import sys
import os

from .code_gen import parse, compile_text
from random import randint
from copy import copy


# TAB = ' ' * 4
TAB = '---|'

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


class Expr(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.op = '?'
		self.var1 = self.get_var()
		self.var2 = self.get_var()

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

	def __str__(self):
		s = f'{TAB * self.depth}{self.var} = {self.expr}\n'

		return s

class Variable(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var = self.get_var()

	def __str__(self):
		s = f'{TAB * self.depth}{self.var}\n'

		return s

class Pass(Element):
	pass


class Return(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.expr = Selector(ArithVars, Variable).select()

	def __str__(self):
		s = f'{TAB * self.depth}{self.get_name()} {self.expr}\n'

		return s


class ControlFlow(Block):
	def __init__(self):
		super().__init__(While, If, Assign, Return)

		self.expr = CompareVars()

	def get_name(self):
		return f'{super().get_name()} {self.expr}'

class While(ControlFlow):
	pass

class If(ControlFlow):
	pass

class ElIf(Block):
	pass

class Else(Block):
	pass

class Func(Block):
	def __init__(self):
		super().__init__(While, If)

	def generate(self, max_length=12, max_depth=3, max_vars=4):
		Element.max_vars = max_vars

		super().generate(max_length, max_depth, randomize=False)


def main():
	f = Func()
	f.generate()

	# b.add_element()
	# b.add_element()

	print(f)
	print(f.count)
	# print(b.depth)


if __name__ == '__main__':
	try:
		main()

	except KeyboardInterrupt:
		pass
