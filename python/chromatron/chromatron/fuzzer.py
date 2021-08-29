import sys
import os
from subprocess import STDOUT, check_output, TimeoutExpired
import math

from datetime import datetime
from .ir2 import DivByZero
from .instructions2 import MAX_INT32, MIN_INT32
from .code_gen import parse, compile_text
from random import randint
from copy import copy


TAB = ' ' * 4
# TAB = '---|'


variables = []
var_count = 1
consts = []
const_count = 8

class Selector(object):
	def __init__(self, *args):
		self.items = args

	def select(self):
		try:
			return self.items[randint(0, len(self.items) - 1)]()

		except TypeError:
			return self.items[randint(0, len(self.items) - 1)]


class Element(object):
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

	def get_var(self, allow_const=False):
		if allow_const:
			if randint(0, 1) == 1:
				return Const()

		return variables[randint(0, var_count - 1)]


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
		self.var1 = self.get_var(allow_const=True)
		self.var2 = self.get_var(allow_const=True)
		self.render_newline = False

	@property
	def code(self):
		return f'{self.var1} {self.op} {self.var2}'

	@code.setter
	def code(self, value):
		pass

	def __str__(self):
		# s = f'{TAB * self.depth}Expr({self.var1} {self.op} {self.var2})\n'
		s = f'{self.var1} {self.op} {self.var2}'

		return s

class CompareVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.op = Selector('<', '<=', '==', '>', '>=').select()

class ArithVars(Expr):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.op = Selector('+', '-', '*', '//', '%').select() # note the use of integer division

