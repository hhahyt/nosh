#include "poisson.hpp"
#include <nosh.hpp>
#include <memory>

#include <Teuchos_StandardCatchMacros.hpp>

using list = std::map<std::string, boost::any>;
int main(int argc, char *argv[]) {
  Teuchos::GlobalMPISession session(&argc, &argv, NULL);
  auto out = Teuchos::VerboseObjectBase::getDefaultOStream();

  bool success = true;
  try {

  const auto mesh = nosh::read("2.h5m");
  std::cout << "READ, thanks!" << std::endl;

  //const auto bc1 = std::make_shared<poisson::bc1>();
  //const auto bc2 = std::make_shared<poisson::bc2>();

  //std::cout << "1" << std::endl;
  //poisson::laplace matrix(mesh, {bc1, bc2});
  //std::cout << "2" << std::endl;

  //const poisson::f rhs;

  nosh::function x(mesh);
  x.putScalar(3.14);
  //std::cout << "3" << std::endl;

  //nosh::scaled_linear_solve(
  //    matrix, rhs, x,
  //    {
  //      {"package", "Belos"},
  //      {"method", "Pseudo Block GMRES"},
  //      {"parameters", list{
  //        {"Convergence Tolerance", 1.0e-10},
  //        {"Output Frequency", 1},
  //        {"Output Style", 1},
  //        {"Verbosity", 33}
  //      }},
  //      {"preconditioner", "MueLu"},
  //      {"preconditioner parameters", list{
  //      }}
  //    }
  //    );
  //std::cout << "4" << std::endl;

  nosh::write(x, "out.h5m");

  }
  TEUCHOS_STANDARD_CATCH_STATEMENTS(true, *out, success);

  return EXIT_SUCCESS;
}
