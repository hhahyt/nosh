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
from .helpers import get_uuid, sanitize_identifier, templates_dir


class FvmMatrixCode(object):
    def __init__(self, namespace, cls):
        self.class_name = sanitize_identifier(cls.__name__)
        self.namespace = namespace

        u = sympy.Function('u')
        u.nosh = True

        # TODO
        self.vector_params = set()

        expr = cls.apply(u)
        self.dependencies = \
            gather_core_dependencies(
                    namespace, expr, cls.dirichlet, matrix_var=u
                    )
        return

    def get_dependencies(self):
        return self.dependencies

    def get_class_object(self, dep_class_objects):
        # Go through the dependencies collect the cores.
        code = get_code_linear_problem(
            'fvm_matrix.tpl',
            self.class_name,
            'nosh::fvm_matrix',
            self.dependencies
            )

        return {
            'code': code
            }


def gather_core_dependencies(namespace, res, dirichlets, matrix_var):
    dependencies = set()
    for integral in res.integrals:
        if isinstance(integral.measure, nfl.ControlVolumeSurface):
            dependencies.add(
                IntegralEdge(
                    namespace,
                    integral.integrand,
                    integral.subdomains,
                    matrix_var=matrix_var
                    )
                )
        elif isinstance(integral.measure, nfl.ControlVolume):
            dependencies.add(
                IntegralVertex(
                    namespace,
                    integral.integrand,
                    integral.subdomains,
                    matrix_var=matrix_var
                    )
                )
        elif isinstance(integral.measure, nfl.BoundarySurface):
            dependencies.add(
                IntegralBoundary(
                    namespace,
                    integral.integrand,
                    integral.subdomains,
                    matrix_var=matrix_var
                    )
                )
        else:
            raise RuntimeError('Illegal measure type \'%s\'.' % measure)

    for dirichlet in dirichlets:
        f, subdomains = dirichlet
        if not isinstance(subdomains, list):
            try:
                subdomains = list(subdomains)
            except TypeError:  # TypeError: 'D1' object is not iterable
                subdomains = [subdomains]
        dependencies.add(Dirichlet(f, subdomains, matrix_var))

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
        dependencies
        ):
    # Go through the dependencies collect the cores.
    dirichlet_cores = []
    vertex_cores = []
    edge_cores = []
    boundary_cores = []
    vector_parameters = {}
    for dep in dependencies:
        # Collect all vector dependencies
        vector_parameters[dep.class_name] = dep.vector_params
        # Sort the cores
        if isinstance(dep, Dirichlet):
            dirichlet_cores.append(dep)
        elif isinstance(dep, IntegralVertex):
            vertex_cores.append(dep)
        elif isinstance(dep, IntegralEdge):
            edge_cores.append(dep)
        elif isinstance(dep, IntegralBoundary):
            boundary_cores.append(dep)
        else:
            raise RuntimeError(
                'Dependency \'%s\' not accounted for.' % dep.class_name
                )

    constructor_args = [
        'const std::shared_ptr<const nosh::mesh> & _mesh'
        ]
    init_matrix_core_edge = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>(_mesh)' % n.class_name
                 for n in edge_cores]
                )
            )
    init_matrix_core_vertex = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>(_mesh)' % n.class_name
                 for n in vertex_cores]
                )
            )
    init_matrix_core_boundary = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>(_mesh)' % n.class_name
                 for n in boundary_cores]
                )
            )
    init_matrix_core_dirichlet = '{%s}' % (
            ', '.join(
                ['std::make_shared<%s>(_mesh)' % n.class_name
                 for n in dirichlet_cores]
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
