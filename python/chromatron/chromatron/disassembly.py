
import dis

with open('dis.fx', 'r') as f:
	code = f.read()

bytecode = compile(code, 'dis.fx', 'exec')
print(bytecode)
print('')

print(dis.dis(bytecode))

print('')
for c in bytecode.co_consts:
	try:
		print(c)
		print(dis.dis(c))
		print('')

	except Exception:
		pass