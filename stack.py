
import sys
import os
import subprocess
from pprint import pprint
import platform

MAX_DEPTH = 2

dirs = ['sapphireos', 'lib_chromatron', 'chromatron', 'lib_gfx', os.path.join('hal','xmega128a4u')]

os.chdir('src')

# get list of files
filenames = []

for d in dirs:
    prev_dir = os.getcwd()
    os.chdir(d)
    for (dirpath, dirnames, _fnames) in os.walk(os.getcwd()):
        # print dirpath
        filenames.extend([os.path.join(dirpath, a) for a in _fnames])
    os.chdir(prev_dir)

# sys.exit(0)

RETURN_ADDR_SIZE = 2

functions = {}
stack_sizes = {}
total_stacks = {}
modules = {}
not_found = []

def get_total_stack(function_name, visited=None):
    stack = 0

    max_stack = 0

    if visited is None:
        visited = [function_name]

    try:
        for callee in functions[function_name]:
            if callee not in visited:
                callee_stack = get_total_stack(callee, visited)

                if callee_stack > max_stack:
                    max_stack = callee_stack

    except KeyError:
        pass

    stack += max_stack

    try:
        stack += stack_sizes[function_name]

    except KeyError:
        if function_name not in not_found:
            print "Not found: %s" % (function_name)
            not_found.append(function_name)

    return stack

def print_call_tree(function_name, depth=0, depth_bytes=0, visited=None):

    if visited is None:
        visited = []

    # if function_name in visited:
    #     return

    visited.append(function_name)

    try:
        depth_bytes += stack_sizes[function_name]
        print "%4d |%4d %s> %2d %4d %-48s" % (total_stacks[function_name], stack_sizes[function_name], '-' * depth, depth, depth_bytes, function_name)

        for callee in functions[function_name]:
            if callee not in visited:
                if depth < MAX_DEPTH:
                    print_call_tree(callee, depth + 1, depth_bytes, visited)

    except KeyError:
        pass

    visited.remove(function_name)


if platform.system() == "Darwin":
    AVR_OBJDUMP = "avr-objdump-mac"

elif platform.system() == "Linux": 
    AVR_OBJDUMP = "avr-objdump-linux"

else:
    print "Unsupported OS"
    sys.exit(-1)

# go through list of object files
for fname in filenames:
    module_name, ext = os.path.splitext(fname)

    function_name = ''

    if ext == '.o':
        objdump = subprocess.check_output('../%s -dr %s' % (AVR_OBJDUMP, fname), shell=True)

        for line in objdump.split('\n'):
            tokens = line.split()
            try:
                if tokens[0] == '00000000':
                    # print tokens

                    function_name = tokens[1].replace('<', '').replace('>:', '')

                    if function_name != 'assert':

                        if function_name in functions:
                            print "Function name conflict in %s: %s" % (module_name, function_name)

                        else:
                            functions[function_name] = []

                        if module_name in modules:
                            modules[module_name].append(function_name)

                        else:
                            modules[module_name] = []

                else:
                    # print tokens
                    # if 'call' in tokens or 'rcall' in tokens:

                    # try:
                    #     if function_name == '__vector_65':
                    #         if 'R_AVR_CALL' in tokens:
                    #             # print tokens, tokens[1], functions[function_name]
                    #             # print functions[function_name]
                    #             # pass
                    #             print tokens

                    # except NameError:
                    #     pass

                    if tokens[1] == 'R_AVR_CALL':
                        try:
                            callee = tokens[2].split('.')[2]

                        except IndexError:
                            callee = tokens[2]

                        # if function_name == '__vector_65':
                            # print callee
                            # print tokens

                        try:
                            if callee not in functions[function_name]:
                                functions[function_name].append(callee)

                        except KeyError:
                            pass


            except IndexError:
                pass

    elif ext == '.su':
        data = open(fname).read()

        for line in data.split('\n'):
            tokens = line.split(':')

            try:
                tokens = tokens[3].split()

                function_name = tokens[0]

                if function_name != 'assert':
                    stack_size = int(tokens[1])

                    stack_sizes[function_name] = stack_size + RETURN_ADDR_SIZE

            except IndexError:
                pass

# pprint(functions)
# pprint(stack_sizes)

for function_name in functions.keys():
    try:
        total_stacks[function_name] = get_total_stack(function_name)

    except RuntimeError as e:
        print e
        print function_name
        sys.exit(-1)

# get_total_stack('rf_u8_read_frame_buf')

for a in sorted(total_stacks, key=total_stacks.get):
    try:
        print "%4d %4d %s" % (total_stacks[a], stack_sizes[a], a)

    except KeyError:
        pass

print ''
# print functions['__vector_65']
print ''



print_call_tree('vm_loader')
# print get_total_stack('gfx_fader_thread', [])
