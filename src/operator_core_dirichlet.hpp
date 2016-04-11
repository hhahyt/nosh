#ifndef NOSH_OPERATOR_CORE_DIRICHLET_HPP
#define NOSH_OPERATOR_CORE_DIRICHLET_HPP

#include <Eigen/Dense>

namespace nosh {
  class operator_core_dirichlet
  {
    public:
      explicit operator_core_dirichlet(
          const std::set<std::string> & _subdomain_ids
          ):
        subdomain_ids(_subdomain_ids)
      {};

      virtual
      ~operator_core_dirichlet()
      {};

      virtual
      double
      eval(
          const Eigen::Vector3d & x,
          const double u
          ) const = 0;

    public:
      const std::set<std::string> subdomain_ids;
  };
}
#endif // NOSH_DIRICHLET_BC_HPP
