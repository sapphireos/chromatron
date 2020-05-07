# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2020  Jeremy Billheimer
# 
# 
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
# 
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
# 
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
# 
# </license>

import time
import os
import sys

from pprint import pprint

from pyparsing import *

from elysianfields import *
from catbus import *

from . import code_gen



# comment_delimiter = Word('#')
# comment = comment_delimiter + ZeroOrMore(Word(printables)) + LineEnd()

quotes = Or([Literal("'"), Literal('"')])

var_name = Word(alphanums + '_')

alias = Group(Suppress(Keyword('alias')) + var_name + Suppress(Keyword('as')) + var_name).setResultsName('alias', listAllMatches=True)
var = Group(Suppress(Keyword('var')) + var_name).setResultsName('var', listAllMatches=True)
db = Group(Suppress(Keyword('db')) + var_name).setResultsName('db', listAllMatches=True)

day_of_week = Or([CaselessLiteral('Monday'), 
                  CaselessLiteral('Tuesday'), 
                  CaselessLiteral('Wednesday'),
                  CaselessLiteral('Thursday'),
                  CaselessLiteral('Friday'),
                  CaselessLiteral('Saturday'),
                  CaselessLiteral('Sunday')])
day_param = Keyword('day') + Suppress(Literal('=')) + Suppress(Optional(quotes)) + day_of_week + Suppress(Optional(quotes))
hour_param = Keyword('hour') + Suppress(Literal('=')) + Word(nums)
minute_param = Keyword('minute') + Suppress(Literal('=')) + Word(nums)
second_param = Keyword('second') + Suppress(Literal('=')) + Word(nums)

_date_params = Or([day_param, hour_param, minute_param, second_param])
date_params = Group(ZeroOrMore(_date_params + Suppress(Literal(','))) + OneOrMore(_date_params))

_time_params = Or([day_param, hour_param, minute_param, second_param])
time_params = Group(ZeroOrMore(_time_params + Suppress(Literal(','))) + OneOrMore(_time_params))


tag = Suppress(Optional(quotes)) + var_name + Suppress(Optional(quotes))
tags = Group(Suppress(Literal('[')) + ZeroOrMore(tag + Suppress(Literal(','))) + OneOrMore(tag) + Suppress(Literal(']')))

query = Group(var_name + Suppress(Literal('@')) + tags).setResultsName('query')

dest_var = var_name.setResultsName('dest_var')
source_var = var_name.setResultsName('source_var')
link_tag = Group(var_name + Suppress(Literal(':'))).setResultsName('tag')

receive = Group(Keyword('receive') + Optional(link_tag) + dest_var + Keyword('from') + query).setResultsName('receive', listAllMatches=True)
send = Group(Keyword('send') + Optional(link_tag) + source_var + Keyword('to') + query).setResultsName('send', listAllMatches=True)

when_ = Keyword('when')
do_ = Keyword('do')
end_ = Keyword('end')

at = Group(Suppress(Keyword('at')) + Suppress(Literal('(')) + date_params + Suppress(Literal(')'))).setResultsName('time')
every = Group(Suppress(Keyword('every')) + Suppress(Literal('(')) + time_params + Suppress(Literal(')'))).setResultsName('time')

rule_keywords = Or([when_, do_, end_])

code = originalTextFor(Word(':') + OneOrMore(~rule_keywords + Word(printables)))
condition = code.setResultsName('condition', listAllMatches=True)
action = code.setResultsName('action', listAllMatches=True)
rule = Group(when_ + condition + do_ + action + end_).setResultsName('rule', listAllMatches=True)

at_rule = Group(at + action + end_).setResultsName('at_rule', listAllMatches=True)
every_rule = Group(every + action + end_).setResultsName('every_rule', listAllMatches=True)


directives = ZeroOrMore(Or([var, db, alias, receive, send, rule, at_rule, every_rule]))

automaton_file = directives().ignore(pythonStyleComment)


AUTOMATON_FILE_MAGIC       = 0x4F545541  # 'AUTO'
AUTOMATON_RULE_MAGIC       = 0x454C5552  # 'RULE'
AUTOMATON_VERSION          = 2


