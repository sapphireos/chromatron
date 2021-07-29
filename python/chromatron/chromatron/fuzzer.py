
from .code_gen import compile_text

from random import randint


class Block(object):
	block_num = 0
	def __init__(self, min_length=0, max_length=3, target_depth=None, depth=None):
		self.indent = True
		self.target_length = randint(min_length, max_length)
		self.target_depth = target_depth
		self.depth = depth

		self.blocks = []	
		self.block_num = Block.block_num
		Block.block_num += 1

	def __str__(self):
		tab = '\t'
		s = f'{tab * self.depth}Block {self.block_num}: {self.count}/{self.total} @ {self.depth}\n'

		for block in self.blocks:
			s += f'{block}\n'

		return s

	@property
	def count(self):
		return len(self.blocks)

	@property
	def total(self):
		c = self.count
		for block in self.blocks:
			c += block.total

		return c

	def render(self):
		raise NotImplementedError

	def open(self):
		raise NotImplementedError

	def close(self):
		raise NotImplementedError

	def generate(self, current_total=0):
		current_depth = self.depth

		if current_depth >= self.target_depth:
			# max depth reached
			return

		if self.indent:
			current_depth += 1

		while self.total + current_total < self.target_length:
			b = Block(depth=current_depth, target_depth=self.target_depth)

			self.blocks.append(b) 

			b.generate(self.total)

class WhileLoop(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

class If(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

class IfElse(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)


class Func(Block):
	def __init__(self, min_depth=0, max_depth=1):
		super().__init__()

		self.depth = 0
		self.target_depth = randint(min_depth, max_depth)

	def __str__(self):
		s = f'Func: {self.count}/{self.total} @ {self.depth}\n'

		for block in self.blocks:
			s += f'{block}\n'

		return s

	def generate(self, length=None, depth=None):
		if length:
			self.target_length = length

		if depth:
			self.target_depth = depth

		super().generate()
		


def main():
	f = Func()

	f.generate(6, 3)

	print(f)

if __name__ == '__main__':
	main()
