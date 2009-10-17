#include "glSystem.h"
#include "ioFactory.h"
#include "glException.h"

#include <iostream>
#include <fstream>

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

// abbreviate the complex type name
typedef std::complex<double> double_complex;

// =============================================================================
// Default constructor
GlSystem::GlSystem ( GinzburgLandau::GinzburgLandau                               &gl,
		     const Teuchos::RCP<Epetra_Comm>                              eComm,
                     const bool                                                   &rv,
                     const Teuchos::RCP<Tpetra::MultiVector<double_complex,int> > psi  ) :
    NumRealUnknowns_ ( 0 ),
    NumMyElements_ ( 0 ),
    NumComplexUnknowns_ ( 0 ),
    Gl_ ( gl ),
    EComm_ ( eComm ),
    TComm_ ( 0 ),
    RealMap_ ( 0 ),
    ComplexMap_ ( 0 ),
    rhs_ ( 0 ),
    Graph_ ( 0 ),
    jacobian_ ( 0 ),
    initialSolution_ ( 0 ),
    reverse_ ( rv ),
    outputDir_ ( "." )
{
  int Nx = Gl_.getStaggeredGrid()->getNx();
  NumComplexUnknowns_ = ( Nx+1 ) * ( Nx+1 );
  NumRealUnknowns_    = 2*NumComplexUnknowns_+1;

  // @TODO
  // There is (until now?) no way to convert a Teuchos::Comm (of psi) 
  // to an Epetra_Comm (of the real valued representation of psi), so the
  // Epetra_Comm has to be generated explicitly, and two communicators are kept
  // side by side all the time. One must make sure that the two are actually
  // equivalent, which can be checked by Thyra's conversion method create_Comm.
  // @TODO
  // Is is actually necessary to have equivalent communicators on the
  // real-valued and the complex-valued side?
  // How to compare two communicators anyway?
  
  // create fitting Tpetra::Comm
  // TODO: move into initializer
  TComm_ = Thyra::create_Comm( EComm_ );
  
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // define maps
  if ( psi.is_null() )   // null pointer
    {
      // define uniform distribution
      ComplexMap_ = Teuchos::rcp(new Tpetra::Map<int>( NumComplexUnknowns_, 0, TComm_ ) );
    }
  else
    {
      // psi->getMap() returns a CONST map
      ComplexMap_ = Teuchos::RCP<const Tpetra::Map<int> >( psi->getMap() );
    }

  // get the map for the real values
  makeRealMap( ComplexMap_ );
  
  // set the number of local elements
  NumMyElements_ = RealMap_->NumMyElements();
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Create a map for the real-valued vector to be spread all over all
  // processors.
  // @TODO Remove (the need for) this.
  // define the map where each processor has a full solution vector
  EverywhereMap_ = Teuchos::rcp( new Epetra_Map ( NumRealUnknowns_,
                                                  NumRealUnknowns_,
                                                  0,
                                                  *EComm_ ) );
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // initialize solution
  initialSolution_ = Teuchos::rcp ( new Epetra_Vector ( *RealMap_ ) );
  if ( psi.is_null() )   // null pointer
    {
      // define map for psi
      initialSolution_->PutScalar ( 0.5 ); // Default initialization
    }
  else
    {
      if ( psi->getGlobalLength() != (unsigned int)NumComplexUnknowns_ )
        {
          std::string message = "Size of the initial guess vector ("
                                + EpetraExt::toString ( int ( psi->getGlobalLength() ) )
                                + ") does not coincide with the number of unknowns ("
                                + EpetraExt::toString ( NumComplexUnknowns_ ) + ").";
          throw glException ( "GlSystem::GlSystem",
                              message );
        }
      psi2real ( *psi, *initialSolution_ );
    }
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  


  // create the sparsity structure (graph) of the Jacobian
  // use x as DUMMY argument
  Epetra_Vector dummy ( *RealMap_ );
  createJacobian ( ONLY_GRAPH, dummy );

  // Allocate the sparsity pattern of the Jacobian matrix
  jacobian_ = Teuchos::rcp ( new Epetra_CrsMatrix ( Copy, *Graph_ ) );

  // Clean-up
  jacobian_->FillComplete();
}
// =============================================================================
// Destructor
GlSystem::~GlSystem()
{
}
// =============================================================================
int GlSystem::realIndex2complexIndex ( const int realIndex ) const
{
  if ( ! ( realIndex%2 ) ) // realIndex even
    return realIndex/2;
  else
    return ( realIndex-1 ) /2;
}
// =============================================================================
void GlSystem::real2complex ( const Epetra_Vector    &realvec,
                              vector<double_complex> &psi ) const
{
  for ( int k=0; k<NumComplexUnknowns_; k++ )
    psi[k] = double_complex ( realvec[2*k], realvec[2*k+1] );
}
// =============================================================================
void GlSystem::complex2real ( const vector<double_complex> &psi,
                              Teuchos::RCP<Epetra_Vector>  realvec ) const
{
  for ( int k=0; k<NumComplexUnknowns_; k++ )
    {
      ( *realvec ) [2*k]   = real ( psi[k] );
      ( *realvec ) [2*k+1] = imag ( psi[k] );
    }
}
// =============================================================================
// converts a real-valued vector to a complex-valued psi vector
void GlSystem::real2psi ( const Epetra_Vector                     &realvec,
                          Tpetra::MultiVector<double_complex,int> &psi     ) const
{
  for ( unsigned int k=0; k<psi.getGlobalLength(); k++ ) {
    double_complex z = double_complex ( realvec[2*k], realvec[2*k+1] );
    psi.replaceGlobalValue( k, 0, z );
  }
}
// =============================================================================
// converts a real-valued vector to a complex-valued psi vector
void GlSystem::psi2real ( const Tpetra::MultiVector<double_complex,int> &psi,
                          Epetra_Vector                                 &x   ) const
{
  Teuchos::ArrayRCP<const double_complex> psiView = psi.getVector(0)->get1dView();
  for ( int k=0; k<NumComplexUnknowns_; k++ )
    {
      x[2*k]   = real ( psiView[k] );
      x[2*k+1] = imag ( psiView[k] );
    }
}
// =============================================================================
void GlSystem::makeRealMap( const Teuchos::RCP<const Tpetra::Map<int> >  complexMap )
{ 
  int numRealGlobalElements = 2*complexMap->getNodeNumElements();
  
  int myPid = TComm_->getRank();

  // treat the phase condition on the first node
  if ( myPid==0 )
    numRealGlobalElements++;
  
  Epetra_IntSerialDenseVector   realMapGIDs( numRealGlobalElements );
  Teuchos::ArrayView<const int> myGlobalElements = complexMap->getNodeElementList();
  // Construct the map in such a way that all complex entries on processor K
  // are split up into real and imaginary part, which will both reside on
  // processor K again.
  for (unsigned int i=0; i<complexMap->getNodeNumElements(); i++)
    {
      realMapGIDs[2*i]   = 2*myGlobalElements[i];
      realMapGIDs[2*i+1] = 2*myGlobalElements[i] + 1;
    }
    
  // set the phase condition
  if ( myPid==0 )
    realMapGIDs[numRealGlobalElements-1] = NumRealUnknowns_-1;

  RealMap_ = Teuchos::rcp(new Epetra_Map( numRealGlobalElements,
                                          realMapGIDs.Length(),
                                          realMapGIDs.Values(),
                                          complexMap->getIndexBase(),
                                          *EComm_                            ) );

  return;
}
// =============================================================================
bool
GlSystem::computeF ( const Epetra_Vector &x,
                     Epetra_Vector       &FVec,
                     const NOX::Epetra::Interface::Required::FillType fillFlag
                   )
{
//   vector<double_complex> psi ( NumComplexUnknowns_ );
//   double_complex         val;
  
//   // define the Tpetra platform
// #ifdef TPETRA_MPI
//         Tpetra::MpiPlatform<int, double_complex> platformV(MPI_COMM_WORLD);
//         Tpetra::MpiPlatform<int, int>            platformE(MPI_COMM_WORLD);
// #else
//         Tpetra::SerialPlatform<int, double_complex> platformV;
//         Tpetra::SerialPlatform<int, int>            platformE;
// #endif
  
  // ***************************************************************************
   
  // make sure that the input and output vectors are correctly mapped
  if ( !x.Map().SameAs( *RealMap_ ) ) {
      throw glException ( "GlSystem::computeF",
                          "Maps of x and the computed real-valued map do not coincide." );    
  }
  
  if ( !FVec.Map().SameAs( *RealMap_ ) ) {
      throw glException ( "GlSystem::computeF",
                          "Maps of FVec and the computed real-valued map do not coincide." );    
  }
   
  // define vector
  // @TODO: Replace this by Tpetra::Vector as soon as upstream is ready.
  Tpetra::MultiVector<double_complex,int> psi( ComplexMap_, 1, true );

  // convert from x to psi2
  real2psi ( x, psi );

  // define output vector
  Tpetra::MultiVector<double_complex,int> res( ComplexMap_, 1, true );
  
  // compute the GL residual
  res = Gl_.computeGlVector ( psi );
  
  // transform back to fully real equation
  psi2real( res, FVec );
  
  // add phase condition
  FVec[2*NumComplexUnknowns_] = 0.0;
  
//   // ***************************************************************************  
// 
//   // scatter x over all processors
//   Epetra_Export Exporter ( *StandardMap, *EverywhereMap_ );
//   xEverywhere.Export ( x, Exporter, Insert );
//   ( void ) real2complex ( xEverywhere, psi );
// 
//   // loop over the system rows
//   double passVal;
//   for ( int i=0; i<NumMyElements_; i++ )
//     {
//       int myGlobalIndex = StandardMap->GID ( i );
//       if ( myGlobalIndex==2*NumComplexUnknowns_ )   // phase condition
//         {
//           passVal = 0.0;
//         }
//       else   // GL equations
//         {
//           // get the index of the complex valued equation
//           int psiIndex = realIndex2complexIndex ( myGlobalIndex );
//           // get the complex value
//           // TODO: The same value is actually fetched twice in this loop
//           //       possibly consecutively: Once for the real, once for
//           //       the imaginary part of it.
//           val = Gl_.computeGl ( psiIndex, psi );
// 
// 
//           if ( ! ( myGlobalIndex%2 ) ) // myGlobalIndex is even
//             passVal = real ( val );
//           else // myGlobalIndex is odd
//             passVal = imag ( val );
//         }
//       FVec.ReplaceGlobalValues ( 1, &passVal, &myGlobalIndex );
//     }

  return true;
}
// =============================================================================
bool GlSystem::computeJacobian ( const Epetra_Vector &x,
                                 Epetra_Operator     &Jac )
{
  // compute the values of the Jacobian
  createJacobian ( VALUES, x );

  // optimize storage
  jacobian_->FillComplete();

  // Sync up processors to be safe
  EComm_->Barrier();

  return true;
}
// =============================================================================
bool GlSystem::computePreconditioner ( const Epetra_Vector    &x,
                                       Epetra_Operator        &Prec,
                                       Teuchos::ParameterList *precParams ) const
{
  throw glException ( "GlSystem::preconditionVector",
                      "Use explicit Jacobian only for this test problem!" );
}
// =============================================================================
Teuchos::RCP<Epetra_Vector> GlSystem::getSolution() const
{
  return initialSolution_;
}
// =============================================================================
Teuchos::RCP<Epetra_CrsMatrix> GlSystem::getJacobian() const
{
  return jacobian_;
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
bool
GlSystem::createJacobian ( const jacCreator    jc,
                           const Epetra_Vector &x )
{
//   std::vector<double_complex> psi ( NumComplexUnknowns_ );
  Teuchos::RCP<Tpetra::MultiVector<double_complex,int> > psi =
     Teuchos::rcp( new Tpetra::MultiVector<double_complex,int>(ComplexMap_,1) );

  vector<int>            colIndA, colIndB;
  vector<double_complex> valuesA, valuesB;

  int    *colInd      = NULL,
         *colIndAReal = NULL, *colIndAImag = NULL,
         *colIndBReal = NULL, *colIndBImag = NULL;
  double *values      = NULL,
         *valuesAReal = NULL, *valuesAImag = NULL,
         *valuesBReal = NULL, *valuesBImag = NULL;

  int ierr, k, complexRow, numEntries;
  
  Teuchos::ArrayRCP<const double_complex> psiView;
  
  if ( jc==VALUES )
    {
//       Epetra_Vector xEverywhere ( *EverywhereMap_ );
      // scatter x over all processors
//       Epetra_Export Exporter ( *RealMap_, *EverywhereMap_ );

//       xEverywhere.Export ( x, Exporter, Insert );
//       real2complex ( xEverywhere, psi );

      real2psi( x, *psi );
      
      psiView = psi->getVector(0)->get1dView();
      
      // set the matrix to 0
      jacobian_->PutScalar ( 0.0 );
    }
  else
    {
      if ( Graph_.is_valid_ptr() )
        {
	  // Nullify Graph_ pointer.
          Graph_ = Teuchos::ENull();
        }
      // allocate the graph
      int approxNumEntriesPerRow = 1;
      Graph_ = Teuchos::rcp<Epetra_CrsGraph>( new Epetra_CrsGraph ( Copy, *RealMap_, approxNumEntriesPerRow, false ) );
    }

  // Construct the Epetra Matrix
  for ( int i=0 ; i<NumMyElements_ ; i++ )
    {
      int Row = RealMap_->GID ( i );

      if ( Row==2*NumComplexUnknowns_ )   // phase condition
        {
          // fill in phase condition stuff
          numEntries = 2*NumComplexUnknowns_;
          colInd = new int[numEntries];
          for ( int k=0; k<numEntries; k++ )
            colInd[k] = k;
          if ( jc==VALUES )   // fill on columns and values
            {
              values = new double[numEntries];
              for ( int k=0; k<NumComplexUnknowns_; k++ )
                {
                  values[2*k]   = -imag ( psiView[k] );
                  values[2*k+1] =  real ( psiView[k] );
                }
              // fill it in!
              ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                     numEntries,
                                                     values,
                                                     colInd );
              delete [] values;
              values = NULL;
            }
          else   // only fill the sparsity graph
            {
              Graph_->InsertGlobalIndices ( Row,
                                           numEntries,
                                           colInd );
            }
          delete [] colInd;
          colInd = NULL;


        }
      else   // GL equations
        {
          // get the values and column indices
          // TODO: The same value is actually fetched twice in this loop
          //       possibly consecutively: Once for the real, once for
          //       the imaginary part of it.
          if ( ! ( Row%2 ) ) // Row even
            complexRow = Row/2;
          else
            complexRow = ( Row-1 ) /2;

          if ( jc==VALUES )
            {
              // fill on columns and values
              Gl_.getJacobianRow ( complexRow,
                                   psi,
                                   colIndA, valuesA,
                                   colIndB, valuesB );
            }
          else
            {
              // only fill the sparsity graph
              Gl_.getJacobianRowSparsity ( complexRow,
                                          colIndA,
                                          colIndB );
            }

          if ( ! ( Row%2 ) )   // myGlobalIndex is even <=> Real part of the equation system
            {
              // ---------------------------------------------------------------
              // insert the coefficients Re(A) of Re(psi)
              numEntries = colIndA.size();
              colIndAReal = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndAReal[k] = 2*colIndA[k];

              if ( jc==VALUES )
                {
                  valuesAReal = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesAReal[k] = real ( valuesA[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesAReal,
                                                         colIndAReal );
                  delete [] valuesAReal;
                  valuesAReal = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndAReal );
                }
              delete [] colIndAReal;
              colIndAReal = NULL;

              // insert the coefficients Re(B) of Re(psi)
              numEntries = colIndB.size();
              colIndBReal = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndBReal[k] = 2*colIndB[k];
              if ( jc==VALUES )
                {
                  valuesBReal = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesBReal[k] = real ( valuesB[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesBReal,
                                                         colIndBReal );
                  delete [] valuesBReal;
                  valuesBReal = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndBReal );
                }
              delete [] colIndBReal;
              colIndBReal = NULL;

              // insert the coefficients -Im(A) of Im(psi)
              numEntries = colIndA.size();
              colIndAImag = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndAImag[k] = 2*colIndA[k]+1;
              if ( jc==VALUES )
                {
                  valuesAImag = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesAImag[k] = -imag ( valuesA[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesAImag,
                                                         colIndAImag );
                  delete [] valuesAImag;
                  valuesAImag = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndAImag );
                }
              delete [] colIndAImag;
              colIndAImag = NULL;


              // insert the coefficients Im(B) of Im(psi)
              numEntries = colIndB.size();
              colIndBImag = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndBImag[k] = 2*colIndB[k]+1;
              if ( jc==VALUES )
                {
                  valuesBImag = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesBImag[k] = imag ( valuesB[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesBImag,
                                                         colIndBImag );
                  delete [] valuesBImag;
                  valuesBImag = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndBImag );
                }
              delete [] colIndBImag;
              colIndBImag = NULL;


              // right bordering
              int k = realIndex2complexIndex ( Row );
              int column = 2*NumComplexUnknowns_;
              if ( jc==VALUES )
                {
                  double value  = imag ( psiView[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         1,
                                                         &value,
                                                         &column );
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               1,
                                               &column );
                }

              // ---------------------------------------------------------
            }
          else   // Row is odd <=> Imaginary part of the equation
            {
              // ---------------------------------------------------------
              // insert the coefficients Im(A) of Re(psi)
              numEntries = colIndA.size();
              colIndAReal = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndAReal[k] = 2*colIndA[k];
              if ( jc==VALUES )
                {
                  valuesAImag = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesAImag[k] = imag ( valuesA[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesAImag,
                                                         colIndAReal );
                  delete [] valuesAImag;
                  valuesAImag = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndAReal );
                }
              delete [] colIndAReal;
              colIndAReal = NULL;

              // insert the coefficients Im(B) of Re(psi)
              numEntries = colIndB.size();
              colIndBReal = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndBReal[k] = 2*colIndB[k];
              if ( jc==VALUES )
                {
                  valuesBImag = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesBImag[k] = imag ( valuesB[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesBImag,
                                                         colIndBReal );
                  delete [] valuesBImag;
                  valuesBImag = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndBReal );
                }
              delete [] colIndBReal;
              colIndBReal = NULL;


              // insert the coefficients Re(A) of Im(psi)
              numEntries = colIndA.size();
              colIndAImag = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndAImag[k] = 2*colIndA[k]+1;
              if ( jc==VALUES )
                {
                  valuesAReal = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesAReal[k] = real ( valuesA[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesAReal,
                                                         colIndAImag );
                  delete [] valuesAReal;
                  valuesAReal = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndAImag );
                }
              delete [] colIndAImag;
              colIndAImag = NULL;


              // insert the coefficients -Re(B) of Im(psi)
              numEntries = colIndB.size();
              colIndBImag = new int[numEntries];
              for ( k=0; k<numEntries; k++ )
                colIndBImag[k] = 2*colIndB[k]+1;
              if ( jc==VALUES )
                {
                  valuesBReal = new double[numEntries];
                  for ( k=0; k<numEntries; k++ )
                    valuesBReal[k] = -real ( valuesB[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         numEntries,
                                                         valuesBReal,
                                                         colIndBImag );
                  delete [] valuesBReal;
                  valuesBReal = NULL;
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               numEntries,
                                               colIndBImag );
                }
              delete [] colIndBImag;
              colIndBImag = NULL;

              // right bordering
              int column = 2*NumComplexUnknowns_;
              if ( jc==VALUES )
                {
                  int k = realIndex2complexIndex ( Row );
                  double value  = -real ( psiView[k] );
                  ierr = jacobian_->SumIntoGlobalValues ( Row,
                                                         1,
                                                         &value,
                                                         &column );
                }
              else
                {
                  Graph_->InsertGlobalIndices ( Row,
                                               1,
                                               &column );
                }
              // ---------------------------------------------------------
            }
        }
    }

  // ---------------------------------------------------------------------------
  // finish up the graph construction
  try
    {
      if ( jc==VALUES )
        {
          jacobian_->FillComplete();
          jacobian_->OptimizeStorage();
        }
      else
        {
          Graph_->FillComplete();
        }
    }
  catch ( int i )
    {
      std::string message = "FillComplete returned error code "
                            + EpetraExt::toString ( i ) + ".";
      throw glException ( "GlSystem::createJacobian",
                          message );
    }
  // ---------------------------------------------------------------------------

  // Sync up processors for safety's sake
  EComm_->Barrier();
  
  return true;
}
// =============================================================================
// function used by LOCA
void GlSystem::setParameters ( const LOCA::ParameterVector &p )
{
  double h0 = p.getValue ( "H0" );

  // set H0 in the underlying problem class
  Gl_.getStaggeredGrid()->setH0 ( h0 );
}
// =============================================================================
// function used by LOCA
void GlSystem::printSolution ( const Epetra_Vector &x,
                               double              conParam )
{
  static int conStep = 0;

  if (reverse_)
      conStep--;
  else
      conStep++;

  // define vector
  // @TODO: Replace this by Tpetra::Vector as soon as upstream is ready.
  Tpetra::MultiVector<double_complex,int>  psi(ComplexMap_,1,true);
  // convert from x to psi
  real2psi ( x, psi );

  double energy    = Gl_.freeEnergy  ( psi );
  int    vorticity = Gl_.getVorticity  ( psi );
  
  // create temporary parameter list
  // TODO: get rid of this
  // -- An ugly thing here is that we have to explicitly mention the parameter
  // names. A solution could possibly be to include the parameter list in the
  // constructor of glSystem.
  Teuchos::ParameterList tmpList;
  tmpList.get ( "H0",         conParam );
  tmpList.get ( "edgelength", Gl_.getStaggeredGrid()->getEdgeLength() );
  tmpList.get ( "Nx",         Gl_.getStaggeredGrid()->getNx() );
  tmpList.get ( "freeEnergy", energy);
  tmpList.get ( "vorticity",  vorticity);

  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // TODO:
  // see if the following can be replaced by some FORMAT construction
  std::string stepString;
  if (conStep>0)
      stepString = "+" + EpetraExt::toString ( conStep );
  else
      stepString = EpetraExt::toString ( conStep );
  std::string fileName = "continuationStep"
                         + stepString
                         + ".vtk";

  IoVirtual* fileIo = IoFactory::createFileIo ( outputDir_+"/"+fileName );
  fileIo->write ( psi,
                  tmpList,
                  * ( Gl_.getStaggeredGrid() ) );
  delete fileIo;
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // fill the continuation parameters file
  std::string contFileBaseName;
  if (conStep>0)
       contFileBaseName = "continuation+.dat";
  else
       contFileBaseName = "continuation-.dat";
  std::string contFileName     = outputDir_+"/"+contFileBaseName;

  std::ofstream contFileStream;

  // Set the output format
  // Think about replacing this with NOX::Utils::Sci.
  contFileStream.setf( std::ios::scientific );
  contFileStream.precision(15);

  if ( abs(conStep)==1 ) {
      contFileStream.open (contFileName.c_str(),ios::trunc);
      contFileStream << "# Step  "
                     << "\tH0              "
                     << "\tfree energy         "
                     << "\tvorticity\n";
  } else {
      // just append to the the contents to the file
      contFileStream.open (contFileName.c_str(),ios::app);
  }

  contFileStream << "  " << conStep << "     "
                 << "\t" << conParam
                 << "\t" << energy
                 << "\t" << vorticity << std::endl;

  contFileStream.close();
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

}
// =============================================================================
// function used by LOCA
void GlSystem::setOutputDir ( const string &directory )
{
  outputDir_ = directory;
}
// =============================================================================
void GlSystem::solutionToFile ( const Epetra_Vector    &x,
                                Teuchos::ParameterList &problemParams,
                                const std::string      &fileName )
{
  
  // define vector
  // @TODO: Replace this by Tpetra::Vector as soon as upstream is ready.
  Tpetra::MultiVector<double_complex,int> psi(ComplexMap_,1,true);

  // convert from x to psi2
  real2psi ( x, psi );

  // set extra parameters
  problemParams.set( "freeEnergy", Gl_.freeEnergy( psi ) );
  problemParams.set( "vorticity" , Gl_.getVorticity( psi ) );

  IoVirtual* fileIo = IoFactory::createFileIo ( fileName );
  fileIo->write ( psi,
                  problemParams,
                  * ( Gl_.getStaggeredGrid() ) );

}
// =============================================================================