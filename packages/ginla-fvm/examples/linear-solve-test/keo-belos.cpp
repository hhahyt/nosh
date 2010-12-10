#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

#include <Teuchos_CommandLineProcessor.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_XMLParameterListHelpers.hpp>
#include <Epetra_LinearProblem.h>

//#include <ml_epetra_preconditioner.h>

//#include "BelosConfigDefs.hpp"
#include <BelosLinearProblem.hpp>
#include <BelosEpetraAdapter.hpp>
//#include <BelosPseudoBlockGmresSolMgr.hpp>
#include <BelosBlockCGSolMgr.hpp>

#include <ml_epetra_preconditioner.h>

#include <boost/filesystem.hpp>

#include "Ginla_EpetraFVM_ModelEvaluator.h"
#include "Ginla_EpetraFVM_State.h"
#include "VIO_EpetraMesh_Reader.h"
#include "Ginla_EpetraFVM_KeoFactory.h"

#include "Ginla_MagneticVectorPotential_X.h"
#include "Ginla_MagneticVectorPotential_Y.h"
#include "Ginla_MagneticVectorPotential_Z.h"

// =============================================================================
typedef double                           ST;
typedef Epetra_MultiVector               MV;
typedef Epetra_Operator                  OP;
typedef Belos::MultiVecTraits<ST,MV>     MVT;
typedef Belos::OperatorTraits<ST,MV,OP>  OPT;
// =============================================================================
int main ( int argc, char *argv[] )
{
  // Initialize MPI
#ifdef HAVE_MPI
  MPI_Init( &argc, &argv );
#endif

    // Create a communicator for Epetra objects
#ifdef HAVE_MPI
    Teuchos::RCP<Epetra_MpiComm> eComm =
      Teuchos::rcp<Epetra_MpiComm> ( new Epetra_MpiComm ( MPI_COMM_WORLD ) );
#else
    Teuchos::RCP<Epetra_SerialComm>  eComm =
           Teuchos::rcp<Epetra_SerialComm> ( new Epetra_SerialComm() );
#endif

    int status = 0;
    Belos::ReturnType ret;

    try
    {
      // ===========================================================================
      // handle command line arguments
      Teuchos::CommandLineProcessor My_CLP;

      My_CLP.setDocString (
              "Linear solver testbed for KEO and Jacobian operator.\n"
      );

      std::string inputFileName( "" );
      My_CLP.setOption ( "input", &inputFileName, "Input state file", true );

      bool verbose = true;
      My_CLP.setOption("verbose","quiet",&verbose,"Print messages and results.");

      int frequency = 5;
      My_CLP.setOption("frequency",&frequency,"Solvers frequency for printing residuals (#iters).");

      // print warning for unrecognized arguments
      My_CLP.recogniseAllOptions ( true );

      // finally, parse the command line
      TEUCHOS_ASSERT_EQUALITY( My_CLP.parse ( argc, argv ),
                               Teuchos::CommandLineProcessor::PARSE_SUCCESSFUL
                             );
      // =========================================================================
      // Only print on the zero processor
      bool proc_verbose = verbose && (eComm->MyPID()==0);

      Teuchos::ParameterList              problemParameters;
      Teuchos::RCP<Epetra_Vector>         z = Teuchos::null;
      Teuchos::RCP<VIO::EpetraMesh::Mesh> mesh = Teuchos::null;

      VIO::EpetraMesh::read( eComm,
                             inputFileName,
                             z,
                             mesh,
                             problemParameters
                           );

      double mu = problemParameters.get<double>( "mu" );
      Teuchos::RCP<Ginla::MagneticVectorPotential::Virtual> mvp =
              Teuchos::rcp ( new Ginla::MagneticVectorPotential::Z ( mu ) );

      // create the kinetic energy operator
      Teuchos::RCP<Ginla::EpetraFVM::KeoFactory> keoFactory =
              Teuchos::rcp( new Ginla::EpetraFVM::KeoFactory( mesh, mvp ) );
      Teuchos::RCP<Epetra_FECrsMatrix> keoMatrix =
              Teuchos::rcp( new Epetra_FECrsMatrix( Copy, keoFactory->buildKeoGraph() ) );
      Teuchos::Tuple<double,3> scaling( Teuchos::tuple(1.0,1.0,1.0) );
      keoFactory->buildKeo( *keoMatrix, mu, scaling );

      // Make sure the matrix is indeed positive definite, and not
      // negative definite. Belos needs that (2010-11-05).
      keoMatrix->Scale( -1.0 );

      // create initial guess and right-hand side
      Teuchos::RCP<Epetra_Vector> epetra_x =
              Teuchos::rcp( new Epetra_Vector( keoMatrix->OperatorDomainMap() ) );
      Teuchos::RCP<Epetra_MultiVector> epetra_b =
              Teuchos::rcp( new Epetra_Vector( keoMatrix->OperatorRangeMap(), 1 ) );
      epetra_b->Random();

//      // check that the matrix is indeed symmetric
//      for ( int i=0; i<epetra_b->GlobalLength(); i++ )
//      {
//          int numEntries = 0;
//          double *values;
//          int *indices;
//          keoMatrix->ExtractMyRowView( i, numEntries, values, indices );
//          for ( int k=0; k<numEntries; k++ )
//          {
//              int j = indices[k];
//              if ( j>i )
//              {
//                  // get element [j,i]
//                  int numEntries2 = 0;
//                  double *values2;
//                  int *indices2;
//                  keoMatrix->ExtractMyRowView( j, numEntries2, values2, indices2 );
//                  // check if element [j,i] is there at all
//                  int kk = -1;
//                  for ( int k2=0; k2<numEntries2; k2++ )
//                  {
//                      if ( indices2[k2] == i )
//                      {
//                          kk = k2;
//                          break;
//                      }
//                  }
//                  // make sure it's been found
//                  TEUCHOS_ASSERT_INEQUALITY( kk, !=, -1 );
//                  // check that values are the same
//                  TEUCHOS_ASSERT_EQUALITY( values[k], values2[kk] );
//              }
//          }
//      }
//      std::cout << "Matrix appears to be symmetric" << std::endl;
      // -----------------------------------------------------------------------
      // Belos part
      ParameterList belosList;
      belosList.set( "Convergence Tolerance", 1.0e-10 );         // Relative convergence tolerance requested
      if (verbose) {
        belosList.set( "Verbosity", Belos::Errors + Belos::Warnings +
                       Belos::TimingDetails + Belos::StatusTestDetails );
        if (frequency > 0)
          belosList.set( "Output Frequency", frequency );
      }
      else
        belosList.set( "Verbosity", Belos::Errors + Belos::Warnings );

      // Construct an unpreconditioned linear problem instance.
      Belos::LinearProblem<double,MV,OP> problem( keoMatrix, epetra_x, epetra_b );
      bool set = problem.setProblem();
      if (set == false) {
        if (proc_verbose)
          std::cout << std::endl << "ERROR:  Belos::LinearProblem failed to set up correctly!" << std::endl;
        return -1;
      }
      // -----------------------------------------------------------------------
      // create preconditioner
      Teuchos::ParameterList MLList;
      ML_Epetra::SetDefaults( "SA", MLList );
      MLList.set("ML output", 0);
      MLList.set("max levels", 10);
      MLList.set("increasing or decreasing", "increasing");
      MLList.set("aggregation: type", "Uncoupled");
      MLList.set("smoother: type", "Chebyshev"); // "block Gauss-Seidel" "Chebyshev"
//      MLList.set("aggregation: threshold", 0.0);
      MLList.set("smoother: sweeps", 3);
      MLList.set("smoother: pre or post", "both");
      MLList.set("coarse: type", "Amesos-KLU");
      MLList.set("PDE equations", 2);
      Teuchos::RCP<ML_Epetra::MultiLevelPreconditioner> MLPrec =
                  Teuchos::rcp( new ML_Epetra::MultiLevelPreconditioner(*keoMatrix, MLList) );
      MLPrec->PrintUnused(0);

      //
      Teuchos::RCP<Epetra_Operator> Prec =
              Teuchos::rcp(  new ML_Epetra::MultiLevelPreconditioner(*keoMatrix, MLList) );
      TEUCHOS_ASSERT( !Prec.is_null() );

      // Create the Belos preconditioned operator from the preconditioner.
      // NOTE:  This is necessary because Belos expects an operator to apply the
      //        preconditioner with Apply() NOT ApplyInverse().
      RCP<Belos::EpetraPrecOp> belosPrec = rcp( new Belos::EpetraPrecOp( Prec ) );
      problem.setLeftPrec( belosPrec );
      // -----------------------------------------------------------------------
      // Create an iterative solver manager.
      RCP< Belos::SolverManager<double,MV,OP> > newSolver
              = rcp( new Belos::BlockCGSolMgr<double,MV,OP>( rcp(&problem,false),
                                                             rcp(&belosList,false)
                                                           )
                   );

      // Perform solve
      ret = newSolver->solve();
      // -----------------------------------------------------------------------
      //
      // Compute actual residuals.
      //
      bool badRes = false;
      std::vector<double> actual_resids( 1 );
      std::vector<double> rhs_norm( 1 );
      Epetra_Vector resid( keoMatrix->OperatorRangeMap() );
      OPT::Apply( *keoMatrix, *epetra_x, resid );
      MVT::MvAddMv( -1.0, resid, 1.0, *epetra_b, resid );
      MVT::MvNorm( resid, actual_resids );
      MVT::MvNorm( *epetra_b, rhs_norm );
      if (proc_verbose) {
        std::cout<< "---------- Actual Residuals (normalized) ----------" <<std::endl<<std::endl;
        for ( int i=0; i<1; i++) {
          double actRes = actual_resids[i]/rhs_norm[i];
          std::cout << "Problem " << i << " : \t" << actRes << std::endl;
          if (actRes > 1.0e-10) badRes = true;
        }
      }
      // -----------------------------------------------------------------------
    }
    catch ( std::exception & e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << e.what() << std::endl;
        status += 10;
    }
    catch ( std::string & e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << e << std::endl;
        status += 10;
    }
    catch ( int e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << "Caught unknown exception code " << e <<  "." << std::endl;
        status += 10;
    }
    catch (...)
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << "Caught unknown exception." << std::endl;
        status += 10;
    }

#ifdef HAVE_MPI
      MPI_Finalize();
#endif

    return ret==Belos::Converged ? EXIT_SUCCESS : EXIT_FAILURE;
}
// =========================================================================
