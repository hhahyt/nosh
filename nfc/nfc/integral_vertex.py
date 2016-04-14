# -*- coding: utf-8 -*-
#
import os
from string import Template
import sympy
import nfl

from .code_generator_eigen import CodeGeneratorEigen
from .expression import *
from .subdomain import *
from .helpers import \
        extract_c_expression, \
        extract_linear_components, \
        get_uuid, \
        is_affine_linear, \
        list_unique, \
        members_init_declare, \
        replace_nosh_functions, \
        templates_dir


class IntegralVertex(object):
    def __init__(self, namespace, integrand, subdomains, matrix_var=None):
        self.namespace = namespace
        self.class_name = 'vertex_core_' + get_uuid()

        self.matrix_var = matrix_var

        x = sympy.MatrixSymbol('x', 3, 1)
        fx = integrand(x)
        self.expr, self.vector_vars = _discretize_expression(fx)

        # collect vector parameters
        self.vector_params = set()
        for s in self.expr.atoms(sympy.IndexedBase):
            # `u` is an argument to the kernel and hence already defined
            if s != sympy.IndexedBase('u'):
                self.vector_params.add(s)

        self.dependencies = set().union(
            [ExpressionCode(type(atom))
                for atom in self.expr.atoms(nfl.Expression)],
            [SubdomainCode(sd) for sd in subdomains]
            )
        return

    def get_dependencies(self):
        return self.dependencies

    def get_class_object(self, dependency_class_objects):
        if self.matrix_var:
            arguments = set([sympy.Symbol('vertex')])
        else:
            arguments = set([sympy.Symbol('vertex'), sympy.Symbol('u')])
        used_vars = self.expr.free_symbols

        eval_body = []
        init = []
        declare = []
        methods = []

        # now take care of the template substitution
        deps_init, deps_declare = \
            members_init_declare(
                    self.namespace,
                    'matrix_core_vertex' if self.matrix_var else
                    'operator_core_vertex',
                    dependency_class_objects
                    )
        init.extend(deps_init)
        declare.extend(deps_declare)

        # handle vector parameters
        vector_parameters, vector_init, vector_declare, vector_methods = \
            _handle_vector_parameters(self.vector_params)
        arguments.update(vector_parameters)
        init.extend(vector_init)
        declare.extend(vector_declare)
        methods.extend(vector_methods)

        extra_body, extra_init, extra_declare = _get_extra(
                arguments, used_vars
                )
        eval_body.extend(extra_body)
        init.extend(extra_init)
        declare.extend(extra_declare)

        # remove double lines
        eval_body = list_unique(eval_body)
        init = list_unique(init)
        declare = list_unique(declare)

        if self.matrix_var:
            coeff, affine = extract_linear_components(
                    self.expr,
                    sympy.Symbol('%s[k]' % self.matrix_var)
                    )
            type = 'matrix_core_vertex'
            filename = os.path.join(templates_dir, 'matrix_core_vertex.tpl')
            with open(filename, 'r') as f:
                src = Template(f.read())
                code = src.substitute({
                    'name': self.class_name,
                    'vertex_contrib': extract_c_expression(coeff),
                    'vertex_affine': extract_c_expression(-affine),
                    'vertex_body': '\n'.join(eval_body),
                    'members_init': ':\n' + ',\n'.join(init) if init else '',
                    'members_declare': '\n'.join(declare)
                    })
        else:
            type = 'operator_core_vertex'
            filename = os.path.join(templates_dir, 'operator_core_vertex.tpl')
            with open(filename, 'r') as f:
                src = Template(f.read())
                code = src.substitute({
                    'name': self.class_name,
                    'return_value': extract_c_expression(self.expr),
                    'eval_body': '\n'.join(eval_body),
                    'members_init': ':\n' + ',\n'.join(init) if init else '',
                    'members_declare': '\n'.join(declare),
                    'methods': '\n'.join(methods)
                    })

        return {
            'type': type,
            'code': code,
            'class_name': self.class_name,
            'constructor_args': [],
            'vector_parameters': vector_parameters
            }


