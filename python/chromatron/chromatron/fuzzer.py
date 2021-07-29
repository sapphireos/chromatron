
from .code_gen import compile_text

from random import randint

class Block(object):
	block_num = 0
	def __init__(self, min_length=0, max_length=4, target_depth=None, depth=None):
		self.indent = True
		self.target_length = randint(min_length, max_length)
		self.target_depth = target_depth
		self.depth = depth
		self.type = 'Block'

		self.blocks = []	
		self.block_num = Block.block_num
		Block.block_num += 1

		self.available_blocks = []

	def __str__(self):
		tab = '\t'
		s = f'{tab * self.depth}{self.type} {self.block_num}: {self.count}/{self.total}\n'

		for block in self.blocks:
			s += f'{block}'

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

	def choose_block(self):
		return self.available_blocks[randint(0, len(self.available_blocks) - 1)]

	def render(self):
		code = ''
		tab = '    '

		# code += f'{tab * self.depth}# {self.type}: {self.block_num}\n'
		code += f'{tab * self.depth}{self.open()}\n'

		for b in self.blocks:
			code += b.render()

		# code += f'{tab * self.depth}{self.close()}'

		return code

	def open(self):
		raise NotImplementedError

	def close(self):
		return ''

	def generate(self, current_total=0):
		current_depth = self.depth

		if self.indent:
			current_depth += 1

		if current_depth >= self.target_depth:
			# max depth reached
			return

		if len(self.available_blocks) == 0:
			return

		while self.total < self.target_length:
			b = self.choose_block()(depth=current_depth, target_depth=self.target_depth)

			self.blocks.append(b) 

			b.generate(self.total)

		if len(self.blocks) == 0:
			self.blocks.append(Pass(depth=current_depth))

class WhileLoop(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.type = 'While'

		self.available_blocks = [
			WhileLoop,
			If,
			# IfElse,	
			Statements,
		]

	def open(self):
		return 'while i > 0:'

class If(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.type = 'If'

		self.available_blocks = [
			WhileLoop,
			If,
			# IfElse,	
			Statements,
		]

	def open(self):
		return 'if True:'

class IfElse(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.type = 'IfElse'

class Statements(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.indent = False
		self.type = 'Statements'

	def generate(self, *args, **kwargs):
		pass

	def open(self):
		return 'pass'

class Pass(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.indent = False
		self.type = 'Pass'

	def generate(self, *args, **kwargs):
		pass

	def open(self):
		return 'pass'

class Func(Block):
	def __init__(self, min_depth=0, max_depth=1):
		super().__init__()

		self.depth = 0
		self.target_depth = randint(min_depth, max_depth)

		self.available_blocks = [
			WhileLoop,
			If,
			# IfElse,	
			Statements,
		]

	def __str__(self):
		s = f'Func: {self.count}/{self.total} @ {self.depth}\n'

		for block in self.blocks:
			s += f'{block}'

		return s

	def generate(self, length=None, depth=None):
		if length:
			self.target_length = length

		if depth:
			self.target_depth = depth

		super().generate()

	def open(self):
		return 'def func():'

# BLOCK_TYPES = [
# 	WhileLoop,
# 	If,
# 	# IfElse,	
# 	# Statements,
# ]

# def choose_block():
# 	return BLOCK_TYPES[randint(0, len(BLOCK_TYPES) - 1)]
		


def main():
	f = Func()

	f.generate(16, 5)

	print(f)
	print(f.render())

if __name__ == '__main__':
	main()
