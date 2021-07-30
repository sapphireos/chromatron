
import sys
import os


from .code_gen import parse, compile_text
from random import randint
from copy import copy



class Element(object):
	def __init__(self, depth=None, target_vars=1, variables={}):
		self.depth = depth
		self.available_elements = []
		self.target_vars = target_vars
		self.variables = variables

	def choose_element(self):
		return self.available_elements[randint(0, len(self.available_elements) - 1)]

	def get_var(self):
		try:
			return list(self.variables.values())[randint(0, self.target_vars - 1)]

		except IndexError:
			new_var = chr(len(self.variables) + 97)
			self.variables[new_var] = new_var
			return new_var


class Block(Element):
	block_num = 0
	
	def __init__(self, 
				 min_length=0, 
				 max_length=4, 
				 target_depth=None, 
				 **kwargs):

		super().__init__(**kwargs)

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

	def generate(self, current_total=0, target_vars=None):
		current_depth = self.depth

		if target_vars is not None:
			self.target_vars = target_vars

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
			b = self.choose_element()(depth=current_depth, target_depth=self.target_depth, variables=copy(self.variables), target_vars=self.target_vars)

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

		self.compare = CompareVars(depth=self.depth, target_vars=self.target_vars)

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

		self.compare = CompareVars(depth=self.depth, target_vars=self.target_vars)

	def open(self):
		return f'if {self.compare.render()}:'

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
			Assign,
			AugAssign,
			Return,
		]

	def __str__(self):
		tab = '    '
		s = ''
		for i in self.statements:
			s = f'{i}\n'

		return s

	def generate(self, *args, **kwargs):
		self.statements.append(self.choose_element()(depth=self.depth, target_vars=self.target_vars))

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
	def __init__(self, var, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var = var
		self.type = 'DeclareVar'

	def render(self):
		return f'{self.var} = Number()'

class Return(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'Return'
		self.var = self.get_var()

	def render(self):
		return f'return {self.var}'

class Expr(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'Expr'

		self.op = '?'
		self.var1 = self.get_var()
		self.var2 = self.get_var()

	def render(self):
		return f'{self.var1} {self.op} {self.var2}'

class CompareVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'CompareVars'

		self.op = '>'
		
class ArithVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'ArithVars'

		self.op = '+'

class ConstVar(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'ConstVar'

		self.const = randint(0, 4)

	def render(self):
		return f'{self.const}'

class DeclaredVar(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'DeclaredVar'

		self.var = self.get_var()

	def render(self):
		return f'{self.var}'

class AugAssign(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.type = 'AugAssign'

		self.op = '+'

		self.var1 = self.get_var()
		self.var2 = self.get_var()

	def render(self):
		return f'{self.var1} {self.op}= {self.var2}'


class Assign(Statement):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.available_elements = [
			ArithVars,
			CompareVars,
			ConstVar,
			DeclaredVar,
		]

		self.type = 'Assign'
		self.var = self.get_var()
		self.expr = self.choose_element()(depth=self.depth, target_vars=self.target_vars)

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

		self.code = None

	def __str__(self):
		s = f'Func: {self.count}/{self.total} @ {self.depth}\n'

		for block in self.blocks:
			s += f'{block}'

		return s

	def render(self):
		if self.code is None:
			self.code = super().render()

			# ensure code is parsable
			parse(self.code)

		return self.code

	def generate(self, length=None, depth=None, max_vars=None):
		if length:
			self.target_length = length

		if depth:
			self.target_depth = depth

		super().generate(target_vars=max_vars)

	def open(self):
		s = 'def func():'

		tab = '    '
		for var in self.variables.values():
			declare = DeclareVar(var)
			s += f'\n{tab * (self.depth + 1)}{declare.render()}'

		return s

PY_HEADER = """
def Number():
	return 0

"""

PY_TRAILER = """
def main():
	func()

if __name__ == '__main__':
	main()

"""

def generate_python(func):
	code = func.render()

	py_code = PY_HEADER

	py_code += code

	py_code += PY_TRAILER

	return py_code

def generate_fx(func):
	code = func.render()

	return code


def run_py(func):
	py = generate_python(func)

	with open('_fuzz.py', 'w') as f:
		f.write(py)

	os.system(f'python3 _fuzz.py')


def compile_fx(func):
	fx = generate_fx(func)

	# with open('_fuzz.fx', 'w') as f:
		# f.write(fx)

	compile_text(fx)


def main():
	func = Func()

	func.generate(16, 5, 4)

	# print(func)
	code = func.render()
	# print(code)

	# py = generate_python(func)
	# with open('_fuzz.py', 'w') as f:
		# f.write(py)

	for i in range(10000):
		print(i)
		
		try:
			run_py(func)
			compile_fx(func)

		except Exception as e:
			print(func)
			print(code)
			raise

	return 

	# fx = generate_fx(func)

	with open('_fuzz.fx', 'w') as f:
		f.write(fx)

	return

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
	try:
		main()

	except KeyboardInterrupt:
		pass
