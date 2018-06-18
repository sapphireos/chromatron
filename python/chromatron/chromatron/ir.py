

class IR(object):
	def __init__(self):
		pass

class IRVar(IR):
	def __init__(self, name, type, length):
		self.name = name
		self.type = type
		self.length = length

	def __str__(self):
		return "IRVar (%s, %s, %d)" % (self.name, self.type, self.length)


class IRBuilder(object):
	def __init__(self):
		self.funcs = {}
		self.locals = {}
		self.globals = {}
		self.objects = {}

	def __str__(self):
		s = "FX IR:\n"

		s += 'Globals:\n'
		for i in self.globals.values():
			s += '\t%s\n' % i


		return s

	def add_global(self, name, type, length):
		ir = IRVar(name, type, length)
		self.globals[name] = ir




