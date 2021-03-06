#include "poisson.hpp"
#include <nosh.hpp>
#include <memory>

#include <Teuchos_StandardCatchMacros.hpp>

using dict = std::map<std::string, boost::any>;
int main(int argc, char *argv[]) {
  Teuchos::GlobalMPISession session(&argc, &argv, NULL);
  auto out = Teuchos::VerboseObjectBase::getDefaultOStream();

  bool success = true;
  try {
  //const auto mesh = nosh::read("r2.h5m");
  const auto mesh = nosh::read("pacman.h5m");
  //const auto mesh = nosh::read("screw3.h5m");
  //const auto mesh = nosh::read("cubesmall.h5m");
  //const auto mesh = nosh::read("cube.h5m");

  mesh->mark_subdomains({
      std::make_shared<poisson::gamma0>(),
      std::make_shared<poisson::gamma1>()
      });

  poisson::poisson problem(mesh);

  nosh::function x(mesh);
  x.putScalar(0.0);

  //nosh::scaled_linear_solve(
  mikado::linear_solve(
      *(problem.matrix), *(problem.rhs), x,
      {
#if 0
        // For solver parameters, check
        // https://trilinos.org/wordpress/wp-content/uploads/2015/05/MueLu_Users_Guide_Trilinos12_0.pdf
        {"package", "MueLu"},
        {"parameters", dict{
          {"convergence tolerance", 1.0e-10},
          {"max cycles", 25},
          {"cycle type", "W"},
        }}
#endif
#if 0
        // Check
        // https://trilinos.org/docs/dev/packages/amesos2/doc/html/group__amesos2__solver__parameters.html
        // for more options.
        {"package", "Amesos2"},
        {"method", "SuperLU"},
        {"parameters", dict{
          {"Transpose", false},
          {"ColPerm", "COLAMD"}
        }}
#endif
#if 1
        {"package", std::string("Belos")},
        //,{"method", "Pseudo Block GMRES"},
        {"method", std::string("Pseudo Block CG")},
        {"parameters", dict{
          {"Convergence Tolerance", 1.0e-10},
          {"Output Frequency", 1},
          {"Output Style", 1},
          {"Verbosity", 33}
        }},
        {"preconditioner", std::string("MueLu")}
        // {"preconditioner matrix", M},
        // {"preconditioner parameters", dict{
        //   {"cycle type", "V"}
        // }}
#endif
      }
      );

  nosh::write(x, "out.h5m");
  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, *out, success);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
