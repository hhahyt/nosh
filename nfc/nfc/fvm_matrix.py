# -*- coding: utf-8 -*-
#
import nfl
import os
from string import Template
import sympy

from .integral_boundary import IntegralBoundary
from .dirichlet import Dirichlet
from .integral_edge import IntegralEdge
from .integral_vertex import IntegralVertex
from .helpers import get_uuid, templates_dir


class FvmMatrixCode(object):
    def __init__(self, obj):
        self.dependencies = gather_dependencies(obj)
        return

    def get_dependencies():
        return self.dependencies

    def get_class_object():
        return


def gather_dependencies(obj):
    dependencies = set()
    u = sympy.Function('u')
    res = obj.apply(u)
    for integral in res.integrals:
        if isinstance(integral.measure, nfl.ControlVolumeSurface):
            dependencies.add(
                IntegralEdge(
                    u, integral.integrand, integral.subdomains, True
                    )
                )
        elif isinstance(integral.measure, nfl.ControlVolume):
            dependencies.add(
                IntegralVertex(
                    u, integral.integrand, integral.subdomains, True
                    )
                )
        elif isinstance(integral.measure, nfl.BoundarySurface):
            dependencies.add(
                IntegralBoundary(
                    u, integral.integrand, integral.subdomains, True
                    )
                )
        else:
            raise RuntimeError('Illegal measure type \'%s\'.' % measure)

    for dirichlet in obj.dirichlet:
        f, subdomains = dirichlet
        if not isinstance(subdomains, list):
            try:
                subdomains = list(subdomains)
            except TypeError:  # TypeError: 'D1' object is not iterable
                subdomains = [subdomains]
        dependencies.add(Dirichlet(f, subdomains, True))

    return dependencies


def get_code_fvm_matrix(namespace, class_name, obj):
    code, dependencies, matrix_core_names = \
            handle_core_dependencies(namespace, obj)

    # append the code of the linear problem itself
    code += '\n' + get_code_linear_problem(
            'fvm_matrix.tpl',
            class_name.lower(),
            'nosh::fvm_matrix',
            matrix_core_names['edge'],
            matrix_core_names['vertex'],
            matrix_core_names['boundary'],
            matrix_core_names['dirichlet'],
            )

    return code, dependencies


def get_code_linear_problem(
        template_filename,
        class_name,
        base_class_name,
        matrix_core_edge_names,
        matrix_core_vertex_names,
        matrix_core_boundary_names,
        matrix_core_dirichlet_names,
        ):
    constructor_args = [
        'const std::shared_ptr<const nosh::mesh> & _mesh'
        ]
    init_matrix_core_edge = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>()' % n
                 for n in matrix_core_edge_names]
                )
            )
    init_matrix_core_vertex = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>()' % n
                 for n in matrix_core_vertex_names]
                )
            )
    init_matrix_core_boundary = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>()' % n
                 for n in matrix_core_boundary_names]
                )
            )
    init_matrix_core_dirichlet = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>()' % n
                 for n in matrix_core_dirichlet_names]
                )
            )

    members_init = [
      '%s(\n_mesh,\n %s,\n %s,\n %s,\n %s\n)' %
      (base_class_name,
       init_matrix_core_edge,
       init_matrix_core_vertex,
       init_matrix_core_boundary,
       init_matrix_core_dirichlet
       )
      ]
    members_declare = []

    templ = os.path.join(templates_dir, template_filename)
    with open(templ, 'r') as f:
        src = Template(f.read())
        code = src.substitute({
            'name': class_name,
            'constructor_args': ',\n'.join(constructor_args),
            'members_init': ',\n'.join(members_init),
            'members_declare': '\n'.join(members_declare)
            })

    return code
