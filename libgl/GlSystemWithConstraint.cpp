/*
 * GlSystemWithConstraint.cpp
 *
 *  Created on: Dec 16, 2009
 *      Author: Nico Schl�mer
 */

#include "GlSystemWithConstraint.h"

#include "ioFactory.h"

#include <complex>
#include <vector>

#include <Epetra_Export.h>
#include <Epetra_CrsMatrix.h>
#include <NOX_Utils.H>

#include <EpetraExt_RowMatrixOut.h>

#include <EpetraExt_Utils.h>

#include <Epetra_Map.h>

#include <Tpetra_Vector.hpp>
#include <Tpetra_MultiVector.hpp>

#include <Thyra_EpetraThyraWrappers.hpp>

#include <Teuchos_DefaultComm.hpp>

#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

// abbreviate the complex type name
typedef std::complex<double> double_complex;

// =============================================================================
// Default constructor
GlSystemWithConstraint::GlSystemWithConstraint( GinzburgLandau::GinzburgLandau &gl,
                    const Teuchos::RCP<const Epetra_Comm> eComm,
                    const Teuchos::RCP<ComplexVector> psi,
                    const std::string outputDir,
                    const std::string outputDataFileName,
                    const std::string outputFileFormat,
                    const std::string solutionFileNameBase,
                    const std::string nullvectorFileNameBase
                  ) :
        NumMyElements_(0),
        NumComplexUnknowns_(0),
        stepper_(0),
        Gl_(gl),
        EComm_(eComm),
        TComm_(0),
        regularRealMap_(0),
        extendedRealMap_(0),
        ComplexMap_(0),
        rhs_(0),
        Graph_(0),
        jacobian_(0),
        initialSolution_(0),
        outputDir_(outputDir),
        solutionFileNameBase_(solutionFileNameBase),
        nullvectorFileNameBase_(nullvectorFileNameBase),
        outputFileFormat_(outputFileFormat),
        outputDataFileName_(outputDataFileName),
        glKomplex_( Teuchos::rcp(new GlKomplex(eComm) ) )
{
  NumComplexUnknowns_ = Gl_.getNumUnknowns();

  // TODO Don't throw exception in constructor?
  TEST_FOR_EXCEPTION( !psi.is_valid_ptr(),
                      std::logic_error,
                      "Invalid pointer" );

  // TODO Don't throw exception in constructor?
  TEST_FOR_EXCEPTION( psi.is_null(),
                      std::logic_error,
                      "Input guess is null pointer" );

  // TODO There is (until now?) no way to convert a Teuchos::Comm (of psi)
  // to an Epetra_Comm (of the real valued representation of psi), so the
  // Epetra_Comm has to be generated explicitly, and two communicators are kept
  // side by side all the time. One must make sure that the two are actually
  // equivalent, which can be checked by Thyra's conversion method create_Comm.
  // TODO Is is actually necessary to have equivalent communicators on the
  // real-valued and the complex-valued side?
  // How to compare two communicators anyway?

  // create fitting Tpetra::Comm
  // TODO: move into initializer
  TComm_ = create_CommInt(EComm_);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Define maps and initialize solution.

  // TODO Don't throw exception in constructor?
  TEST_FOR_EXCEPTION( psi->getGlobalLength() != (unsigned int) NumComplexUnknowns_,
                      std::logic_error,
                      "Size of the initial guess vector ("
                      << psi->getGlobalLength()
                      << ") does not coincide with the number of unknowns ("
                      << NumComplexUnknowns_ << ")" );

  // psi->getMap() returns a CONST map
  ComplexMap_ = Teuchos::RCP<const Tpetra::Map<Thyra::Ordinal> >(psi->getMap());
        
  // do the rest of the initialization
  initialize( psi );
}
// =============================================================================
// constructor *without* initial guess
GlSystemWithConstraint::GlSystemWithConstraint(GinzburgLandau::GinzburgLandau &gl,
                   const Teuchos::RCP<const Epetra_Comm> eComm,
                   const std::string outputDir,
                   const std::string outputDataFileName,
                   const std::string outputFileFormat,
                   const std::string solutionFileNameBase,
                   const std::string nullvectorFileNameBase
                  ) :
