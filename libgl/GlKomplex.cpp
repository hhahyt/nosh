/*
 * GlKomplex.cpp
 *
 *  Created on: Dec 16, 2009
 *      Author: Nico Schl�mer
 */

#include "GlKomplex.h"

#include <Epetra_IntSerialDenseVector.h>
#include <Epetra_Map.h>

#include <Teuchos_DefaultComm.hpp> // for Teuchos::SerialComm

#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif // HAVE_MPI

// abbreviate the complex type name
typedef std::complex<double> double_complex;

// =============================================================================
// Default constructor
GlKomplex::GlKomplex( const Teuchos::RCP<const Epetra_Comm>                  eComm,
		              const Teuchos::RCP<const Tpetra::Map<Thyra::Ordinal> > ComplexMap ) :
  EComm_( eComm ),
  TComm_( create_CommInt(eComm) ),
  RealMap_( createRealMap(ComplexMap) ),
  ComplexMap_( ComplexMap ),
  realMatrix_( Teuchos::null )
{
}
// =============================================================================
GlKomplex::~GlKomplex()
{
}
// =============================================================================
Teuchos::RCP<Epetra_Map>
GlKomplex::getRealMap() const
{
	return RealMap_;
}
// =============================================================================
Teuchos::RCP<const Tpetra::Map<Thyra::Ordinal> >
GlKomplex::getComplexMap() const
{
	return ComplexMap_;
}
// =============================================================================
// converts a real-valued vector to a complex-valued psi vector
Teuchos::RCP<ComplexVector>
GlKomplex::real2complex(const Epetra_Vector & x ) const
{
  TEST_FOR_EXCEPTION( !RealMap_.is_valid_ptr() || RealMap_.is_null(),
                      std::logic_error,
                      "RealMap_ not properly initialized." );

  TEST_FOR_EXCEPTION( !x.Map().SameAs(*RealMap_),
                      std::logic_error,
                      "Maps for real-valued vectors do not coincide. "
                      << "Check, for example, the number of elements "
                      << "(" << x.Map().NumGlobalElements() << " for x vs. "
                      << RealMap_->NumGlobalElements() << " for RealMap_).");

  TEST_FOR_EXCEPTION( !ComplexMap_.is_valid_ptr() || ComplexMap_.is_null(),
                      std::logic_error,
                      "ComplexMap_ has not been properly initialized." );

  Teuchos::RCP<ComplexVector> z = Teuchos::rcp( new ComplexVector(ComplexMap_) );

  // TODO: parallelize
  for (unsigned int k=0; k < z->getGlobalLength(); k++) {
      double_complex c = double_complex(x[2 * k], x[2 * k + 1]);
      z->replaceGlobalValue(k, c);
  }
  return z;
}
// =============================================================================
// converts a real-valued vector to a complex-valued psi vector
Teuchos::RCP<Epetra_Vector>
GlKomplex::complex2real( const ComplexVector &complexVec ) const
{
  TEUCHOS_ASSERT(    ComplexMap_.is_valid_ptr()
		          && !ComplexMap_.is_null()
		          && complexVec.getMap()->isSameAs(*ComplexMap_) );

  Teuchos::RCP<Epetra_Vector> x = Teuchos::rcp( new Epetra_Vector(*RealMap_) );
  // TODO: parallelize
  Teuchos::ArrayRCP<const double_complex> complexVecView = complexVec.get1dView();
  int numComplexUnknowns = ComplexMap_->getGlobalNumElements();

  for (int k = 0; k < numComplexUnknowns; k++) {
      x->ReplaceGlobalValue( 2*k  , 0, real(complexVecView[k]) );
      x->ReplaceGlobalValue( 2*k+1, 0, imag(complexVecView[k]) );
  }
  return x;
}
// =============================================================================
Teuchos::RCP<Epetra_Map>
GlKomplex::createRealMap( const Teuchos::RCP<const Tpetra::Map<Thyra::Ordinal> > & ComplexMap ) const
{
  TEST_FOR_EXCEPTION( !ComplexMap.is_valid_ptr() || ComplexMap.is_null(),
                      std::logic_error,
                      "ComplexMap has not been properly initialized." );

  // get view for the global indices of the global elements
  Teuchos::ArrayView<const Thyra::Ordinal> myComplexGIDs =
                                             ComplexMap->getNodeElementList();

  // Construct the map in such a way that all complex entries on processor K
  // are split up into real and imaginary part, which will both reside on
  // processor K again.
  unsigned int numMyComplexElements = myComplexGIDs.size();
  unsigned int numMyRealElements    = 2*numMyComplexElements;
  Epetra_IntSerialDenseVector myRealGIDs( numMyRealElements );
  for (unsigned int i = 0; i < numMyComplexElements; i++) {
	  myRealGIDs[2*i  ] = 2 * myComplexGIDs[i];
	  myRealGIDs[2*i+1] = 2 * myComplexGIDs[i] + 1;
  }

  return Teuchos::rcp( new Epetra_Map(numMyRealElements,
		                              myRealGIDs.Length(),
		                              myRealGIDs.Values(),
                                      ComplexMap->getIndexBase(),
                                      *EComm_)
                     );
}
// =============================================================================
Teuchos::RCP<const Teuchos::Comm<int> >
GlKomplex::create_CommInt( const Teuchos::RCP<const Epetra_Comm> &epetraComm )
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
  Teuchos::RCP<const Epetra_SerialComm>
    serialEpetraComm = rcp_dynamic_cast<const Epetra_SerialComm>(epetraComm);
  if( serialEpetraComm.get() ) {
    Teuchos::RCP<const Teuchos::SerialComm<int> >
      serialComm = rcp(new Teuchos::SerialComm<int>());
    set_extra_data( serialEpetraComm, "serialEpetraComm", Teuchos::inOutArg(serialComm) );
    return serialComm;
  }
