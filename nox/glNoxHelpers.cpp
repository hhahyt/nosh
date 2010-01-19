/*
 * glNoxHelpers.cpp
 *
 *  Created on: Jan 17, 2010
 *      Author: Nico Schloemer
 */

#include <Teuchos_RCP.hpp>
#include <Teuchos_Comm.hpp>
#include <Teuchos_ParameterList.hpp>

#include <Epetra_Comm.h>

#include <Tpetra_Vector.hpp>

#include <NOX.H>
#include <NOX_Epetra.H>

#include <NOX.H>
#include <NOX_Epetra_LinearSystem_AztecOO.H>
#include <NOX_Epetra.H>

// for the eigenvalue computation:
#include <AnasaziBasicEigenproblem.hpp>
#include <AnasaziBasicOutputManager.hpp>
#include <AnasaziBlockDavidsonSolMgr.hpp>
#include <AnasaziBlockKrylovSchurSolMgr.hpp>
#include <AnasaziLOBPCGSolMgr.hpp>
#include <AnasaziRTRSolMgr.hpp>
#include <AnasaziConfigDefs.hpp>
#include <AnasaziEpetraAdapter.hpp>
#include <AnasaziBasicSort.hpp>

#include <Teuchos_XMLParameterListHelpers.hpp>

#include "ioVirtual.h"
#include "ioFactory.h"

#include "glBoundaryConditionsVirtual.h"
#include "glBoundaryConditionsInner.h"
#include "glBoundaryConditionsOuter.h"
#include "glBoundaryConditionsCentral.h"
#include "ginzburgLandau.h"
#include "glPrePostOperator.h"
#include "GridUniformSquare.h"

#include "GridSquare.h"
#include "GlSystemWithConstraint.h"

