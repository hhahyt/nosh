// @HEADER
//
//    <one line to give the program's name and a brief idea of what it does.>
//    Copyright (C) 2012  Nico Schl\"omer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// @HEADER
#include <string>

#include <Teuchos_ParameterList.hpp>

#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

#include <Epetra_Map.h>
#include <Epetra_Vector.h>
#include <Thyra_EpetraModelEvaluator.hpp>

#include "nosh/StkMesh.hpp"
#include "nosh/ScalarField_Constant.hpp"
#include "nosh/ParameterMatrix_Keo.hpp"
#include "nosh/ParameterMatrix_DKeoDP.hpp"
#include "nosh/VectorField_ExplicitValues.hpp"
#include "nosh/ModelEvaluator_Nls.hpp"

#include <Teuchos_UnitTestHarness.hpp>
#include <Teuchos_RCPStdSharedPtrConversions.hpp>

namespace
{
// =============================================================================
void
testComputeF(const std::string & inputFileNameBase,
             const double mu,
             const double controlNormOne,
             const double controlNormTwo,
             const double controlNormInf,
             Teuchos::FancyOStream & out,
             bool & success)
{
  // Create a communicator for Epetra objects
#ifdef HAVE_MPI
  std::shared_ptr<Epetra_MpiComm> eComm(new Epetra_MpiComm (MPI_COMM_WORLD));
#else
  std::shared_ptr<Epetra_SerialComm> eComm(new Epetra_SerialComm());
#endif

  std::string inputFileName = "data/" + inputFileNameBase + ".e";
  // =========================================================================
  // Read the data from the file.
  std::shared_ptr<Nosh::StkMesh> mesh(
      new Nosh::StkMesh(eComm, inputFileName, 0)
      );

  // Cast the data into something more accessible.
  std::shared_ptr<Epetra_Vector> z =
    mesh->createComplexVector("psi");

  // Set the thickness field.
  std::shared_ptr<Nosh::ScalarField::Virtual> thickness(
      new Nosh::ScalarField::Constant(*mesh, 1.0)
      );

  std::shared_ptr<Nosh::VectorField::Virtual> mvp(
      new Nosh::VectorField::ExplicitValues(*mesh, "A", mu)
      );
  const std::shared_ptr<Nosh::ParameterMatrix::Virtual> keoBuilder(
      new Nosh::ParameterMatrix::Keo(mesh, thickness, mvp)
      );
  const std::shared_ptr<Nosh::ParameterMatrix::Virtual> DKeoDPBuilder(
      new Nosh::ParameterMatrix::DKeoDP(mesh, thickness, mvp, "mu")
      );

  std::shared_ptr<Nosh::ScalarField::Virtual> sp(
      new Nosh::ScalarField::Constant(*mesh, -1.0)
      );

  Teuchos::RCP<Nosh::ModelEvaluator::Nls> modelEvalE =
    Teuchos::rcp(new Nosh::ModelEvaluator::Nls(
          mesh,
          keoBuilder,
          DKeoDPBuilder,
          sp,
          1.0,
          thickness,
          z
          ));

  Teuchos::RCP<Thyra::ModelEvaluator<double> > modelEval =
    Thyra::epetraModelEvaluator(modelEvalE, Teuchos::null);

  Teuchos::RCP<const Thyra::VectorSpaceBase<double> > vectorSpace =
    Thyra::create_VectorSpace(modelEvalE->get_x_map());

  // Create inArgs. Use p_init as parameters.
  Thyra::ModelEvaluatorBase::InArgs<double> inArgs =
    modelEval->createInArgs();
  inArgs.set_x(Thyra::create_Vector(Teuchos::rcp(z), vectorSpace));
  //inArgs.set_p(0, modelEvalT->get_p_init(0));

  // Create outArgs.
  Thyra::ModelEvaluatorBase::OutArgs<double> outArgs =
    modelEval->createOutArgs();
  Teuchos::RCP<Epetra_Vector> f = Teuchos::rcp(new Epetra_Vector(*modelEvalE->get_f_map()));
  Teuchos::RCP<const Thyra::VectorSpaceBase<double> > vectorSpaceF =
    Thyra::create_VectorSpace(modelEvalE->get_f_map());
  outArgs.set_f(Thyra::create_Vector(f, vectorSpaceF));

  // Fetch.
  modelEval->evalModel(inArgs, outArgs);

  // check the norms
  double normOne;
  TEUCHOS_ASSERT_EQUALITY(0, f->Norm1(&normOne));
  TEST_FLOATING_EQUALITY(normOne, controlNormOne, 1.0e-10);

  double normTwo;
  TEUCHOS_ASSERT_EQUALITY(0, f->Norm2(&normTwo));
  TEST_FLOATING_EQUALITY(normTwo, controlNormTwo, 1.0e-10);

  double normInf;
  TEUCHOS_ASSERT_EQUALITY(0, f->NormInf(&normInf));
  // Relax the tolerance just a little bit here.
  TEST_FLOATING_EQUALITY(normInf, controlNormInf, 1.0e-9);

  return;
}
// ===========================================================================
//TEUCHOS_UNIT_TEST(Nosh, ComputeFRectangleSmallHashes)
//{
//  std::string inputFileNameBase = "rectanglesmall";
//
//  double mu = 1.0e-2;
//  double controlNormOne = 0.50126061034211067;
//  double controlNormTwo = 0.24749434381636057;
//  double controlNormInf = 0.12373710977782607;
//
//  testComputeF(inputFileNameBase,
//               mu,
//               controlNormOne,
//               controlNormTwo,
//               controlNormInf,
//               out,
//               success);
//}
// ============================================================================
TEUCHOS_UNIT_TEST(Nosh, ComputeFPacmanHashes)
{
  std::string inputFileNameBase = "pacman";

  double mu = 1.0e-2;
  double controlNormOne = 0.71366475047893463;
  double controlNormTwo = 0.12552206259336218;
  double controlNormInf = 0.055859319123267033;

  testComputeF(inputFileNameBase,
               mu,
               controlNormOne,
               controlNormTwo,
               controlNormInf,
               out,
               success);
}
// ============================================================================
//TEUCHOS_UNIT_TEST(Nosh, ComputeFCubeSmallHashes)
//{
//  std::string inputFileNameBase = "cubesmall";
//
//  double mu = 1.0e-2;
//  double controlNormOne = 8.3541623156163313e-05;
//  double controlNormTwo = 2.9536515963905867e-05;
//  double controlNormInf = 1.0468744547749431e-05;
//
//  testComputeF(inputFileNameBase,
//               mu,
//               controlNormOne,
//               controlNormTwo,
//               controlNormInf,
//               out,
//               success);
//}
// ============================================================================
TEUCHOS_UNIT_TEST(Nosh, ComputeFBrickWHoleHashes)
{
  std::string inputFileNameBase = "brick-w-hole";

  double mu = 1.0e-2;
  double controlNormOne = 1.8084716102419285;
  double controlNormTwo = 0.15654267585120338;
  double controlNormInf = 0.03074423493622647;

  testComputeF(inputFileNameBase,
               mu,
               controlNormOne,
               controlNormTwo,
               controlNormInf,
               out,
               success);
}
// ============================================================================
} // namespace
