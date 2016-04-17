#include "nonlinear_solver.hpp"
#include "helper.hpp"

#include <Piro_NOXSolver.hpp>
#include <Piro_LOCASolver.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCPStdSharedPtrConversions.hpp>
#include <Thyra_TpetraThyraWrappers.hpp>

#include <map>

// =============================================================================
void
nosh::
nonlinear_solve(
    const std::shared_ptr<Thyra::ModelEvaluatorDefaultBase<double>> & model,
    std::map<std::string, boost::any> solver_params
    )
{
  // nominal_values.set_x(x_thyra);

  // auto nv = model->getNominalValues();
  // auto xx = nv.get_x();
  // auto xx_tpetra =
  //   Thyra::TpetraOperatorVectorExtraction<double,int,int>::getConstTpetraVector(
  //       xx
  //       );
  // auto data = xx_tpetra->getData();
  // std::cout << "nv->x" << std::endl;
  // std::cout << "   " << data[0] << std::endl;
  // std::cout << "   " << data[1] << std::endl;
  // std::cout << "   " << data[2] << std::endl;

  auto p = Teuchos::rcp(new Teuchos::ParameterList());
  std_map_to_teuchos_list(solver_params, *p);

  auto piro = std::make_shared<Piro::NOXSolver<double>>(
        p,
        Teuchos::rcp(model)
        // Teuchos::rcp(observer)
        );

  // Now the setting of inputs and outputs.
  Thyra::ModelEvaluatorBase::InArgs<double> inArgs = piro->createInArgs();
  inArgs.set_p(
      0,
      piro->getNominalValues().get_p(0)
      );

  // // set initial value
  // x.putScalar(3.0);
  // const auto x_rcp = Teuchos::rcpFromRef(x);
  // const auto x_thyra = Thyra::createVector(x_rcp, model->get_x_space());
  // inArgs.set_x(x_thyra);

  // Set output arguments to evalModel call.
  Thyra::ModelEvaluatorBase::OutArgs<double> outArgs = piro->createOutArgs();

  // Now solve the problem and return the responses.
  piro->evalModel(inArgs, outArgs);

  // // get solution
  // auto sol = model.get_x();
  // auto sol_tpetra =
  //   Thyra::TpetraOperatorVectorExtraction<double,int,int>::getConstTpetraVector(
  //       sol
  //       );

  // // Copy over solution vector. TODO check if really necessary
  // Tpetra::deep_copy(x, *sol_tpetra);

  return;
}
// =============================================================================