namespace glNoxHelpers {
// =========================================================================
void
createGlSystem( const Teuchos::RCP<const Teuchos::Comm<int> > & comm,
		        const Teuchos::RCP<const Epetra_Comm>         & eComm,
		        const std::string                             & fileName,
		              Teuchos::ParameterList                  & problemParameters,
		              Teuchos::RCP<GlSystemWithConstraint>    & glSystem )
{
  Teuchos::ParameterList glParameters;
  Teuchos::RCP<ComplexVector> psi;
  Teuchos::RCP<GridUniformVirtual> grid;

  try
    {
      readStateFromFile(comm, fileName, psi, grid, problemParameters);
    }
  catch (std::exception & e )
    {
      std::cerr << e.what() << std::endl;
    }

  double scaling = problemParameters.get<double>("scaling");
  double H0      = problemParameters.get<double>("H0");

  Teuchos::RCP<MagneticVectorPotential> A =
                              Teuchos::rcp ( new MagneticVectorPotential( H0, scaling ) );
  Teuchos::RCP<GlBoundaryConditionsVirtual> boundaryConditions =
                             Teuchos::rcp ( new GlBoundaryConditionsCentral() );

  GinzburgLandau glProblem = GinzburgLandau( grid,
                                             A,
                                             boundaryConditions
                                           );

  // Create the interface between NOX and the application
  // This object is derived from NOX::Epetra::Interface
  glSystem =  Teuchos::rcp ( new GlSystemWithConstraint ( glProblem, eComm, psi ) );
}
// =============================================================================
void
createGlSystem( const Teuchos::RCP<const Teuchos::Comm<int> > & comm,
                const Teuchos::RCP<const Epetra_Comm>         & eComm,
                const unsigned int Nx,
                const double scaling,
                const double H0,
	                  Teuchos::ParameterList                  & problemParameters,
                      Teuchos::RCP<GlSystemWithConstraint>    & glSystem )
{
  problemParameters.set ( "Nx"     , Nx );
  problemParameters.set ( "scaling", scaling );
  problemParameters.set ( "H0"     , H0 );

  Teuchos::RCP<GlBoundaryConditionsVirtual> boundaryConditions =
                             Teuchos::rcp ( new GlBoundaryConditionsCentral() );

  Teuchos::RCP<GridUniformVirtual> grid = Teuchos::rcp ( new GridUniformSquare( Nx, scaling ) );

  Teuchos::RCP<MagneticVectorPotential> A
                             = Teuchos::rcp ( new MagneticVectorPotential(H0, scaling) );

  GinzburgLandau glProblem = GinzburgLandau( grid,
                                             A,
                                             boundaryConditions
                                           );

  glSystem = Teuchos::rcp ( new GlSystemWithConstraint ( glProblem, eComm ) );
}
// =========================================================================
void
setPrePostWriter(       Teuchos::ParameterList                  & noxParaList,
		          const Teuchos::RCP<const AbstractStateWriter> & asw,
                  const std::string                             & outputDir )
{
	using namespace Teuchos;
//   Teuchos::RCP<AbstractStateWriter> asw = glSystem_;
   RCP<NOX::Abstract::PrePostOperator> ppo =
              rcp ( new GlPrePostOperator ( asw,
                                            outputDir )
                    );
   noxParaList.sublist ( "Solver Options" )
              .set ( "User Defined Pre/Post Operator", ppo );
}
// =========================================================================
Teuchos::RCP<NOX::Epetra::Group>
createSolverGroup( const Teuchos::RCP<GlSystemWithConstraint> glSystem,
		           const Teuchos::RCP<Teuchos::ParameterList> nlParamsPtr )
{
	// Create all possible Epetra_Operators.
	Teuchos::RCP<Epetra_RowMatrix> Analytic = glSystem->getJacobian();

	// Create the linear system
	Teuchos::RCP<NOX::Epetra::Interface::Required> iReq = glSystem;
	Teuchos::RCP<NOX::Epetra::Interface::Jacobian> iJac = glSystem;

	Teuchos::ParameterList & printParams = nlParamsPtr->sublist( "Printing" );

	Teuchos::ParameterList & lsParams    = nlParamsPtr->sublist ( "Direction" )
	                                                   .sublist ( "Newton" )
		                                               .sublist ( "Linear Solver" );

	// Get initial solution
	Teuchos::RCP<Epetra_Vector> soln = glSystem->getSolution();

	Teuchos::RCP<NOX::Epetra::LinearSystemAztecOO> linSys =
			Teuchos::rcp ( new NOX::Epetra::LinearSystemAztecOO ( printParams,
					lsParams,
					iReq,
					iJac,
					Analytic,
					*soln ) );

	// Create the Group
	NOX::Epetra::Vector initialGuess ( soln, NOX::Epetra::Vector::CreateView );

	return Teuchos::rcp ( new NOX::Epetra::Group ( printParams,
			                                       iReq,
			                                       initialGuess,
			                                       linSys ) );
}
// =========================================================================
Teuchos::RCP<NOX::StatusTest::Generic>
createConvergenceTest( Teuchos::ParameterList & noxStatusList,
		               Teuchos::ParameterList & nlParamsPtr )
{
  NOX::StatusTest::Factory statusTestFactory;

  Teuchos::ParameterList & printParams = nlParamsPtr.sublist( "Printing" );
  NOX::Utils outputUtils( printParams );

  return statusTestFactory.buildStatusTests( noxStatusList, outputUtils ) ;
}
// =============================================================================
// Create the solver
Teuchos::RCP<NOX::Solver::Generic>
createSolver( const Teuchos::RCP<NOX::Epetra::Group>       grpPtr,
		      const Teuchos::RCP<NOX::StatusTest::Generic> statusTest,
		      const Teuchos::RCP<Teuchos::ParameterList>   nlParamsPtr )
{
	TEST_FOR_EXCEPTION( grpPtr.is_null(),
			            std::logic_error,
                        "Group not initialized." );

	TEST_FOR_EXCEPTION( statusTest.is_null(),
			            std::logic_error,
                        "Status test not initialized." );

	TEST_FOR_EXCEPTION( nlParamsPtr.is_null(),
         			    std::logic_error,
                        "Nonlinear solver parameters not initialized." );

    return NOX::Solver::buildSolver ( grpPtr,
                                      statusTest,
                                      nlParamsPtr );
}
// =========================================================================
double
computeJacobianConditionNumber( const Teuchos::RCP<const NOX::Solver::Generic> solver,
		                        const Teuchos::RCP<      NOX::Epetra::Group>   grpPtr )
{
  double kappa;
  try
    {
      const NOX::Epetra::Group& finalGroup =
        dynamic_cast<const NOX::Epetra::Group&> ( solver->getSolutionGroup() );
      grpPtr->computeJacobian();
      grpPtr->computeJacobianConditionNumber ( 2000, 1e-2, 30, true );
      kappa = finalGroup.getJacobianConditionNumber();
    }
  catch ( std::exception& e )
    {
      std::cerr << e.what() << std::endl;
    }

  return kappa;
}
// =============================================================================
// compute some eigenvalues using anasazi
void
computeJacobianEigenvalues( const Teuchos::RCP<const NOX::Solver::Generic> solver,
		                    const Teuchos::RCP<      NOX::Epetra::Group>   grpPtr,
		                    const int MyPID )
{
  bool debug = true; // even more increased verbosity

  std::string which ( "LR" ); // compute the rightmost eigenvalues

  bool boolret;

  typedef double ScalarType;
  typedef Teuchos::ScalarTraits<ScalarType>          SCT;
  typedef SCT::magnitudeType               MagnitudeType;
  typedef Epetra_MultiVector                          MV;
  typedef Epetra_Operator                             OP;
  typedef Anasazi::MultiVecTraits<ScalarType,MV>     MVT;
  typedef Anasazi::OperatorTraits<ScalarType,MV,OP>  OPT;

  const NOX::Epetra::Group& finalGroup =
        dynamic_cast<const NOX::Epetra::Group&> ( solver->getSolutionGroup() );

  const Epetra_Vector& finalSolution =
            ( dynamic_cast<const NOX::Epetra::Vector&> ( finalGroup.getX() ) ).
                                                              getEpetraVector();

  Epetra_BlockMap Map = finalSolution.Map();

  // shortname for the final Jacobian
  Teuchos::RCP<Epetra_Operator> J =
            grpPtr->getLinearSystem()->getJacobianOperator();

  // - - - - - - - - - - - - - - - - -
  // Start the block Arnoldi iteration
  // - - - - - - - - - - - - - - - - -
  // Variables used for the Block Krylov Schur Method
  int nev = 10;
  int blockSize = 1;
  int numBlocks = 30;
  int maxRestarts = 250;
  double tol = 1e-10;

  // Create a sort manager to pass into the block Krylov-Schur solver manager
  // -->  Make sure the reference-counted pointer is of type Anasazi::SortManager<>
  // -->  The block Krylov-Schur solver manager uses Anasazi::BasicSort<> by default,
  //      so you can also pass in the parameter "Which", instead of a sort manager.
  Teuchos::RCP<Anasazi::SortManager<MagnitudeType> > MySort =
	    Teuchos::rcp ( new Anasazi::BasicSort<MagnitudeType> ( which ) );

  // Set verbosity level
  int verbosity = Anasazi::Errors + Anasazi::Warnings;
  verbosity += Anasazi::FinalSummary + Anasazi::TimingDetails;
  if ( debug )
    {
      verbosity += Anasazi::Debug;
    }

  // Create parameter list to pass into solver manager
  Teuchos::ParameterList MyPL;
  MyPL.set ( "Verbosity", verbosity );
  MyPL.set ( "Sort Manager", MySort );
  //MyPL.set( "Which", which );
  MyPL.set ( "Block Size", blockSize );
  MyPL.set ( "Num Blocks", numBlocks );
  MyPL.set ( "Maximum Restarts", maxRestarts );
  //MyPL.set( "Step Size", stepSize );
  MyPL.set ( "Convergence Tolerance", tol );

  // Create an Epetra_MultiVector for an initial vector to start the solver.
  // Note:  This needs to have the same number of columns as the blocksize.
  Teuchos::RCP<Epetra_MultiVector> ivec =
		  Teuchos::rcp ( new Epetra_MultiVector ( Map, blockSize ) );
  ivec->Random();

  // Create the eigenproblem.
  Teuchos::RCP<Anasazi::BasicEigenproblem<double, MV, OP> > MyProblem =
    Teuchos::rcp ( new Anasazi::BasicEigenproblem<double, MV, OP> ( J, ivec ) );

  // Set the number of eigenvalues requested
  MyProblem->setNEV ( nev );

  // Inform the eigenproblem that you are finishing passing it information
  boolret = MyProblem->setProblem();
  TEST_FOR_EXCEPTION( !boolret,
		              std::runtime_error,
                      "Anasazi::BasicEigenproblem::setProblem() returned with error." );

  // Initialize the Block Arnoldi solver
//       Anasazi::BlockDavidsonSolMgr<double, MV, OP>
//       Anasazi::LOBPCGSolMgr<double, MV, OP>
//       Anasazi::RTRSolMgr<double, MV, OP>
  Anasazi::BlockKrylovSchurSolMgr<double, MV, OP>
					        MySolverMgr ( MyProblem, MyPL );

  // Solve the problem to the specified tolerances or length
  Anasazi::ReturnType returnCode = MySolverMgr.solve();
  if ( returnCode != Anasazi::Converged && MyPID==0 )
    cout << "Anasazi::EigensolverMgr::solve() returned unconverged." << endl;

  // Get the Ritz values from the eigensolver
//       std::vector<Anasazi::Value<double> > ritzValues = MySolverMgr.getRitzValues();

//       // Output computed eigenvalues and their direct residuals
//       if (MyPID==0) {
//         int numritz = (int)ritzValues.size();
//         cout.setf(std::ios_base::right, std::ios_base::adjustfield);
//         cout<<endl<< "Computed Ritz Values"<< endl;
//         if (MyProblem->isHermitian()) {
//           cout<< std::setw(16) << "Real Part"
//             << endl;
//           cout<<"-----------------------------------------------------------"<<endl;
//           for (int i=0; i<numritz; i++) {
//             cout<< std::setw(16) << ritzValues[i].realpart
//               << endl;
//           }
//           cout<<"-----------------------------------------------------------"<<endl;
//         }
//         else {
//           cout<< std::setw(16) << "Real Part"
//             << std::setw(16) << "Imag Part"
//             << endl;
//           cout<<"-----------------------------------------------------------"<<endl;
//           for (int i=0; i<numritz; i++) {
//             cout<< std::setw(16) << ritzValues[i].realpart
//               << std::setw(16) << ritzValues[i].imagpart
//               << endl;
//           }
//           cout<<"-----------------------------------------------------------"<<endl;
//           }
//         }

  // Get the eigenvalues and eigenvectors from the eigenproblem
  Anasazi::Eigensolution<ScalarType,MV> sol = MyProblem->getSolution();
  std::vector<Anasazi::Value<ScalarType> > evals = sol.Evals;
  Teuchos::RCP<MV> evecs = sol.Evecs;
  std::vector<int> index = sol.index;
  int numev = sol.numVecs;

  Teuchos::RCP<Epetra_Vector> ev1 ( ( *evecs ) ( 0 ) );

//   glsystem->solutionToFile( *ev1,
//                             problemParameters,
//                             "data/ev1.vtk" );
//
//       cout<< std::setw(16) << "Real Part"
//           << std::setw(16) << "Imag Part" << endl;
//       cout<<"-----------------------------------------------------------"<<endl;
//       for (int i=0; i<numev; i++) {
//           cout<< std::setw(16) << evals[i].realpart
//             << std::setw(16) << evals[i].imagpart << endl;
//       }
  // -----------------------------------------------------------------------
}
// =========================================================================
void
printSolutionToFile( const Teuchos::RCP<const NOX::Solver::Generic> solver,
		             const Teuchos::RCP<const GlSystemWithConstraint> glSystem,
		             const std::string & fileName )
{
  const NOX::Epetra::Group& finalGroup =
	dynamic_cast<const NOX::Epetra::Group&> ( solver->getSolutionGroup() );

  const Epetra_Vector& finalSolution =
	    ( dynamic_cast<const NOX::Epetra::Vector&> ( finalGroup.getX() ) ).
							      getEpetraVector();

  glSystem->writeSolutionToFile ( finalSolution,
                                  fileName );

}
// =========================================================================
int
checkConvergence( const Teuchos::RCP<const NOX::Solver::Generic> solver )
{
  int status = 0;

//   // 1. Convergence
//   if ( solvStatus != NOX::StatusTest::Converged ) {
//       status = 1;
//       if ( printing.isPrintType ( NOX::Utils::Error ) )
//         printing.out() << "Nonlinear solver failed to converge!" << endl;
//   }

#ifndef HAVE_MPI
  // 2. Linear solve iterations (53) - SERIAL TEST ONLY!
  //    The number of linear iterations changes with # of procs.
  if ( const_cast<Teuchos::ParameterList&> ( solver->getList() )
                                           .sublist ( "Direction" )
                                           .sublist ( "Newton" )
                                           .sublist ( "Linear Solver" )
                                           .sublist ( "Output" )
                                           .get ( "Total Number of Linear Iterations",0 )
     != 53 )
    {
      status = 2;
    }
#endif

//  // 3. Nonlinear solve iterations (10)
//  if ( const_cast<Teuchos::ParameterList&> ( solver_->getList() )
//                                             .sublist ( "Output" )
//                                             .get ( "Nonlinear Iterations", 0 )
//        == maxNonlinearIterations_ )
//    status = 3;

  return status;
}
// =========================================================================
} // namespace glNoxHelpers