class AutomatonDBVar(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusHash(_name="hash"),
                  CatbusStringField(_name="name")]

        super(AutomatonDBVar, self).__init__(_name="automaton_db_var", _fields=fields, **kwargs)

class AutomatonLink(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusHash(_name="source_hash"),
                  CatbusHash(_name="dest_hash"),
                  CatbusQuery(_name="query")]

        super(AutomatonLink, self).__init__(_name="automaton_link", _fields=fields, **kwargs)

class AutomatonTriggerIndex(StructField):
    def __init__(self, **kwargs):
        fields = [CatbusHash(_name="hash"),
                  Uint16Field(_name="condition_offset"),
                  Uint8Field(_name="status")]

        super(AutomatonTriggerIndex, self).__init__(_name="automaton_trigger_index", _fields=fields, **kwargs)

class AutomatonRule(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="magic"),
                  Uint8Field(_name="condition_data_len"),
                  Uint16Field(_name="condition_code_len"),
                  Uint8Field(_name="action_data_len"),
                  Uint16Field(_name="action_code_len"),
                  ArrayField(_name="condition_data", _field=Int32Field),
                  ArrayField(_name="condition_code", _field=Uint8Field),
                  ArrayField(_name="action_data", _field=Int32Field),
                  ArrayField(_name="action_code", _field=Uint8Field)]

        super(AutomatonRule, self).__init__(_name="automaton_rule", _fields=fields, **kwargs)

        self.magic              = AUTOMATON_RULE_MAGIC
        self.condition_data_len = len(self.condition_data)
        self.condition_code_len = len(self.condition_code)
        self.action_data_len    = len(self.action_data)
        self.action_code_len    = len(self.action_code)


class AutomatonFile(StructField):
    def __init__(self, **kwargs):
        fields = [Uint32Field(_name="prog_magic"),
                  Uint8Field(_name="version"),
                  Uint8Field(_name="local_vars_len"),
                  Uint8Field(_name="db_vars_len"),
                  Uint8Field(_name="send_len"),
                  Uint8Field(_name="recv_len"),
                  Uint8Field(_name="trigger_index_len"),
                  Uint8Field(_name="rules_len"),
                  ArrayField(_name="db_vars", _field=AutomatonDBVar),
                  ArrayField(_name="send_links", _field=AutomatonLink),
                  ArrayField(_name="receive_links", _field=AutomatonLink),
                  ArrayField(_name="trigger_index", _field=AutomatonTriggerIndex),
                  ArrayField(_name="rules", _field=AutomatonRule)]

        super(AutomatonFile, self).__init__(_name="automaton_file", _fields=fields, **kwargs)

        self.prog_magic         = AUTOMATON_FILE_MAGIC
        self.version            = AUTOMATON_VERSION

        self.db_vars_len        = len(self.db_vars)
        self.send_len           = len(self.send_links)
        self.recv_len           = len(self.receive_links)
        self.trigger_index_len  = len(self.trigger_index)
        self.rules_len          = len(self.rules)
        

def format_code(s, condition=True):
    tokens = s.split('\n')

    if len(tokens) <= 1:
        return s

    assert tokens[0].strip() == ':'
    tokens.pop(0)

    indentation = tokens[0][:len(tokens[0]) - len(tokens[0].lstrip())]

    if condition:
        new_tokens = []
        for tok in tokens:
            new_token = tok.replace(indentation, '', 1)
            new_tokens.append(new_token)

        return '\n'.join(new_tokens)

    else:
        tokens.insert(0, 'def init():')
        tokens.append(indentation + 'return ___return_val ')
          
        return '\n'.join(tokens)

def compile_vm_code(text, debug_print=True, condition=True, local_vars=[]):
    return code_gen.compile_automaton_text(text, debug_print=debug_print, condition=condition, local_vars=local_vars)

