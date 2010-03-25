/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2010 Nico Sch\"omer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "Ginla_ModelEvaluator_Default.h"

#include <Epetra_LocalMap.h>
#include <Epetra_CrsMatrix.h>

#include "Ginla_Operator_Virtual.h"

// ============================================================================
Ginla::ModelEvaluator::Default::
Default ( const Teuchos::RCP<Ginla::Operator::Virtual> & glOperator,
          const Teuchos::RCP<Ginla::Komplex>           & komplex
        ) :
        glOperator_ ( glOperator ),
        komplex_ ( komplex ),
        x_(  Teuchos::rcp( new Epetra_Vector( *komplex_->getRealMap(), true ) ) ),
        firstTime_ ( true ),
        numParameters_( 1 )
{
//   x_->Random();

  Teuchos::RCP<ComplexVector> psi =
      Teuchos::rcp( new ComplexVector( komplex_->getComplexMap() ) );
  psi->putScalar( double_complex(1.0,0.0) );
  *x_ = *(komplex_->complex2real( psi ));
  
  // set up parameters
  p_map_ = Teuchos::rcp(new Epetra_LocalMap( numParameters_,
                                             0,
                                             komplex_->getRealMap()->Comm() ) );

  p_init_ = Teuchos::rcp(new Epetra_Vector(*p_map_));
  for (int i=0; i<numParameters_; i++)
      (*p_init_)[i]= 0.0;
  
  Teuchos::Tuple<std::string,1> t = Teuchos::tuple<std::string>( "Parameter 0" );
  p_names_ = Teuchos::rcp( new Teuchos::Array<std::string>( t ) );

  return;
}
// ============================================================================
Ginla::ModelEvaluator::Default::
Default ( const Teuchos::RCP<Ginla::Operator::Virtual> & glOperator,
          const Teuchos::RCP<Ginla::Komplex>           & komplex,
          const Teuchos::RCP<const ComplexVector>      & psi
        ) :
        glOperator_ ( glOperator ),
        komplex_ ( komplex ),
        x_( komplex_->complex2real( psi ) ),
        firstTime_ ( true ),
        numParameters_( 1 )
{
  // make sure the maps are compatible
  TEUCHOS_ASSERT( psi->getMap()->isSameAs( *komplex->getComplexMap()) );
}
// ============================================================================
Ginla::ModelEvaluator::Default::
~Default()
{
}
// ============================================================================
Teuchos::RCP<const Epetra_Map>
Ginla::ModelEvaluator::Default::
get_x_map() const
{
  return komplex_->getRealMap();
}
// ============================================================================
Teuchos::RCP<const Epetra_Map>
Ginla::ModelEvaluator::Default::
get_f_map() const
{
  return komplex_->getRealMap();
}
// ============================================================================
Teuchos::RCP<const Epetra_Vector>
Ginla::ModelEvaluator::Default::
get_x_init () const
{
  return x_;
}
// ============================================================================
Teuchos::RCP<const Epetra_Vector>
Ginla::ModelEvaluator::Default::
get_p_init ( int l ) const
{
  TEUCHOS_ASSERT_EQUALITY( 0, l );

  std::cout << "Ginla::ModelEvaluator::Default::get_p_init " << p_init_ << std::endl;
  std::cout << ">>>>" << (*p_init_)[0] << "<<<<" << std::endl;

  return p_init_;
}
// ============================================================================
Teuchos::RCP<const Epetra_Map>
Ginla::ModelEvaluator::Default::
get_p_map(int l) const
{
  TEUCHOS_ASSERT_EQUALITY( 0, l );
  std::cout << "get_p_map" << std::endl;
  return p_map_;
}
// ============================================================================
Teuchos::RCP<const Teuchos::Array<std::string> >
Ginla::ModelEvaluator::Default::
get_p_names (int l) const
{
  TEUCHOS_ASSERT_EQUALITY( 0, l );
  std::cout << *p_names_ << std::endl;
  return p_names_;
}
// ============================================================================
Teuchos::RCP<Epetra_Operator>
Ginla::ModelEvaluator::Default::
create_W() const
{
  return komplex_->getMatrix();
}
// ============================================================================
EpetraExt::ModelEvaluator::InArgs
Ginla::ModelEvaluator::Default::
createInArgs() const
{
  EpetraExt::ModelEvaluator::InArgsSetup inArgs;
  inArgs.setModelEvalDescription( "Ginzburg--Landau extreme type-II on a square" );
  inArgs.set_Np( 1 );
  inArgs.setSupports( IN_ARG_x, true );
  
  return inArgs;
}
// ============================================================================
EpetraExt::ModelEvaluator::OutArgs
Ginla::ModelEvaluator::Default::
createOutArgs() const
{
  EpetraExt::ModelEvaluator::OutArgsSetup outArgs;
  outArgs.setModelEvalDescription("Ginzburg--Landau extreme type-II on a square");
  outArgs.setSupports( OUT_ARG_f, true );
  outArgs.setSupports( OUT_ARG_W, true );
  outArgs.set_Np_Ng( 1 , 0 );
  outArgs.set_W_properties( DerivativeProperties( DERIV_LINEARITY_NONCONST,
                                                  DERIV_RANK_FULL,
                                                  false // supportsAdjoint
                                                )
                          );
  return outArgs;
}
// ============================================================================
void
Ginla::ModelEvaluator::Default::
evalModel( const InArgs  & inArgs, 
           const OutArgs & outArgs
         ) const
{
  const Teuchos::RCP<const Epetra_Vector> & x = inArgs.get_x();
  
  Teuchos::RCP<Epetra_Vector>   f_out = outArgs.get_f();
  Teuchos::RCP<Epetra_Operator> W_out = outArgs.get_W();
  
  // Parse InArgs
  Teuchos::RCP<const Epetra_Vector> p_in = inArgs.get_p(0);
  TEUCHOS_ASSERT( !p_in.is_null() );
  //int numParameters = p_in->GlobalLength();
  
  // compute F
  if (!f_out.is_null())
      this->computeF_( *x, *f_out ); 

  if( !W_out.is_null() )
  {
      // fill jacobian
      Teuchos::RCP<Epetra_RowMatrix> tmp =
             Teuchos::rcp_dynamic_cast<Epetra_RowMatrix>(W_out);
      this->computeJacobian_( *x, *W_out );
  }

  return;
}
// ============================================================================
void
Ginla::ModelEvaluator::Default::
computeF_ ( const Epetra_Vector & x,
            Epetra_Vector       & FVec
          ) const
{
    // ------------------------------------------------------------------------
    // convert from x to psi
    const Teuchos::RCP<ComplexVector> psi = komplex_->real2complex ( x );
    // ------------------------------------------------------------------------
    // compute the GL residual
    // setup output vector with the same map as psi
    Teuchos::RCP<ComplexVector> res =
        Teuchos::rcp ( new ComplexVector ( psi->getMap(), true ) );

    // TODO not really necessary?
    glOperator_->updatePsi ( psi );

    Teuchos::ArrayRCP<double_complex> resView = res->get1dViewNonConst();
    // loop over the nodes
    for ( unsigned int k=0; k<psi->getLocalLength(); k++ )
    {
        int globalIndex = psi->getMap()->getGlobalElement ( k );
        resView[k] = glOperator_->getEntry ( globalIndex );

//         if ( !perturbation_.is_null() )
//             resView[k] += perturbation_->computePerturbation ( globalIndex );
    }
    // ------------------------------------------------------------------------
    // TODO Avoid this explicit copy?
    // transform back to fully real equation
    FVec = * ( komplex_->complex2real ( *res ) );
    // ------------------------------------------------------------------------
    
    return;
}
// ============================================================================
void
Ginla::ModelEvaluator::Default::
computeJacobian_ ( const Epetra_Vector & x,
                   Epetra_Operator     & Jac
                 ) const
{
    Teuchos::Array<int> indicesA, indicesB;
    Teuchos::Array<double_complex> valuesA, valuesB;

    komplex_->zeroOutMatrix();

    Teuchos::RCP<ComplexVector> psi = komplex_->real2complex ( x );

    // update to the latest psi vector before retrieving the Jacobian
    glOperator_->updatePsi ( psi );
    
    // loop over the rows and fill the matrix
    int numMyElements = komplex_->getComplexMap()->getNodeNumElements();
    for ( int row = 0; row < numMyElements; row++ )
    {
        int globalRow = komplex_->getComplexMap()->getGlobalElement ( row );
        // get the values from the operator
        glOperator_->getJacobianRow ( globalRow,
                                      indicesA, valuesA,
                                      indicesB, valuesB );
        // ... and fill them into glKomplex_
        komplex_->updateRow ( globalRow,
                              indicesA, valuesA,
                              indicesB, valuesB,
                              firstTime_ );
    }

    if ( firstTime_ )
    {
        komplex_->finalizeMatrix();
        firstTime_ = false;
    }

    return;
}
// ============================================================================
Teuchos::RCP<const Ginla::Komplex>
Ginla::ModelEvaluator::Default::
getKomplex() const
{
    return komplex_; 
}
// ============================================================================