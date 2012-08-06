// @HEADER
//
//    Regularized kinetic energy operator.
//    Copyright (C) 2010--2012  Nico Schl\"omer
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

#include "Nosh_BorderedOperator.hpp"

#include "Nosh_BorderingHelpers.hpp"

// =============================================================================
namespace Nosh {
// =============================================================================
BorderedOperator::
BorderedOperator(const Teuchos::RCP<Epetra_Operator> & innerOperator,
                 const Teuchos::RCP<Epetra_Vector> & b,
                 const Teuchos::RCP<Epetra_Vector> & c,
                 const double d
                 ):
  innerOperator_( innerOperator ),
  b_( b ),
  c_( c ),
  d_( d ),
  useTranspose_( false ),
  domainMap_(*Nosh::BorderingHelpers::extendMapBy1(innerOperator_->OperatorDomainMap())),
  rangeMap_(*Nosh::BorderingHelpers::extendMapBy1(innerOperator_->OperatorRangeMap()))
{
}
// =============================================================================
BorderedOperator::
~BorderedOperator()
{
}
// =============================================================================
int
BorderedOperator::
SetUseTranspose(bool useTranspose)
{
  innerOperator_->SetUseTranspose(useTranspose);
  useTranspose_ = useTranspose;
  return 0;
}
// =============================================================================
int
BorderedOperator::
Apply(const Epetra_MultiVector &X,
      Epetra_MultiVector &Y
      ) const
{
#if _DEBUG_
  TEUCHOS_ASSERT(X.Map().IsSameAs(*domainMap_));
  TEUCHOS_ASSERT(Y.Map().IsSameAs(*rangeMap_));
#endif
  const int n = X.NumVectors();
#if _DEBUG_
  TEUCHOS_ASSERT_EQUALITY(n, Y.NumVectors());
#endif
  // Dissect X.
  Epetra_Vector innerX(innerOperator_->OperatorDomainMap());
  double lambda[n];
  Nosh::BorderingHelpers::dissect(X, innerX, lambda);
  // Apply inner operator.
  Epetra_Vector innerY(innerOperator_->OperatorRangeMap());
  TEUCHOS_ASSERT_EQUALITY(0, innerOperator_->Apply(innerX, innerY));

  Teuchos::RCP<Epetra_Vector> rightBordering;
  Teuchos::RCP<Epetra_Vector> lowerBordering;
  if (useTranspose_)
  {
    rightBordering = c_;
    lowerBordering = b_;
  }
  else
  {
    rightBordering = b_;
    lowerBordering = c_;
  }

  // Add right bordering.
  for (int k=0; k<n; k++)
    TEUCHOS_ASSERT_EQUALITY(0, innerY(k)->Update(lambda[k], *rightBordering, 1.0));

  // Add lower bordering.
  double alpha[n];
  TEUCHOS_ASSERT_EQUALITY(0, lowerBordering->Dot(innerX, alpha));
  for (int k=0; k<n; k++)
    alpha[k] += lambda[k] * d_;

  // Merge it all together.
  Nosh::BorderingHelpers::merge(innerY, alpha, Y);

  return 0;
}
// =============================================================================
int
BorderedOperator::
ApplyInverse(const Epetra_MultiVector &X,
             Epetra_MultiVector &Y
             ) const
{
  TEUCHOS_TEST_FOR_EXCEPT_MSG(false,
                              "Not yet implemented.");
  return 0;
}
// =============================================================================
double
BorderedOperator::
NormInf() const
{
  TEUCHOS_TEST_FOR_EXCEPT_MSG(false,
                              "Not yet implemented.");
  return 0.0;
}
// =============================================================================
const char *
BorderedOperator::
Label() const
{
  return "Bordered operator";
}
// =============================================================================
bool
BorderedOperator::
UseTranspose() const
{
  return useTranspose_;
}
// =============================================================================
bool
BorderedOperator::
HasNormInf() const
{
  return false;
}
// =============================================================================
const Epetra_Comm &
BorderedOperator::
Comm() const
{
  return innerOperator_->Comm();
}
// =============================================================================
const Epetra_Map &
BorderedOperator::
OperatorDomainMap() const
{
  return domainMap_;
}
// =============================================================================
const Epetra_Map &
BorderedOperator::
OperatorRangeMap() const
{
  return rangeMap_;
}
// =============================================================================
const Teuchos::RCP<Epetra_Operator>
BorderedOperator::
getInnerOperator() const
{
  return innerOperator_;
}
// =============================================================================
} // namespace Nosh