def compile_file(filename, debug_print=False):
    with open(filename) as f:
        data = f.read()

    scriptname = os.path.split(filename)[1]

    results = automaton_file.parseString(data, parseAll=True).asDict()

    if debug_print:
        pprint(results)
        print('\n')
        
    # aliases = results['alias']

    # print "Aliases:"
    # print aliases

    db_vars = []

    if 'db' in results:
        for v in results['db']:
            v = v[0]
            kv = AutomatonDBVar(hash=catbus_string_hash(v), name=v)
            db_vars.append(kv)

    if debug_print:
        print("DB Vars:")
        for v in db_vars:
            print(v)


    local_vars = []
    if 'var' in results:
        for v in results['var']:
            v = v[0]
            local_vars.append(v)


    if debug_print:
        print("Local Vars:")
        for v in local_vars:
            print(v)

    send_links = []

    if 'send' in results:
        for link in results['send']:
            source_hash = catbus_string_hash(link['source_var'])
            dest_hash = catbus_string_hash(link['query'][0])
            query_tags = [catbus_string_hash(a) for a in link['query'][1]]

            query = CatbusQuery()
            query._value = query_tags

            info = AutomatonLink(
                    source_hash=source_hash,
                    dest_hash=dest_hash,
                    query=query)
            
            send_links.append(info)

    if debug_print:
        print("Send:")
        print(send_links)

    receive_links = []

    if 'receive' in results:
        for link in results['receive']:
            dest_hash = catbus_string_hash(link['dest_var'])
            source_hash = catbus_string_hash(link['query'][0])
            query_tags = [catbus_string_hash(a) for a in link['query'][1]]

            query = CatbusQuery()
            query._value = query_tags

            info = AutomatonLink(
                    source_hash=source_hash,
                    dest_hash=dest_hash,
                    query=query)
            
            receive_links.append(info)

    if debug_print:
        print("Recv:")
        print(receive_links)

    
    rules = []
    triggers = []
    largest_data = 0
    largest_code = 0

    if 'rule' in results:
        for rule in results['rule']:
            action = format_code(rule['action'][0], False)
            condition = format_code(rule['condition'][0])

            if debug_print:
                print("\n\nCondition:")
                print(condition)

            condition = compile_vm_code(condition, local_vars=local_vars, debug_print=debug_print)

            # pprint(condition)

            condition_triggers = []
            for name, hashed_val in condition['data']['read_keys'].items():
                condition_triggers.append(hashed_val)

            for name, reg in condition['data']['local_registers'].items():
                if name in local_vars:
                    condition_triggers.append(reg.addr)

            triggers.append(condition_triggers)

            condition_data = condition['data_stream']
            condition_code = condition['code_stream']

            if len(condition_data) > largest_data:
                largest_data = len(condition_data)

            if len(condition_code) > largest_code:
                largest_code = len(condition_code)

            if debug_print:
                print("\n\nAction:")
                print(action)

            action = compile_vm_code(action, condition=False, local_vars=local_vars, debug_print=debug_print)
            # pprint(action)

            action_data = action['data_stream']
            action_code = action['code_stream']

            if len(action_data) > largest_data:
                largest_data = len(action_data)

            if len(action_code) > largest_code:
                largest_code = len(action_code)


            info = AutomatonRule(
                        condition_data=condition_data,
                        condition_code=condition_code,
                        action_data=action_data,
                        action_code=action_code)

            rules.append(info)
    
    index = 0
    trigger_index = []
    for i in range(len(rules)):
        for trigger in triggers[i]:
            info = AutomatonTriggerIndex(
                    hash=trigger,
                    condition_offset=index)

            trigger_index.append(info)

        index += rules[i].size()

    automaton_data = AutomatonFile(
                        local_vars_len=len(local_vars),
                        db_vars=db_vars,
                        send_links=send_links,
                        receive_links=receive_links,
                        trigger_index=trigger_index,
                        rules=rules)

    if debug_print:
        print('')
        print('')
        print('')
        print('')
        print(automaton_data)

    bin_filename = scriptname + '.auto'
    # bin_filename = 'automaton.auto'
    with open(bin_filename, 'wb+') as f:
        f.write(automaton_data.pack())

    if debug_print:
        print("Saved as %s" % (bin_filename))

        print("%d bytes" % (automaton_data.size()))

        print("Max code: %d bytes" % (largest_code))
        print("Max data: %d bytes" % (largest_data * 4))


    return bin_filename


if __name__ == "__main__":
    filename = sys.argv[1]

    compile_file(filename, debug_print=True)
    # pass
