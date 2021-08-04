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
	def __init__(self):
		self.depth = 0

	def __str__(self):
		s = f'{TAB * self.depth}{str(self.__class__.__name__)}\n'

		return s

	@property
	def count(self):
		return 1

class Block(Element):
	def __init__(self, *args):
		super().__init__()

		self.elements = []

		if len(args) == 0:
			args = [Block]

		self.selector = Selector(*args)

	def __str__(self):
		s = f'{TAB * self.depth}{str(self.__class__.__name__)}\n'

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

	def generate(self, max_length=6, max_depth=2):
		if self.depth < max_depth:			
			while self.count < max_length:
				e = self.select()

				e.generate(max_length - self.count, max_depth)

				self.add_element(e)

		if len(self.elements) == 0:
			p = Pass()
			p.depth = self.depth + 1
			self.add_element(p)


class Pass(Element):
	pass

class While(Block):
	def __init__(self):
		super().__init__(While, If)

class If(Block):
	def __init__(self):
		super().__init__(While, If)

class Func(Block):
	def __init__(self):
		super().__init__(While, If)



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
