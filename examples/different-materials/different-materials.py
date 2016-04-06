# -*- coding: utf-8 -*-
from sympy import *
from nfl import *


class Bc1(DirichletBC):
    def eval(self, x): return 0.0


class F(Expression):
    def eval(x): return 1.0
    degree = 0


class eps(Expression):
    pass


class Laplace(FvmMatrix2):
    def eval(u):
        return integrate(
            lambda x: -eps(x) * n_dot_grad(u, x),
            dS()
            )

    boundary_conditions = [Bc1()]


# Alternative (raw) syntax:
# class Core(MatrixCore):
#     def edge_contrib(self, x0, x1, edge_length, edge_covolume):
#         alpha = edge_covolume / edge_length
#         edge_midpoint = 0.5 * (x0 + x1)
#         return [
#                 [
#                     eps(edge_midpoint) * alpha,
#                     -eps(edge_midpoint) * alpha
#                 ],
#                 [
#                     -eps(edge_midpoint) * alpha,
#                     eps(edge_midpoint) * alpha
#                 ]
#                 ]
#
#
# class Laplace(FvmMatrix):
#     matrix_cores = [Core()]
#     boundary_conditions = [Bc1()]