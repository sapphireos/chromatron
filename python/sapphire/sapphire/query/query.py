# <license>
# 
#     This file is part of the Sapphire Operating System.
# 
#     Copyright (C) 2013-2018  Jeremy Billheimer
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

from sapphire.common.util import coerce_string_to_list


def query_dict(d, **kwargs):
    # check for all query (all trumps any other keywords)
    if "_all" in kwargs:
        if kwargs["_all"]:
            return d

        del kwargs["_all"]

    if "_expr" in kwargs:
        if not kwargs["_expr"](d):
            return None

        del kwargs["_expr"]

    contains = False
    if "_contains" in kwargs:
        attr_list = kwargs["_contains"]

        # if passed a single string, make it a list
        attr_list = coerce_string_to_list(attr_list)

        for attr in attr_list:
            # converts keys in dict to string for comparison to work
            if attr not in [str(k) for k in d.keys()]:
                return None

        contains = True
        del kwargs["_contains"]

    # if no other kwargs are passed, we have nothing to match against
    if len(kwargs) == 0:
        if contains:
            return d

        else:
            return None

    # match all kwargs
    for k in kwargs:
        if not k in d or str(d[k]) != str(kwargs[k]):
            return None

    return d