class Assign(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var = self.get_var()
		self.expr = Selector(ArithVars, CompareVars).select()
		self.code = f'{self.var} = {self.expr}'

	def __str__(self):
		s = f'{TAB * self.depth}{self.var} = {self.expr}\n'

		return s

class AugAssign(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.var1 = self.get_var()
		self.var2 = self.get_var()
		self.op = Selector('+', '-', '*', '//', '%').select() # note the use of integer division
		
		self.code = f'{self.var1} {self.op}= {self.var2}'

	def __str__(self):
		s = f'{TAB * self.depth}{self.var1} {self.op}= {self.var2}\n'

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

class Const(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)

		self.const = consts[randint(0, const_count - 1)]
		self.code = f'{self.const}'
		self.render_newline = False

	def __str__(self):
		s = f'{TAB * self.depth}{self.const}'

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

class Break(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = f'break'

	def __str__(self):
		s = f'{TAB * self.depth}break\n'

		return s

class Continue(Element):
	def __init__(self, *args, **kwargs):
		super().__init__(*args, **kwargs)
		self.code = f'continue'

	def __str__(self):
		s = f'{TAB * self.depth}continue\n'

		return s


class ControlFlow(Block):
	def __init__(self, *args, **kwargs):
		super().__init__(
			While,
			IfBlock,
			Assign,
			Assign,
			Assign,
			Assign,
			Assign,
			Assign,
			AugAssign,
			AugAssign,
			AugAssign,
			Return,
			*args, **kwargs)

		self.expr = Selector(CompareVars, Variable).select()

	def get_name(self):
		return f'{super().get_name()} {self.expr}'

class While(ControlFlow):
	def __init__(self):
		super().__init__(Break, Continue)
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
		super().__init__(While, IfBlock, Assign, AugAssign, Return)
		
		self.code = 'def func():'

		self.rendered_code = None

	def render(self):
		if self.rendered_code:
			return self.rendered_code

		self.rendered_code = super().render()
		return self.rendered_code

	def generate(self, max_length=256, max_depth=16, max_vars=16, max_consts=16):

		# global var handling.... not super happy about this but works for now
		global var_count
		global variables
		var_count = randint(1, max_vars)
		variables = []
		
		for i in range(var_count):
			variables.append(chr(97 + i))

		global const_count
		global consts
		const_count = randint(1, max_consts)
		consts = []

		for i in range(const_count):
			consts.append(randint(-100, 100))

		super().generate(max_length, max_depth, randomize=False)

		for var in variables:
			declare = DeclareVar(var)
			declare.depth = self.depth + 1

			self.elements.insert(0, declare)

		if randint(0, 1) == 1: 
			ret = Return()
			ret.depth = self.depth + 1
			self.elements.append(ret)


PY_HEADER = """
def Number():
    return 0

"""

PY_TRAILER = """
def main():
    try:
        return func()

    except ZeroDivisionError:
    	return "ZeroDivisionError"

if __name__ == '__main__':
    print(main())

"""

def generate_python(func):
	code = func.render()

	py_code = PY_HEADER

	py_code += code

	py_code += PY_TRAILER

	return py_code

def compile_fx(func):
	fx = func.render()

	with open('_fuzz.fx', 'w') as f:
		f.write(fx)

	return compile_text(fx)


def run_py(pycode, fname='_fuzz.py'):
	with open(fname, 'w') as f:
		f.write(pycode)

	# cmd = f'python3 _fuzz.py'
	# cmd = 'which python3'
	# os.system(cmd)
	output = check_output(['python3', fname], stderr=STDOUT, cwd=os.getcwd(), timeout=0.1)
	output = output.decode().strip()

	if output == 'ZeroDivisionError':
		raise ZeroDivisionError

	if output == 'None':
		return None
	elif output == 'True':
		return True
	elif output == 'False':
		return False
	
	try:
		return int(output)

	except ValueError:
		return float(output)


class NoneOutput(Exception):
	pass

def generate_valid_program(skip_exc=False):
	f = Func()
	f.generate()
	pycode = generate_python(f)

	output = None

	try:
		fname = f'_fuzz_{os.getpid()}.py'
		output = run_py(pycode, fname=fname)
		
		if output is None:
			raise NoneOutput

	except (ZeroDivisionError, TimeoutExpired, NoneOutput):
		os.remove(fname)
			
		if not skip_exc:
			raise

		return None, None

	output = int(math.floor(output)) # force floored int, this is important to get division to match

	# check output range
	# python supports large ints, but FX does not - every operation is modulo 32 bits.
	# this can affect intermediate computations, so it might be difficult to get the final
	# result from FX and python to match on computations with large numbers.
	# we will just check python's output and reject programs that have this problem.
	if output > MAX_INT32 or output < MIN_INT32:
		os.remove(fname)
		return None, None

	return f, output

def generate_programs(total=None, target_dir='fuzzer'):
	count = 0

	cwd = os.getcwd()
	os.chdir(target_dir)

	while total is None or count < total:
		f, output = generate_valid_program(skip_exc=True)

		if f is None:
			continue

		with open(f'fuzzer_{datetime.utcnow().isoformat()}_{count}_{os.getpid()}.fx', 'w') as file:
			file.write(f'# OUTPUT: {output}\n\n')
			file.write(f.render())
			
		count += 1
		print(count)

	os.chdir(cwd)

def test_programs(target_dir='fuzzer'):
	cwd = os.getcwd()
	os.chdir(target_dir)

	count = 0
	for fx_file in [a for a in os.listdir() if a.endswith('.fx')]:
		count += 1
		print(count)

		with open(fx_file, 'r') as f:
			fx = f.read()

			tokens = fx.split('\n', maxsplit=1)
			output = tokens[0]
			code = tokens[1]

			py_output = output.split('OUTPUT:')[1].strip()
			fx_output = None

			try:
				try:
					prog = compile_text(code)

				except DivByZero:
					continue # Div by 0 is a syntax error in FX, this is ok

				# run it!
				fx_output = str(prog.run_func('func'))

				assert fx_output == py_output

			except Exception:
				print(f'Py:{py_output}, FX:{fx_output}')
				print(fx_file)
				raise

	os.chdir(cwd)

from multiprocessing import Pool

def main():
	with Pool(20) as p:
		p.map(generate_programs, [1000]*20)

	# generate_programs(10000)
	# test_programs()
	return

	i = 0
	while i < 1000:
	# while True:
		py_output = None

		try:
			f, py_output = generate_valid_program()

		except (ZeroDivisionError, TimeoutExpired, NoneOutput):
			# print(e)
			continue

		if f is None:
			continue

		i += 1

		print(i, end=' -> ')

		try:
			prog = compile_fx(f)

		except DivByZero:
			continue # Div by 0 is a syntax error in FX

		except Exception:
			print(f.render())
			raise

		try:
			fx_output = prog.run_func('func')

			print(f'Py:{py_output}, FX:{fx_output}', end='')
			print('')

			assert fx_output == py_output

		except Exception:
			print(f.render())
			raise

		

	# generate_programs()

	return



	f = Func()
	f.generate()

	# b.add_element()
	# b.add_element()

	# print(f)
	# print(f.count)
	# print(b.depth)
	print(f.render())

	print(run_py(f))


	# py_code = generate_python(f)
	
	# print(py_code)

if __name__ == '__main__':
	try:
		main()

	except KeyboardInterrupt:
		pass
