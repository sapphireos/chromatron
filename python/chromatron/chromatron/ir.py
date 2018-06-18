

class IR(object):
	def __init__(self):
		pass

class IRVar(IR):
	def __init__(self, name, type='i32', length=1):
		self.name = name
		self.type = type
		self.length = length

		assert length > 0

	def __str__(self):
		return "Var (%s, %s, %d)" % (self.name, self.type, self.length)

class IRFunc(IR):
	def __init__(self, name, ret_type='i32', params=None, body=None):
		self.name = name
		self.ret_type = ret_type
		self.params = params
		self.body = body

		if self.params == None:
			self.params = []

		if self.body == None:
			self.body = []

	def append(self, node):
		self.body.append(node)

	def __str__(self):
		params = ''

		for p in self.params:
			params += '%s %s,' % (p.type, p.name)

		# strip last comma
		params = params[:len(params) - 1]

		s = "Func %s(%s) -> %s\n" % (self.name, params, self.ret_type)

		for node in self.body:
			s += '\t\t%s\n' % (node)

		return s

class IRReturn(IR):
	def __init__(self, ret_var):
		self.ret_var = ret_var

	def __str__(self):
		return "Return %s" % (self.ret_var)

class IRNop(IR):
	def __str__(self):
		return "NOP" 

class IRBuilder(object):
	def __init__(self):
		self.funcs = {}
		self.locals = {}
		self.globals = {}
		self.objects = {}

		self.current_func = None

	def __str__(self):
		s = "FX IR:\n"

		s += 'Globals:\n'
		for i in self.globals.values():
			s += '\t%s\n' % i

		s += 'Locals:\n'
		for fname in self.locals.keys():
			if len(self.locals[fname].values()) > 0:
				s += '\t%s\n' % (fname)

				for l in self.locals[fname].values():
					s += '\t\t%s\n' % (l)

		s += 'Functions:\n'
		for func in self.funcs.values():
			s += '\t%s\n' % func

		return s

	def add_global(self, name, type, length):
		ir = IRVar(name, type, length)
		self.globals[name] = ir

		return ir

	def add_local(self, name, type, length):
		ir = IRVar(name, type, length)
		self.locals[self.current_func][name] = ir

		return ir

	def func(self, *args, **kwargs):
		func = IRFunc(*args, **kwargs)
		self.funcs[func.name] = func
		self.locals[func.name] = {}
		self.current_func = func.name

		return func

	def append_node(self, node):
		self.funcs[self.current_func].append(node)

	def ret(self, value):
		ir = IRReturn(value)

		self.append_node(ir)

		return ir

	def nop(self):
		ir = IRNop()

		self.append_node(ir)

		return ir

	def binop(self, op, left, right):
		print op, left, right










