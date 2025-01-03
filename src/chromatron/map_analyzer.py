
from pprint import pprint


section_stats = {}


with open('main.map', 'r') as f:
    map_file = f.read()


i = 0
lines = [line.strip() for line in map_file.splitlines()]
while i < len(lines):

    line = lines[i]

    process_line = False
    # for section in ['data', 'bss', 'text']:
    for section in ['data', 'bss']:
        if line.startswith(f'.{section}'):
            process_line = True

    if not process_line:
        i += 1
        continue

    tokens = line.split()

    header_tokens = tokens[0].split('.')

    section = header_tokens[1]

    if section not in section_stats:
        section_stats[section] = {
            'count': 0,
            'total': 0
        }

    try:
        name = header_tokens[2]

    except IndexError:
        name = '?'

    if len(tokens) == 1:
        next_line = lines[i + 1]
        i += 1

        body_tokens = next_line.split()

    else:
        body_tokens = tokens[1:]
    
    addr = body_tokens[0]
    size = int(body_tokens[1], 16)
    try:
        file = body_tokens[2]

    except IndexError:
        file = '?'


    if size > 0 and int(addr, 16) > 0 and file != '?':
        print(f'{section:4} {name:36} {addr:16} {size:4} {file}')

        section_stats[section]['count'] += 1
        section_stats[section]['total'] += size
        

    i += 1
    


pprint(section_stats)