NumMyElements_(0),
NumComplexUnknowns_(0),
stepper_(0),
Gl_(gl),
EComm_(eComm),
TComm_(0),
regularRealMap_(0),
extendedRealMap_(0),
ComplexMap_(0),
rhs_(0),
Graph_(0),
jacobian_(0),
initialSolution_(0),
outputDir_(outputDir),
solutionFileNameBase_(solutionFileNameBase),
nullvectorFileNameBase_(nullvectorFileNameBase),
outputFileFormat_(outputFileFormat),
outputDataFileName_(outputDataFileName),
glKomplex_( Teuchos::rcp(new GlKomplex(eComm) ) )
{
  NumComplexUnknowns_ = Gl_.getNumUnknowns();

  // TODO There is (until now?) no way to convert a Teuchos::Comm (of psi)
  // to an Epetra_Comm (of the real valued representation of psi), so the
  // Epetra_Comm has to be generated explicitly, and two communicators are kept
  // side by side all the time. One must make sure that the two are actually
  // equivalent, which can be checked by Thyra's conversion method create_Comm.
  // TODO Is is actually necessary to have equivalent communicators on the
  // real-valued and the complex-valued side?
  // How to compare two communicators anyway?

  // create fitting Tpetra::Comm
  // TODO: move into initializer
  TComm_ = create_CommInt(EComm_);

  // define complex map
  ComplexMap_ = Teuchos::rcp(new Tpetra::Map<Thyra::Ordinal>(NumComplexUnknowns_, 0, TComm_));

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // initialize solution
  Teuchos::RCP<ComplexVector> psi = Teuchos::rcp(new ComplexVector(
                  ComplexMap_));
  // TODO Move default initialization out to main file
  double_complex alpha(1.0, 0.0);
  psi->putScalar(alpha); // default initialization
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

  // do the rest of the initialization
  initialize( psi );
}
// =============================================================================
// Destructor
GlSystemWithConstraint::~GlSystemWithConstraint() {
        stepper_ = Teuchos::null;
}
// =============================================================================
void
GlSystemWithConstraint::initialize(const Teuchos::RCP<ComplexVector> psi) {

	Teuchos::RCP<Epetra_Vector> tmp = glKomplex_->complex2real(*psi);
	// Create the maps with and without phase constraint.
	regularRealMap_ = Teuchos::rcp( new Epetra_BlockMap(tmp->Map()) );
	createExtendedRealMap( *regularRealMap_ );

	initialSolution_ = Teuchos::rcp( new Epetra_Vector(*extendedRealMap_), true );
	for (int k=0; k<tmp->MyLength(); k++ ) {
	    initialSolution_->ReplaceMyValue( k, 0, (*tmp)[tmp->Map().GID(k)] );
	}
	int n = initialSolution_->GlobalLength();
	initialSolution_->ReplaceGlobalValue( n-1, 0, 0.0 );

	NumMyElements_ = extendedRealMap_->NumMyElements();

	// TODO Remove 'dummy'.
	// create the sparsity structure (graph) of the Jacobian
	// use x as DUMMY argument
	Epetra_Vector dummy(*extendedRealMap_);

	createJacobian(ONLY_GRAPH, dummy);

	// Allocate the sparsity pattern of the Jacobian matrix
	jacobian_ = Teuchos::rcp(new Epetra_CrsMatrix(Copy, *Graph_));
	jacobian_->FillComplete();

	preconditioner_ = Teuchos::rcp(new Epetra_CrsMatrix(Copy, *Graph_));
	preconditioner_->FillComplete();
}
// =============================================================================
int GlSystemWithConstraint::realIndex2complexIndex(const int realIndex) const {
        if (!(realIndex % 2)) // realIndex even
                return realIndex / 2;
        else
                return (realIndex - 1) / 2;
}
// =============================================================================
bool
GlSystemWithConstraint::computeF(const Epetra_Vector &x,
                                       Epetra_Vector &FVec,
                                 const NOX::Epetra::Interface::Required::FillType fillFlag)
{
  TEST_FOR_EXCEPTION( !regularRealMap_.is_valid_ptr() || regularRealMap_.is_null(),
                      std::logic_error,
                      "regularRealMap_ not properly initialized." );

  TEST_FOR_EXCEPTION( !extendedRealMap_.is_valid_ptr() || extendedRealMap_.is_null(),
                      std::logic_error,
                      "extendedRealMap_ not properly initialized." );

  // make sure that the input and output vectors are correctly mapped
  TEST_FOR_EXCEPTION( !x.Map().SameAs(*extendedRealMap_),
                      std::logic_error,
                      "Maps of x and the computed real-valued map do not coincide. "
                      << "Check, for example, the number of elements "
                      << "(" << x.Map().NumGlobalElements() << " for x vs. "
                      << extendedRealMap_->NumGlobalElements() << " for extendedRealMap_).");

  TEST_FOR_EXCEPTION( !FVec.Map().SameAs(*extendedRealMap_),
                      std::logic_error,
                      "Maps of FVec and the computed real-valued map do not coincide."
                      << "Check, for example, the number of elements "
                      << "(" << FVec.Map().NumGlobalElements() << " for FVec vs. "
                      << extendedRealMap_->NumGlobalElements() << " for extendedRealMap_).");

  // strip off the phase constraint
  // TODO replace by {im,ex}porter
  Epetra_Vector tmp(*regularRealMap_);
  for (int k=0; k<tmp.MyLength(); k++)
    tmp.ReplaceMyValue( k, 0, x[x.Map().GID(k)] );

  // convert x to psi
  const Teuchos::RCP<ComplexVector> psi = glKomplex_->real2complex(tmp);

  // compute the GL residual
  Teuchos::RCP<ComplexVector> res = Gl_.computeGlVector( psi );

  // transform back to fully real equation
  const Teuchos::RCP<Epetra_Vector> shortFVec = glKomplex_->complex2real(*res);

  // copy over and add phase condition
  // TODO replace by {im,ex}porter
  for (int k=0; k<shortFVec->MyLength(); k++) {
    FVec.ReplaceMyValue( k, 0, (*shortFVec)[shortFVec->Map().GID(k)] );
  }
  FVec.ReplaceGlobalValue( 2*NumComplexUnknowns_, 0, 0.0 );

  return true;
}
// =============================================================================
void
GlSystemWithConstraint::createExtendedRealMap( const Epetra_BlockMap & realMap  )
{
  // fill up realMapGIDs
  int numMyElements = realMap.NumMyElements();
  Teuchos::Array<int> myElements( numMyElements );
  realMap.MyGlobalElements( myElements.getRawPtr() );

  // Unconditionally put the phase constraint on the first process.
  int myPID = realMap.Comm().MyPID();
  if ( myPID==0 ) {
    int n = realMap.NumGlobalElements();
    // extend the GIDs by the phase constraint
    myElements.append( n );
  }

  int numGlobalElements = realMap.NumGlobalElements() + 1;
  extendedRealMap_ = Teuchos::rcp( new Epetra_Map(numGlobalElements,
                                                  myElements.length(),
                                                  myElements.getRawPtr(),
                                                  realMap.IndexBase(),
                                                  realMap.Comm() )
                                 );
  return;
}
// =============================================================================
bool GlSystemWithConstraint::computeJacobian(const Epetra_Vector &x, Epetra_Operator &Jac) {
  // compute the values of the Jacobian
  createJacobian(VALUES, x);

  // optimize storage
  jacobian_->FillComplete();

  // Sync up processors to be safe
  EComm_->Barrier();

  return true;
}
// =============================================================================
bool GlSystemWithConstraint::computePreconditioner( const Epetra_Vector    & x,
                                                    Epetra_Operator        & Prec,
                                                    Teuchos::ParameterList * precParams )
{
//  Epetra_Vector diag = x;
//  diag.PutScalar(1.0);
//  preconditioner_->ReplaceDiagonalValues( diag );

  TEST_FOR_EXCEPTION( true,
                      std::logic_error,
                      "Use explicit Jacobian only for this test problem!" );
  return true;
}
// =============================================================================
Teuchos::RCP<Epetra_Vector>
GlSystemWithConstraint::getSolution() const {
        return initialSolution_;
}
// =============================================================================
Teuchos::RCP<Epetra_CrsMatrix>
GlSystemWithConstraint::getJacobian() const {
        return jacobian_;
}
// =============================================================================
Teuchos::RCP<Epetra_CrsMatrix>
GlSystemWithConstraint::getPreconditioner() const {
        return preconditioner_;
}
/* =============================================================================
 // Of an equation system
 // \f[
 // A\psi + B \psi^* = b
 // \f]
 // where \f$A,B\in\mathbb{C}^{n\times n}\f$, \f$\psi, b\in\mathbb{C}^{n}\f$,
 // this routine constructs the corresponding real-valued equation system
 // \f[
 // \begin{pmatrix}
 // \Re{A}+\Re{B} & -\Im{A}+\Im{B}\\
// \Im{A}+\Im{B} &  \Re{A}-\Re{B}
 // \end{pmatrix}
 // \begin{pmatrix}
 // \Re{\psi}\\
// \Im{\psi}
 // \end{pmatrix}
 // =
 // \begin{pmatrix}
 // \Re{b}\\
// \Im{b}
 // \end{pmatrix}
 // \f].
 // It also incorporates a phase condition.
 */
