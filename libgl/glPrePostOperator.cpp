#include "glPrePostOperator.h"

#include "glSystem.h"

#include <Teuchos_ParameterList.hpp>
#include <NOX_Solver_Generic.H>

#include <NOX_Epetra_Group.H>

#include <EpetraExt_Utils.h> // for toString

// =============================================================================
GlPrePostOperator::GlPrePostOperator( Teuchos::RCP<GlSystem>       & glsystem,
                                      const Teuchos::ParameterList & problemParams,
                                      const std::string            & outputDir) :
  numRunPreIterate(0),
  problemParameters_( problemParams ),
  glsystem_(glsystem),
  outputDir_(outputDir)
{
}
// =============================================================================
GlPrePostOperator::~GlPrePostOperator()
{
}
// =============================================================================
void GlPrePostOperator::
runPreIterate(const NOX::Solver::Generic& solver)
{
  string fileName;

  ++numRunPreIterate;

  // Get the Epetra_Vector with the final solution from the solver
  const NOX::Epetra::Group& solGrp =
             dynamic_cast<const NOX::Epetra::Group&>(solver.getSolutionGroup());

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const Epetra_Vector& currentSol =
    (dynamic_cast<const NOX::Epetra::Vector&>(solGrp.getX())).
    getEpetraVector();
  fileName = outputDir_ + "/newton-sol-"+EpetraExt::toString(numRunPreIterate)+".vtk";
  glsystem_->writeSolutionToFile( currentSol,
                                  fileName );
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  const Epetra_Vector& currentResidual =
    (dynamic_cast<const NOX::Epetra::Vector&>(solGrp.getF())).getEpetraVector();
  fileName = outputDir_ + "/newton-res-"+EpetraExt::toString(numRunPreIterate)+".vtk";
  glsystem_->writeAbstractStateToFile( currentResidual,
                                       fileName );
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
}
// =============================================================================