# def get_matrix_core_vertex_code(namespace, class_name, core):
#     '''Get code generator from raw core object.
#     '''
#     # handle the vertex contributions
#     x = sympy.MatrixSymbol('x')
#     vol = sympy.Symbol('control_volume')
#     all_symbols = set([x, vol])
#
#     specs = inspect.getargspec(method)
#     assert(len(specs.args) == len(all_symbols) + 1)
#
#     vertex_coeff, vertex_affine = method(x, vol)
#
#     return _get_code_matrix_core_vertex(
#             namespace, class_name,
#             vertex_coeff, vertex_affine
#             )


def _discretize_expression(expr):
    expr, fks = replace_nosh_functions(expr)
    return sympy.Symbol('control_volume') * expr, fks


def _get_extra(arguments, used_variables):
    vertex = sympy.Symbol('vertex')
    unused_arguments = arguments - used_variables
    undefined_symbols = used_variables - arguments

    init = []
    body = []
    declare = []

    control_volume = sympy.Symbol('control_volume')
    if control_volume in undefined_symbols:
        init.append('mesh_(mesh)')
        declare.append('const std::shared_ptr<const nosh::mesh> mesh_;')
        init.append('c_data_(mesh->control_volumes()->getData())')
        declare.append('const Teuchos::ArrayRCP<const double> c_data_;')
        body.append('const auto k = this->mesh_->local_index(vertex);')
        body.append('const auto control_volume = this->c_data_[k];')
        undefined_symbols.remove(control_volume)
        if vertex in unused_arguments:
            unused_arguments.remove(vertex)

    x = sympy.MatrixSymbol('x', 3, 1)
    if x in undefined_symbols:
        init.append('mesh_(mesh)')
        declare.append('const std::shared_ptr<const nosh::mesh> mesh_;')
        init.append('c_data_(mesh->control_volumes()->getData())')
        declare.append('const Teuchos::ArrayRCP<const double> c_data_;')
        body.append('const auto k = this->mesh_->local_index(vertex);')
        body.append('const auto x = this->mesh_->get_coords(vertex);')
        undefined_symbols.remove(x)
        if vertex in unused_arguments:
            unused_arguments.remove(vertex)

    k = sympy.Symbol('k')
    if k in undefined_symbols:
        init.append('mesh_(mesh)')
        declare.append('const std::shared_ptr<const nosh::mesh> mesh_;')
        body.append('const auto k = this->mesh_->local_index(vertex);')
        undefined_symbols.remove(k)
        if vertex in unused_arguments:
            unused_arguments.remove(vertex)

    if len(undefined_symbols) > 0:
        raise RuntimeError(
                'The following symbols are undefined: %s' % undefined_symbols
                )

    for name in unused_arguments:
        body.insert(0, '(void) %s;' % name)

    return body, init, declare


def _handle_vector_parameters(vector_params):
    '''Treat vector variables (u, u0,...)
    '''
    symbols = set()
    vector_init = []
    vector_declare = []
    vector_methods = []

    tpetra_str = 'Tpetra::Vector<double, int, int>'
    for v in vector_params:
        vector_init.extend([
            'mesh_(mesh)',
            '%s_vec_(std::make_shared<%s>(Teuchos::rcp(mesh->map())))' % (v, tpetra_str),
            '%s(%s_vec_->getData())' % (v, v)
            ])
        vector_declare.extend([
            'const std::shared_ptr<const nosh::mesh> mesh_;',
            'std::shared_ptr<const %s> %s_vec_;' % (tpetra_str, v),
            'Teuchos::ArrayRCP<const double> %s;' % v
            ])
        symbols.add(sympy.Symbol('%s' % v))

    if len(vector_params) > 0:
        vector_methods.append('''
        virtual
        std::map<std::string, std::shared_ptr<%s>>
        get_vector_parameters() const
        {
          return {
            %s
            };
        };
        ''' % (
            tpetra_str,
            ',\n'.join(['{"%s", %s_vec_}' % (v, v) for v in vector_params])
            )
        )
        vector_methods.append('''
        virtual
        void
        refill_(
            const std::map<std::string, double> & scalar_params,
            const std::map<std::string, std::shared_ptr<const %s>> & vector_params
            )
        {%s}
        ''' % (
          tpetra_str,
          ',\n'.join(['''
          this->%s_vec_ = vector_params.at("%s");
          this->%s = this->%s_vec_->getData();
          ''' % (vec, vec, vec, vec) for vec in vector_params])
          )
        )

    return symbols, vector_init, vector_declare, vector_methods
