# -*- coding: utf-8 -*-
#
import nfl
import os
from string import Template
import sympy
from .helpers import extract_c_expression, templates_dir


def get_code_dirichlet(name, function, subdomains):
    dependencies = subdomains

    x = sympy.MatrixSymbol('x', 3, 1)
    result = function(x)
    try:
        is_x_used_eval = x in result.free_symbols
    except AttributeError:
        is_x_used_eval = False

    subdomain_ids = set([
        sd.__class__.__name__.lower() for sd in subdomains
        ])

    if len(subdomain_ids) == 0:
        # If nothing is specified, use the entire boundary
        subdomain_ids.add('boundary')

    init = '{%s}' % ', '.join(['"%s"' % s for s in subdomain_ids])

    # template substitution
    with open(os.path.join(templates_dir, 'matrix_core_dirichlet.tpl'), 'r') as f:
        src = Template(f.read())
        code = src.substitute({
            'name': name.lower(),
            'init': 'nosh::matrix_core_dirichlet(%s)' % init,
            'eval_return_value': extract_c_expression(result),
            'eval_void': '' if is_x_used_eval else '(void) x;\n',
            })

    return code, dependencies