bool GlSystemWithConstraint::createJacobian( const jacCreator      jc,
                                             const Epetra_Vector & x   )
{
  TEST_FOR_EXCEPTION( !extendedRealMap_.is_valid_ptr() || extendedRealMap_.is_null(),
                      std::logic_error,
                      "extendedRealMap_ not properly initialized." );

  Teuchos::RCP<ComplexVector> psi;

  vector<int> colIndA, colIndB;
  vector<double_complex> valuesA, valuesB;

  int *colInd = NULL, *colIndAReal = NULL, *colIndAImag = NULL, *colIndBReal =
      NULL, *colIndBImag = NULL;
  double *values = NULL, *valuesAReal = NULL, *valuesAImag = NULL,
         *valuesBReal = NULL, *valuesBImag = NULL;

        int ierr, k, complexRow, numEntries;

        Teuchos::ArrayRCP<const double_complex> psiView;

        if (jc == VALUES) {
            Epetra_Vector tmp(*regularRealMap_);
            for (int k=0; k<tmp.MyLength(); k++)
                tmp.ReplaceMyValue( k, 0, x[x.Map().GID(k)] );
            psi = glKomplex_->real2complex(tmp);
            psiView = psi->get1dView();

            jacobian_->PutScalar(0.0); // set the matrix to 0
        } else {
                // allocate the graph
                int approxNumEntriesPerRow = 1;
                Graph_ = Teuchos::rcp<Epetra_CrsGraph>(new Epetra_CrsGraph(Copy,
                                *extendedRealMap_, approxNumEntriesPerRow, false));
        }

        // Construct the Epetra Matrix
        for (int i = 0; i < NumMyElements_; i++) {
                int Row = extendedRealMap_->GID(i);

                if (Row == 2 * NumComplexUnknowns_) // phase condition
                {
                        // fill in phase condition stuff
                        numEntries = 2 * NumComplexUnknowns_;
                        colInd = new int[numEntries];
                        for (int k = 0; k < numEntries; k++)
                                colInd[k] = k;
                        if (jc == VALUES) // fill on columns and values
                        {
                                values = new double[numEntries];
                                for (int k = 0; k < NumComplexUnknowns_; k++) {
                                        values[2 * k] = -imag(psiView[k]);
                                        values[2 * k + 1] = real(psiView[k]);
                                }
                                // fill it in!
                                ierr = jacobian_->SumIntoGlobalValues(Row, numEntries, values,
                                                colInd);
                                delete[] values;
                                values = NULL;
                        } else // only fill the sparsity graph
                        {
                                Graph_->InsertGlobalIndices(Row, numEntries, colInd);
                        }
                        delete[] colInd;
                        colInd = NULL;

                } else // GL equations
                {
                        // get the values and column indices
                        // TODO: The same value is actually fetched twice in this loop
                        //       possibly consecutively: Once for the real, once for
                        //       the imaginary part of it.
                        if (!(Row % 2)) // Row even
                                complexRow = Row / 2;
                        else
                                complexRow = (Row - 1) / 2;

                        if (jc == VALUES) {
                                // fill on columns and values
                                Gl_.getJacobianRow(complexRow, psi, colIndA, valuesA, colIndB,
                                                valuesB);
                        } else {
                                // only fill the sparsity graph
                                Gl_.getJacobianRowSparsity(complexRow, colIndA, colIndB);
                        }

                        if (!(Row % 2)) // myGlobalIndex is even <=> Real part of the equation system
                        {
                                // ---------------------------------------------------------------
                                // insert the coefficients Re(A) of Re(psi)
                                numEntries = colIndA.size();
                                colIndAReal = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndAReal[k] = 2 * colIndA[k];

                                if (jc == VALUES) {
                                        valuesAReal = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesAReal[k] = real(valuesA[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesAReal, colIndAReal);
                                        delete[] valuesAReal;
                                        valuesAReal = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndAReal);
                                }
                                delete[] colIndAReal;
                                colIndAReal = NULL;

                                // insert the coefficients Re(B) of Re(psi)
                                numEntries = colIndB.size();
                                colIndBReal = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndBReal[k] = 2 * colIndB[k];
                                if (jc == VALUES) {
                                        valuesBReal = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesBReal[k] = real(valuesB[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesBReal, colIndBReal);
                                        delete[] valuesBReal;
                                        valuesBReal = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndBReal);
                                }
                                delete[] colIndBReal;
                                colIndBReal = NULL;

                                // insert the coefficients -Im(A) of Im(psi)
                                numEntries = colIndA.size();
                                colIndAImag = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndAImag[k] = 2 * colIndA[k] + 1;
                                if (jc == VALUES) {
                                        valuesAImag = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesAImag[k] = -imag(valuesA[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesAImag, colIndAImag);
                                        delete[] valuesAImag;
                                        valuesAImag = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndAImag);
                                }
                                delete[] colIndAImag;
                                colIndAImag = NULL;

                                // insert the coefficients Im(B) of Im(psi)
                                numEntries = colIndB.size();
                                colIndBImag = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndBImag[k] = 2 * colIndB[k] + 1;
                                if (jc == VALUES) {
                                        valuesBImag = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesBImag[k] = imag(valuesB[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesBImag, colIndBImag);
                                        delete[] valuesBImag;
                                        valuesBImag = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndBImag);
                                }
                                delete[] colIndBImag;
                                colIndBImag = NULL;

                                // right bordering
                                int k = realIndex2complexIndex(Row);
                                int column = 2 * NumComplexUnknowns_;
                                if (jc == VALUES) {
                                        double value = imag(psiView[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, 1, &value,
                                                        &column);
                                } else {
                                        Graph_->InsertGlobalIndices(Row, 1, &column);
                                }

                                // ---------------------------------------------------------
                        } else // Row is odd <=> Imaginary part of the equation
                        {
                                // ---------------------------------------------------------
                                // insert the coefficients Im(A) of Re(psi)
                                numEntries = colIndA.size();
                                colIndAReal = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndAReal[k] = 2 * colIndA[k];
                                if (jc == VALUES) {
                                        valuesAImag = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesAImag[k] = imag(valuesA[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesAImag, colIndAReal);
                                        delete[] valuesAImag;
                                        valuesAImag = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndAReal);
                                }
                                delete[] colIndAReal;
                                colIndAReal = NULL;

                                // insert the coefficients Im(B) of Re(psi)
                                numEntries = colIndB.size();
                                colIndBReal = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndBReal[k] = 2 * colIndB[k];
                                if (jc == VALUES) {
                                        valuesBImag = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesBImag[k] = imag(valuesB[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesBImag, colIndBReal);
                                        delete[] valuesBImag;
                                        valuesBImag = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndBReal);
                                }
                                delete[] colIndBReal;
                                colIndBReal = NULL;

                                // insert the coefficients Re(A) of Im(psi)
                                numEntries = colIndA.size();
                                colIndAImag = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndAImag[k] = 2 * colIndA[k] + 1;
                                if (jc == VALUES) {
                                        valuesAReal = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesAReal[k] = real(valuesA[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesAReal, colIndAImag);
                                        delete[] valuesAReal;
                                        valuesAReal = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndAImag);
                                }
                                delete[] colIndAImag;
                                colIndAImag = NULL;

                                // insert the coefficients -Re(B) of Im(psi)
                                numEntries = colIndB.size();
                                colIndBImag = new int[numEntries];
                                for (k = 0; k < numEntries; k++)
                                        colIndBImag[k] = 2 * colIndB[k] + 1;
                                if (jc == VALUES) {
                                        valuesBReal = new double[numEntries];
                                        for (k = 0; k < numEntries; k++)
                                                valuesBReal[k] = -real(valuesB[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, numEntries,
                                                        valuesBReal, colIndBImag);
                                        delete[] valuesBReal;
                                        valuesBReal = NULL;
                                } else {
                                        Graph_->InsertGlobalIndices(Row, numEntries, colIndBImag);
                                }
                                delete[] colIndBImag;
                                colIndBImag = NULL;

                                // right bordering
                                int column = 2 * NumComplexUnknowns_;
                                if (jc == VALUES) {
                                        int k = realIndex2complexIndex(Row);
                                        double value = -real(psiView[k]);
                                        ierr = jacobian_->SumIntoGlobalValues(Row, 1, &value,
                                                        &column);
                                } else {
                                        Graph_->InsertGlobalIndices(Row, 1, &column);
                                }
                                // ---------------------------------------------------------
                        }
                }
        }

        // ---------------------------------------------------------------------------
        // finish up the graph construction
        try {
                if (jc == VALUES) {
                        jacobian_->FillComplete();
                        jacobian_->OptimizeStorage();
                } else {
                        Graph_->FillComplete();
                }
        } catch (int i) {
            TEST_FOR_EXCEPTION( true,
                                std::logic_error,
                                "FillComplete returned error code " << i );
        }
        // ---------------------------------------------------------------------------

        // Sync up processors for safety's sake
        EComm_->Barrier();

        return true;
}
// =============================================================================
bool GlSystemWithConstraint::computeShiftedMatrix(double alpha, double beta,
                const Epetra_Vector &x, Epetra_Operator &A) {
        // compute the values of the Jacobian
        createJacobian(VALUES, x);

        jacobian_->Scale(alpha);
        //  jacobian_->FillComplete();

        Epetra_Vector newDiag(x);
        Epetra_Vector unitVector(x);
        unitVector.PutScalar(1.0);
        //  newDiag.PutScalar(0.0);
        jacobian_->ExtractDiagonalCopy(newDiag);
        newDiag.Update(beta, unitVector, 1.0);
        jacobian_->ReplaceDiagonalValues(newDiag);

        // Sync up processors to be safe
        EComm_->Barrier();

        return true;
}
// =============================================================================
// function used by LOCA
void
GlSystemWithConstraint::setParameters(const LOCA::ParameterVector &p) {

  TEST_FOR_EXCEPTION( !p.isParameter("H0"),
                      std::logic_error,
                      "Label \"H0\" not valid." );
  double h0 = p.getValue("H0");
  Gl_.setH0(h0);

  TEST_FOR_EXCEPTION( !p.isParameter("scaling"),
                      std::logic_error,
                      "Label \"scaling\" not valid." );
  double scaling = p.getValue("scaling");
  Gl_.setScaling( scaling );

  if (p.isParameter("chi")) {
      double chi = p.getValue("chi");
      Gl_.setChi( chi );
  }
}
// =============================================================================
void
GlSystemWithConstraint::setLocaStepper(const Teuchos::RCP<const LOCA::Stepper> stepper)
{
        stepper_ = stepper;

        // extract the continuation type
        const Teuchos::ParameterList & bifurcationSublist = stepper_->getList()
                                                                    ->sublist("LOCA")
                                                                     .sublist("Bifurcation");

        std::string bifurcationType = bifurcationSublist.get<string>("Type");

        if ( bifurcationType == "None" )
                continuationType_ = ONEPARAMETER;
        else if ( bifurcationType == "Turning Point" )
                continuationType_ = TURNINGPOINT;
        else
            TEST_FOR_EXCEPTION( true,
                                            std::logic_error,
                                            "Unknown continuation type \""
                                            << bifurcationType << "\"" );
}
// =============================================================================
void
GlSystemWithConstraint::releaseLocaStepper()
{
        stepper_ = Teuchos::null;
}
// =============================================================================
// function used by LOCA
void
GlSystemWithConstraint::printSolution( const  Epetra_Vector &x,
                                       double conParam )
{
        Epetra_Vector tmp(*regularRealMap_);
        for (int k=0; k<tmp.MyLength(); k++)
            tmp.ReplaceMyValue( k, 0, x[x.Map().GID(k)] );

        Teuchos::RCP<ComplexVector> psi = glKomplex_->real2complex(tmp);

        // The switch hack is necessary as different continuation algorithms
        // call printSolution() a different number of times per step, e.g.,
        // to store solutions, null vectors, and so forth.
        switch ( continuationType_ ) {
        case ONEPARAMETER:
            printSolutionOneParameterContinuation( psi );
            break;
        case TURNINGPOINT:
            printSolutionTurningPointContinuation( psi );
            break;
        default:
            TEST_FOR_EXCEPTION( true,
                                            std::logic_error,
                                            "Illegal continuation type " << continuationType_ );
        }
}
// =============================================================================
void
GlSystemWithConstraint::printSolutionOneParameterContinuation( const Teuchos::RCP<const ComplexVector> & psi
                                                       ) const
{
        static int conStep = -1;
        conStep++;

        std::string fileName = outputDir_ + "/" + solutionFileNameBase_
                        + EpetraExt::toString(conStep) + ".vtk";

        // actually print the state to fileName
        Gl_.writeSolutionToFile(psi, fileName);

        writeContinuationStats( conStep, psi );
}
// =============================================================================
// In Turning Point continuation, the printSolution method is called exactly
// twice per step:
//
//   1. For printing the solution.
//   2. For printing the right null vector of the Jacobian.
//
// The method gets called subsequently in this order.
void
GlSystemWithConstraint::printSolutionTurningPointContinuation( const Teuchos::RCP<const ComplexVector> & psi
                                                       ) const
{
        static bool printSolution=false;
        static int conStep = -1;

        // alternate between solution and nullvector
        printSolution = !printSolution;

        // increment the step counter only when printing a solution
        if ( printSolution )
                conStep++;

        // determine file name
        std::string fileName;
        if ( printSolution ) {
            fileName = outputDir_ + "/" + solutionFileNameBase_
                         + EpetraExt::toString(conStep) + ".vtk";
            writeContinuationStats( conStep, psi );
        }
        else
            fileName = outputDir_ + "/" + nullvectorFileNameBase_
                         + EpetraExt::toString(conStep) + ".vtk";

        // actually print the state to fileName
        Gl_.writeSolutionToFile(psi, fileName);

}
// =============================================================================
void
GlSystemWithConstraint::writeContinuationStats( const int conStep,
                                  const Teuchos::RCP<const ComplexVector> psi ) const
{
        // fill the continuation parameters file
        std::string contFileName = outputDir_ + "/" + outputDataFileName_;
        std::ofstream contFileStream;

        // Set the output format
        // Think about replacing this with NOX::Utils::Sci.
        contFileStream.setf(std::ios::scientific);
        contFileStream.precision(15);

        if (conStep == 0) {
                contFileStream.open(contFileName.c_str(), ios::trunc);
                contFileStream << "# Step  \t";
                Gl_.appendStats( contFileStream, true );
                contFileStream << "\t#nonlinear steps\n";
        } else {
                // just append to the the contents to the file
                contFileStream.open(contFileName.c_str(), ios::app);
        }

        int nonlinearIterations = stepper_->getSolver()->getNumIterations();

        contFileStream << "  " << conStep << "      \t";
        Gl_.appendStats( contFileStream, false, psi );
        contFileStream << "       \t" << nonlinearIterations << std::endl;

        contFileStream.close();
}
// =============================================================================
// function used by LOCA
void GlSystemWithConstraint::setOutputDir(const string &directory) {
        outputDir_ = directory;
}
// =============================================================================
void
GlSystemWithConstraint::writeSolutionToFile( const Epetra_Vector &x,
                                             const std::string &filePath) const
{
  // TODO: Remove the need for several real2complex calls per step.

  // strip off the phase constraint
  // TODO replace by {im,ex}porter
  Epetra_Vector tmp(*regularRealMap_);
  for (int k=0; k<tmp.MyLength(); k++)
      tmp.ReplaceMyValue( k, 0, x[x.Map().GID(k)] );

  Teuchos::RCP<ComplexVector> psi = glKomplex_->real2complex(tmp);

  Gl_.writeSolutionToFile( psi, filePath );
}
// =============================================================================
void
GlSystemWithConstraint::writeAbstractStateToFile( const Epetra_Vector &x,
                                    const std::string &filePath) const
{
  // strip off the phase constraint
  // TODO replace by {im,ex}porter
  Epetra_Vector tmp(*regularRealMap_);
  for (int k=0; k<tmp.MyLength(); k++)
      tmp.ReplaceMyValue( k, 0, x[x.Map().GID(k)] );

  Teuchos::RCP<ComplexVector> psi = glKomplex_->real2complex(tmp);

  Gl_.writeAbstractStateToFile( psi, filePath );
}
// =============================================================================
//Teuchos::RCP<Epetra_Vector>
//GlSystemWithConstraint::getGlSystemVector( const Teuchos::RCP<const ComplexVector> psi ) const
//{
//        Teuchos::RCP<Epetra_Vector> x = Teuchos::rcp( new Epetra_Vector(*extendedRealMap_) );
//        complex2real( *psi, *x );
//        return x;
//}
// =============================================================================
Teuchos::RCP<const Teuchos::Comm<int> >
GlSystemWithConstraint::create_CommInt( const Teuchos::RCP<const Epetra_Comm> &epetraComm )
{
  using Teuchos::RCP;
  using Teuchos::rcp;
  using Teuchos::rcp_dynamic_cast;
  using Teuchos::set_extra_data;

#ifdef HAVE_MPI
  RCP<const Epetra_MpiComm>
    mpiEpetraComm = rcp_dynamic_cast<const Epetra_MpiComm>(epetraComm);
  if( mpiEpetraComm.get() ) {
    RCP<const Teuchos::OpaqueWrapper<MPI_Comm> >
      rawMpiComm = Teuchos::opaqueWrapper(mpiEpetraComm->Comm());
    set_extra_data( mpiEpetraComm, "mpiEpetraComm", Teuchos::inOutArg(rawMpiComm) );
    RCP<const Teuchos::MpiComm<int> >
      mpiComm = rcp(new Teuchos::MpiComm<int>(rawMpiComm));
    return mpiComm;
  }
#else
  RCP<const Epetra_SerialComm>
    serialEpetraComm = rcp_dynamic_cast<const Epetra_SerialComm>(epetraComm);
  if( serialEpetraComm.get() ) {
    RCP<const Teuchos::SerialComm<int> >
      serialComm = rcp(new Teuchos::SerialComm<int>());
    set_extra_data( serialEpetraComm, "serialEpetraComm", Teuchos::inOutArg(serialComm) );
    return serialComm;
  }
#endif // HAVE_MPI

  // If you get here then the conversion failed!
  return Teuchos::null;
}
// =============================================================================
