
from .code_gen import parse, compile_text

from random import randint

class Element(object):
	def __init__(self, depth=None):
		self.depth = depth
		self.available_elements = []

	def choose_element(self):
		return self.available_elements[randint(0, len(self.available_elements) - 1)]

class Block(Element):
	block_num = 0
	
	def __init__(self, min_length=0, max_length=4, target_depth=None, depth=None):
		super().__init__(depth=depth)
		self.indent = True
		self.target_length = randint(min_length, max_length)
		self.target_depth = target_depth
		self.type = 'Block'

		self.blocks = []	
		self.block_num = Block.block_num
		Block.block_num += 1

	def __str__(self):
		tab = '    '
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
			if len(self.blocks) == 0:
				self.blocks.append(Pass(depth=current_depth))

			return

		if len(self.available_elements) == 0:
			return

		while self.total < self.target_length:
			b = self.choose_element()(depth=current_depth, target_depth=self.target_depth)

			self.blocks.append(b) 

			b.generate(self.total)

		if len(self.blocks) == 0:
			self.blocks.append(Pass(depth=current_depth))

class WhileLoop(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.type = 'While'

		self.available_elements = [
			WhileLoop,
			If,
			# IfElse,	
			Statements,
		]

		self.compare = CompareVars(depth=self.depth)

	def open(self):
		return f'while {self.compare.render()}:'

class If(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.type = 'If'

		self.available_elements = [
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

class Pass(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.indent = False
		self.type = 'Pass'

	def generate(self, *args, **kwargs):
		pass

	def open(self):
		return 'pass'


class Statements(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.indent = False
		self.type = 'Statements'

		self.statements = []

		self.available_elements = [
			DeclareVar,
			Assign,
		]

	def __str__(self):
		tab = '    '
		s = ''
		for i in self.statements:
			s = f'{i}\n'

		return s

	def generate(self, *args, **kwargs):
		self.statements.append(self.choose_element()(depth=self.depth))

	def open(self):
		s = ''

		for i in self.statements:
			s += i.render()

		return s

class Statement(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'Statement'

	def __str__(self):
		tab = '    '
		s = f'{tab * self.depth}{self.type}'

		return s

	def render(self):
		raise NotImplementedError

class DeclareVar(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'DeclareVar'

	def render(self):
		return 'a = Number()'

class Expr(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'Expr'

		self.op = '?'
		self.var1 = 'a?'
		self.var2 = 'b?'

	def render(self):
		return f'{self.var1} {self.op} {self.var2}'

class CompareVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'CompareVars'

		self.op = '>'
		self.var1 = 'a'
		self.var2 = 'b'

class ArithVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'ArithVars'

		self.op = '+'
		self.var1 = 'a'
		self.var2 = 'b'

class Assign(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.available_elements = [
			ArithVars,
			CompareVars,
		]

		self.type = 'Assign'
		self.var = 'a'
		self.expr = self.choose_element()(depth=self.depth)

	def render(self):
		return f'{self.var} = {self.expr.render()}'

class Func(Block):
	def __init__(self, min_depth=0, max_depth=1):
		super().__init__()

		self.depth = 0
		self.target_depth = randint(min_depth, max_depth)

		self.available_elements = [
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



def main():
	f = Func()

	f.generate(16, 5)

	print(f)
	code = f.render()
	print(code)
	parse(code) # check if parseable


	# execution:
	# need a variable manager to ensure variables are declared
	# variables passed down tree, queries go up tree
	# declare var on use if not already declared

	# need to ensure loops always terminate (yay halting problem!)
	# but since we are controlling input, we can manipulate loop inits
	# we can also check for halting by just killing the program after some
	# amount of time.
	# could also instrument loops to add a counter and raise exception if exceeded.
	# this instrumentation would be generated only on the python check program.
	

if __name__ == '__main__':
	main()