#endif // HAVE_MPI

  // If you get here then the conversion failed!
  return Teuchos::null;
}
// =============================================================================
void
GlKomplex::initializeMatrix()
{
	int numEntriesPerRow = 0; // Fill during insertion phase.
	realMatrix_ = Teuchos::rcp( new Epetra_CrsMatrix(Copy,*RealMap_,0) );
}
// =============================================================================
void
GlKomplex::finalizeMatrix()
{
	TEST_FOR_EXCEPT( 0 != realMatrix_->FillComplete() );
	TEST_FOR_EXCEPT( 0 != realMatrix_->OptimizeStorage() );
}
// =============================================================================
void
GlKomplex::zeroOutMatrix()
{
	TEST_FOR_EXCEPT( 0 != realMatrix_->PutScalar(0.0) );
}
// =============================================================================
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
void
GlKomplex::updateRow( const int                            row,
                      const std::vector<int>             & indicesA,
                      const std::vector<double_complex>  & valuesA,
                      const std::vector<int>             & indicesB,
                      const std::vector<double_complex>  & valuesB,
                      const bool                           firstTime
                    )
{
	TEUCHOS_ASSERT( realMatrix_.is_valid_ptr() && !realMatrix_.is_null() );
	TEUCHOS_ASSERT_IN_RANGE_UPPER_EXCLUSIVE(row, 0, ComplexMap_->getMaxGlobalIndex());

	int numEntries;
	int * indicesAReal;  double * valuesAReal;
	int * indicesAImag;  double * valuesAImag;
	int * indicesBReal;  double * valuesBReal;
	int * indicesBImag;  double * valuesBImag;

	int realRow = 2*row;
	// -------------------------------------------------------------------
	// insert the coefficients Re(A) of Re(z)
	numEntries = indicesA.size();
	indicesAReal = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesAReal[k] = 2 * indicesA[k];

    valuesAReal = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesAReal[k] = std::real(valuesA[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(realRow, numEntries, valuesAReal, indicesAReal, firstTime) );
	delete[] valuesAReal;
	valuesAReal = NULL;
	delete[] indicesAReal;
	indicesAReal = NULL;
	// -------------------------------------------------------------------
	// insert the coefficients Re(B) of Re(z)
	numEntries = indicesB.size();
	indicesBReal = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesBReal[k] = 2 * indicesB[k];
	valuesBReal = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesBReal[k] = std::real(valuesB[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(realRow, numEntries, valuesBReal, indicesBReal, firstTime) );

	delete[] valuesBReal;
	valuesBReal = NULL;
	delete[] indicesBReal;
	indicesBReal = NULL;
	// -------------------------------------------------------------------
	// insert the coefficients -Im(A) of Im(z)
	numEntries = indicesA.size();
	indicesAImag = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesAImag[k] = 2 * indicesA[k] + 1;
	valuesAImag = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesAImag[k] = -std::imag(valuesA[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(realRow, numEntries, valuesAImag, indicesAImag, firstTime) );

	delete[] valuesAImag;
	valuesAImag = NULL;
	delete[] indicesAImag;
	indicesAImag = NULL;
	// -------------------------------------------------------------------
	// insert the coefficients Im(B) of Im(z)
	numEntries = indicesB.size();
	indicesBImag = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesBImag[k] = 2 * indicesB[k] + 1;
	valuesBImag = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesBImag[k] = std::imag(valuesB[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(realRow, numEntries, valuesBImag, indicesBImag, firstTime) );

	delete[] valuesBImag;
	valuesBImag = NULL;
	delete[] indicesBImag;
	indicesBImag = NULL;
	// -------------------------------------------------------------------


	int imagRow = 2*row+1;
	// -------------------------------------------------------------------
	// insert the coefficients Im(A) of Re(z)
	numEntries = indicesA.size();
	indicesAReal = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesAReal[k] = 2 * indicesA[k];
	valuesAImag = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesAImag[k] = std::imag(valuesA[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(imagRow, numEntries, valuesAImag, indicesAReal, firstTime) );

	delete[] valuesAImag;
	valuesAImag = NULL;
	delete[] indicesAReal;
	indicesAReal = NULL;
	// -------------------------------------------------------------------
	// insert the coefficients Im(B) of Re(z)
	numEntries = indicesB.size();
	indicesBReal = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesBReal[k] = 2 * indicesB[k];
	valuesBImag = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesBImag[k] = std::imag(valuesB[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(imagRow, numEntries, valuesBImag, indicesBReal, firstTime) );

	delete[] valuesBImag;
	valuesBImag = NULL;
	delete[] indicesBReal;
	indicesBReal = NULL;
	// -------------------------------------------------------------------
	// insert the coefficients Re(A) of Im(z)
	numEntries = indicesA.size();
	indicesAImag = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesAImag[k] = 2 * indicesA[k] + 1;
	valuesAReal = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesAReal[k] = std::real(valuesA[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(imagRow, numEntries, valuesAReal, indicesAImag, firstTime) );

	delete[] valuesAReal;
	valuesAReal = NULL;
	delete[] indicesAImag;
	indicesAImag = NULL;
	// -------------------------------------------------------------------
	// insert the coefficients -Re(B) of Im(z)
	numEntries = indicesB.size();
	indicesBImag = new int[numEntries];
	for (int k = 0; k < numEntries; k++)
		indicesBImag[k] = 2 * indicesB[k] + 1;
	valuesBReal = new double[numEntries];
	for (int k = 0; k < numEntries; k++)
		valuesBReal[k] = -std::real(valuesB[k]);

	TEST_FOR_EXCEPT( 0 != PutRow(imagRow, numEntries, valuesBReal, indicesBImag, firstTime) );

	delete[] valuesBReal;
	valuesBReal = NULL;
	delete[] indicesBImag;
	indicesBImag = NULL;
	// -------------------------------------------------------------------
}
// =============================================================================
int
GlKomplex::PutRow( int Row, int & numIndices, double * values, int * indices, bool firstTime )
{
	if (firstTime) {
		return realMatrix_->InsertGlobalValues( Row, numIndices, values, indices );
	} else {
		return realMatrix_->SumIntoGlobalValues( Row, numIndices, values, indices );
	}
}
// =============================================================================
Teuchos::RCP<const Epetra_CrsMatrix>
GlKomplex::getMatrix() const
{
	return realMatrix_;
}
// =============================================================================
