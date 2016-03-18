# -*- coding: utf-8 -*-
from nfl import *
from sympy import *


class Bc1(DirichletBC):
    def eval(self, x): return 0.0


class F(Expression):
    def eval(x): return 1.0
    degree = 0


class eps(Expression):
    pass


class Laplace(FvmMatrix):
    def edge_contrib(alpha, edge_midpoint):
        return [
                [
                    # Expression(alpha) * eps(edge_midpoint) * alpha,
                    eps(edge_midpoint) * alpha,
                    -eps(edge_midpoint) * alpha
                ],
                [
                    -eps(edge_midpoint) * alpha,
                    eps(edge_midpoint) * alpha
                ]
                ]
    boundary_conditions = [Bc1()]


# class Laplace(Operator):
#     def eval(u):
#         alpha = Expression()
#         return - alpha * dot(n, grad(u)) * dS
#
#     boundary_conditions = [Bc1()]
