#include <boost/filesystem.hpp>

#include <LOCA.H>
#include <LOCA_Epetra_Factory.H>
#include <LOCA_Epetra_Group.H>

#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

#include <Epetra_Vector.h>
#include <NOX_Epetra_LinearSystem_AztecOO.H>

#include <LOCA_StatusTest_MaxIters.H>

#include <Teuchos_ParameterList.hpp>
#include <Teuchos_XMLParameterListHelpers.hpp>
#include <Teuchos_CommandLineProcessor.hpp>
#include <Teuchos_DefaultComm.hpp>

#include "ioFactory.h"

#include "ginzburgLandau.h"

#include "glSystem.h"
#include "glBoundaryConditionsInner.h"
#include "glBoundaryConditionsOuter.h"
#include "glBoundaryConditionsCentral.h"
#include "eigenSaver.h"

#include "GridUniformSquare.h"

int
main(int argc, char *argv[])
{

  // Initialize MPI
#ifdef HAVE_MPI
  MPI_Init(&argc,&argv);
#endif

  // create Epetra communicator
#ifdef HAVE_MPI
  Teuchos::RCP<Epetra_MpiComm> eComm =
  Teuchos::rcp<Epetra_MpiComm> ( new Epetra_MpiComm ( MPI_COMM_WORLD ) );
#else
  Teuchos::RCP<Epetra_SerialComm> eComm = Teuchos::rcp<Epetra_SerialComm>(
      new Epetra_SerialComm());
#endif

  // Create a communicator for Tpetra objects
  const Teuchos::RCP<const Teuchos::Comm<int> > Comm =
      Teuchos::DefaultComm<int>::getComm();

  // =========================================================================
  // handle command line arguments
  Teuchos::CommandLineProcessor My_CLP;

  My_CLP.setDocString(
      "This program does continuation for the Ginzburg--Landau problem with a LOCA interface.\n"
        "It is possible to give an initial guess in VTK format on the command line.\n");

  std::string xmlInputFileName = "";
  My_CLP.setOption( "xml-input-file", &xmlInputFileName,
                    "XML file containing the parameter list", true);

  // print warning for unrecognized arguments
  My_CLP.recogniseAllOptions(true);

  // don't throw exceptions
  My_CLP.throwExceptions(true);

  // finally, parse the stuff!
  Teuchos::CommandLineProcessor::EParseCommandLineReturn parseReturn;
  try {
      parseReturn = My_CLP.parse(argc, argv);
  }
  catch (std::exception &e)
  {
      std::cerr << e.what() << std::endl;
      return 1;
  }


  Teuchos::RCP<Teuchos::ParameterList> paramList = Teuchos::rcp(
      new Teuchos::ParameterList);

  std::cout << "Reading parameter list from \"" << xmlInputFileName << "\"."
      << std::endl;

  try {
      Teuchos::updateParametersFromXmlFile(xmlInputFileName, paramList.get());
  }
  catch (std::exception &e)
  {
      std::cerr << e.what() << "\n"
                << "Check your XML file for syntax errors." << std::endl;
      return 1;
  }

  // =========================================================================
  // extract data for the output the parameter list
  Teuchos::ParameterList outputList;
  try {
	  outputList = paramList->sublist("Output", true);
  }
  catch (std::exception &e)
  {
      std::cerr << e.what() << std::endl;
      return 1;
  }
  // set default directory to be the directory of the XML file itself
  std::string outputDirectoryDefault = boost::filesystem::path(xmlInputFileName).branch_path().string();
  std::string outputDirectory = outputList.get<string> ("Output directory", outputDirectoryDefault );
  std::string contFileBaseName = outputList.get<string> (
      "Continuation file base name");
  std::string contFileFormat = outputList.get<string> ("Continuation file format");
  std::string contDataFileName = outputList.get<string> (
      "Continuation data file name");
  // =========================================================================


  // ---------------------------------------------------------------------------
  Teuchos::ParameterList glParameters;
  Teuchos::RCP<ComplexVector> psi;
  Teuchos::RCP<ComplexVector> tangent;
  Teuchos::RCP<GridUniformVirtual> grid;

  Teuchos::ParameterList initialGuessList;
  try {
	  initialGuessList = paramList->sublist("Initial guess", true);
  }
  catch (std::exception &e)
  {
      std::cerr << e.what() << std::endl;
      return 1;
  }
  std::string stateFile   = initialGuessList.get<string> ("State file", "");
  std::string tangentFile = initialGuessList.get<string> ("Tangent file", "");


  if ( !inputGuessFile.empty() )
    {
      try
        {
          readStateFromFile(Comm, stateFile  , psi    , grid, glParameters);
          readStateFromFile(Comm, tangentFile, tangent, grid, glParameters);
        }
      catch (std::exception &e)
        {
          std::cerr << e.what() << std::endl;
          return 1;
        }

	  if ( initialGuessList.isParameter("Nx") ) {
		  std::cerr << "Warning: Parameter 'Nx' *and* input guess file given."
				    << "Using value as in input guess file, discarding 'Nx'."
				    << std::endl;
	  }
	  if ( initialGuessList.isParameter("scaling") ) {
		  std::cerr << "Warning: Parameter 'scaling' *and* input guess file given."
				    << "Using value as in input guess file, discarding 'scaling'."
				    << std::endl;
	  }
	  if ( initialGuessList.isParameter("H0") ) {
		  std::cerr << "Warning: Parameter 'H0' *and* input guess file given."
				    << "Using value as in input guess file, discarding 'H0'."
				    << std::endl;
	  }

    }
  else // no or empty input guess file
    {
      throw "Exception!";
    }
  // ---------------------------------------------------------------------------

  Teuchos::ParameterList & stepperList = paramList->sublist("LOCA").sublist("Stepper");

  // set the initial value from glParameters
  std::string contParam = stepperList.get<string>("Continuation Parameter", "");
  TEST_FOR_EXCEPTION( !glParameters.isParameter(contParam),
		              std::logic_error,
		              "Parameter \"" << contParam << "\" given as continuation parameter, but doesn't exist"
		              << "in the glParameters list." );

  // check if the initial value was given (will be unused anyway)
  if ( stepperList.isParameter("Initial Value") ) {
	  std::cerr << "Warning: Parameter 'LOCA->Stepper->Initial Value' given, but will not be used."
			    << std::endl;
  }

  // TODO Get rid of the explicit "double".
  stepperList.set("Initial Value", glParameters.get<double>(contParam) );

  // create the gl problem
  Teuchos::RCP<GlBoundaryConditionsVirtual> boundaryConditions = Teuchos::rcp(
      new GlBoundaryConditionsCentral());

  Teuchos::RCP<MagneticVectorPotential> A = Teuchos::rcp(
      new MagneticVectorPotential(glParameters.get<double> ("H0"),
          glParameters.get<double> ("scaling")));

  // upcast to GridUniformVirtual
  Teuchos::RCP<GridUniformVirtual> gridV = grid;
  GinzburgLandau glProblem = GinzburgLandau(gridV, A, boundaryConditions);

  Teuchos::RCP<GlSystem> glsystem =
      Teuchos::rcp(new GlSystem(glProblem, eComm, psi, outputDirectory,
          contDataFileName, contFileFormat, contFileBaseName));

  // ---------------------------------------------------------------------------
  // Create the necessary objects
  // ---------------------------------------------------------------------------
  // Create Epetra factory
  Teuchos::RCP<LOCA::Abstract::Factory> epetraFactory = Teuchos::rcp(
      new LOCA::Epetra::Factory);

  // Create global data object
  Teuchos::RCP<LOCA::GlobalData> globalData = LOCA::createGlobalData(paramList,
      epetraFactory);

  // get the initial solution
  Teuchos::RCP<Epetra_Vector> soln = glsystem->getSolution();
  // ---------------------------------------------------------------------------


#ifdef HAVE_LOCA_ANASAZI
  Teuchos::ParameterList& eigenList = paramList->sublist("LOCA").sublist(
      "Stepper") .sublist("Eigensolver");
  Teuchos::RCP<Teuchos::ParameterList> eigenListPtr = Teuchos::rcpFromRef(
      (eigenList));
  std::string eigenvaluesFileName =
      outputList.get<string> ("Eigenvalues file name");
  std::string eigenstateFileNameAppendix = outputList.get<string> (
      "Eigenstate file name appendix");
  Teuchos::RCP<EigenSaver> glEigenSaver = Teuchos::RCP<EigenSaver>(
      new EigenSaver(eigenListPtr, outputDirectory,
          eigenvaluesFileName, contFileBaseName, eigenstateFileNameAppendix,
          glsystem));

  Teuchos::RCP<LOCA::SaveEigenData::AbstractStrategy> glSaveEigenDataStrategy =
      glEigenSaver;
  eigenList.set("glSaveEigenDataStrategy", glSaveEigenDataStrategy);
#endif

  // ---------------------------------------------------------------------------
  // Create all possible Epetra_Operators.
  Teuchos::RCP<Epetra_RowMatrix> J = glsystem->getJacobian();
  Teuchos::RCP<Epetra_RowMatrix> M = glsystem->getPreconditioner();

  // Create the linear system.
  // Use the TimeDependent interface for computation of shifted matrices.
  Teuchos::RCP<LOCA::Epetra::Interface::Required> iReq = glsystem;
  Teuchos::RCP<NOX::Epetra::Interface::Jacobian> iJac = glsystem;
  Teuchos::RCP<NOX::Epetra::Interface::Preconditioner> iPrec = glsystem;

  Teuchos::ParameterList& nlPrintParams = paramList->sublist("NOX") .sublist(
      "Printing");

  Teuchos::ParameterList& lsParams = paramList->sublist("NOX") .sublist(
      "Direction") .sublist("Newton") .sublist("Linear Solver");

//  Teuchos::RCP<NOX::Epetra::LinearSystemAztecOO> linSys = Teuchos::rcp(
//      new NOX::Epetra::LinearSystemAztecOO(nlPrintParams, lsParams, iJac, J, iPrec, M, *soln));

  Teuchos::RCP<NOX::Epetra::LinearSystemAztecOO> linSys = Teuchos::rcp(
      new NOX::Epetra::LinearSystemAztecOO(nlPrintParams, lsParams, iReq, iJac, J, *soln));

  Teuchos::RCP<LOCA::Epetra::Interface::TimeDependent> iTime = glsystem;
  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Create a group which uses that problem interface. The group will
  // be initialized to contain the default initial guess for the
  // specified problem.
  LOCA::ParameterVector locaParams;

  locaParams.addParameter("H0", glParameters.get<double> ("H0"));
  locaParams.addParameter("scaling", glParameters.get<double> (
      "scaling"));

  NOX::Epetra::Vector initialGuess(soln, NOX::Epetra::Vector::CreateView);

  Teuchos::RCP<LOCA::Epetra::Group> grp = Teuchos::rcp(new LOCA::Epetra::Group(
      globalData, nlPrintParams, iTime, initialGuess, linSys, linSys,
      locaParams));

  grp->setParams(locaParams);
  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Get the vector from the Problem
  Teuchos::RCP<NOX::Epetra::Vector> noxSoln = Teuchos::rcp(
      new NOX::Epetra::Vector(soln, NOX::Epetra::Vector::CreateView));
  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Set up the NOX status tests
  // ---------------------------------------------------------------------------
  Teuchos::ParameterList& noxList = paramList->sublist("NOX", true);
  double tol = noxList.get<double> ("Tolerance");
  int maxNonlinarSteps = noxList.get<int> ("Max steps");
  Teuchos::RCP<NOX::StatusTest::NormF> normF = Teuchos::rcp(
      new NOX::StatusTest::NormF(tol));
  Teuchos::RCP<NOX::StatusTest::MaxIters> maxIters = Teuchos::rcp(
      new NOX::StatusTest::MaxIters(maxNonlinarSteps));
  Teuchos::RCP<NOX::StatusTest::Generic> comboOR = Teuchos::rcp(
      new NOX::StatusTest::Combo(NOX::StatusTest::Combo::OR, normF, maxIters));
  // ---------------------------------------------------------------------------


  // ---------------------------------------------------------------------------
  // Set up the LOCA status tests
  // ---------------------------------------------------------------------------
  int maxLocaSteps = 5000;
  Teuchos::RCP<LOCA::StatusTest::MaxIters> maxLocaStepsTest = Teuchos::rcp(
      new LOCA::StatusTest::MaxIters(maxLocaSteps));
  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // Create the stepper
  Teuchos::RCP<LOCA::Stepper> stepper = Teuchos::rcp(new LOCA::Stepper(
      globalData, grp, maxLocaStepsTest, comboOR, paramList));
//  Teuchos::RCP<LOCA::Stepper> stepper = Teuchos::rcp(new LOCA::Stepper(
//      globalData, grp, comboOR, paramList));
  // ---------------------------------------------------------------------------

  // make sure that the stepper starts off with the correct starting value

  // pass pointer to stepper to glSystem to be able to read stats from the stepper in there
  glsystem->setLocaStepper(stepper);
#ifdef HAVE_LOCA_ANASAZI
  glEigenSaver->setLocaStepper(stepper);
#endif

  // ---------------------------------------------------------------------------
  // Perform continuation run
  LOCA::Abstract::Iterator::IteratorStatus status;
  try
    {
      status = stepper->run();
    }
  catch (char const* e)
    {
      std::cerr << "Exception raised: " << e << std::endl;
    }
  // ---------------------------------------------------------------------------

  // clean up
  LOCA::destroyGlobalData(globalData);
  glsystem->releaseLocaStepper();
#ifdef HAVE_LOCA_ANASAZI
  glEigenSaver->releaseLocaStepper();
#endif

#ifdef HAVE_MPI
  MPI_Finalize();
#endif

  // Final return value (0 = successful, non-zero = failure)
  return status;
}