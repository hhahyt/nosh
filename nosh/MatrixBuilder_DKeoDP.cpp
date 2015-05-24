// @HEADER
//
//    Builds the kinetic energy operator and its variants.
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
// =============================================================================
// includes
#include "nosh/MatrixBuilder_DKeoDP.hpp"

#include <map>
#include <string>

#include "nosh/StkMesh.hpp"
#include "nosh/ScalarField_Virtual.hpp"
#include "nosh/VectorField_Virtual.hpp"

#include <Epetra_SerialDenseMatrix.h>
#include <Epetra_Comm.h>
#include <Epetra_Vector.h>
#include <Epetra_Import.h>

#include <ml_epetra_preconditioner.h>

#ifdef NOSH_TEUCHOS_TIME_MONITOR
#include <Teuchos_TimeMonitor.hpp>
#endif

namespace Nosh
{
namespace MatrixBuilder
{
// =============================================================================
DKeoDP::
DKeoDP(
    const Teuchos::RCP<const Nosh::StkMesh> &mesh,
    const Teuchos::RCP<const Nosh::ScalarField::Virtual> &thickness,
    const Teuchos::RCP<const Nosh::VectorField::Virtual> &mvp,
    const std::string & paramName
   ):
  Virtual(mesh),
#ifdef NOSH_TEUCHOS_TIME_MONITOR
  keoFillTime_(Teuchos::TimeMonitor::getNewTimer("Nosh: DKeoDP::fill_")),
#endif
  thickness_(thickness),
  mvp_(mvp),
  matrixCache_(Copy, graph_),
  keoBuildParameters_(),
  alphaCache_(),
  alphaCacheUpToDate_(false),
  paramName_(paramName)
{
}
// =============================================================================
DKeoDP::
~DKeoDP()
{
}
// =============================================================================
void
DKeoDP::
apply(const std::map<std::string, double> & params,
      const Epetra_Vector &X,
      Epetra_Vector &Y
     ) const
{
  // Unconditionally rebuild the cache.
  // It would be little effort to also wrap this into a conditional
  // with mvpParams and parmamIndex, but it never seems to occur
  // that dK/dp with the same parameter needs to be applied
  // twice in a row.
  this->fill_(matrixCache_, params, &DKeoDP::fillerDp_);

  TEUCHOS_ASSERT_EQUALITY(0, matrixCache_.Apply(X, Y));
  return;
}
// =============================================================================
void
DKeoDP::
fill(
    Epetra_FECrsMatrix &matrix,
    const std::map<std::string, double> & params
    ) const
{
  // Cache the construction of the KEO.
  // This is useful because in the continuation context,
  // getKeo() is called a number of times with the same arguments
  // (in computeF, getJacobian(), and getPreconditioner().
  bool needsRebuild;
  if (keoBuildParameters_.empty()) {
    needsRebuild = true;
  } else {
    needsRebuild = false;
    for (auto it = keoBuildParameters_.begin();
         it != keoBuildParameters_.end();
         ++it) {
      // Check if it->first is in params at all and if their values are equal.
      std::map<std::string, double>::const_iterator it2 =
        params.find(it->first);
      TEUCHOS_ASSERT(it2 != params.end());
      if (it2->second != it->second) {
        needsRebuild = true;
        break;
      }
    }
  }

  if (needsRebuild) {
    this->fill_(matrixCache_, params, &DKeoDP::fillerDp_);
    // Reset build parameters.
    for (auto it = keoBuildParameters_.begin();
         it != keoBuildParameters_.end();
         ++it) {
      std::map<std::string, double>::const_iterator it2 =
        params.find(it->first);
      TEUCHOS_ASSERT(it2 != params.end());
      it->second = it2->second;
    }
  }

  matrix = matrixCache_;
  return;
}
// =============================================================================
const std::map<std::string, double>
DKeoDP::
getInitialParameters() const
{
  return mvp_->getInitialParameters();
}
// =============================================================================
void
DKeoDP::
fillerDp_(
    const int k,
    const std::map<std::string, double> & params,
    double * v
    ) const
{
  double aInt = mvp_->getEdgeProjection(k, params);
  // paramName_ is set in the KEO building routine.
  double dAdPInt = mvp_->getDEdgeProjectionDp(k, params, paramName_);
  //sincos(aInt, &sinAInt, &cosAInt);
  v[0] =  dAdPInt * sin(aInt);
  v[1] = -dAdPInt * cos(aInt);
  v[2] = 0.0;
  return;
}
// =============================================================================
void
DKeoDP::
fill_(
    Epetra_FECrsMatrix &keoMatrix,
    const std::map<std::string, double> & params,
    void (DKeoDP::*filler)(const int,
                           const std::map<std::string, double> &,
                           double*
                           ) const
    ) const
{
#ifdef NOSH_TEUCHOS_TIME_MONITOR
  Teuchos::TimeMonitor tm(*keoFillTime_);
#endif
  // Zero-out the matrix.
  TEUCHOS_ASSERT_EQUALITY(0, keoMatrix.PutScalar(0.0));

#ifndef NDEBUG
  TEUCHOS_ASSERT(!mesh_.is_null());
#endif
  // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
  // Loop over the cells, create local load vector and mass matrix,
  // and insert them into the global matrix.
#ifndef NDEBUG
  TEUCHOS_ASSERT(!thickness_.is_null());
  TEUCHOS_ASSERT(!mvp_.is_null());
#endif

  const Teuchos::Array<edge> edges = mesh_->getEdgeNodes();
  if (!alphaCacheUpToDate_)
    this->buildAlphaCache_(edges, mesh_->getEdgeCoefficients());

  double v[3];
  Epetra_SerialDenseMatrix A(4, 4);
  // Loop over all edges.
  for (auto k = 0; k < edges.size(); k++) {
    // ---------------------------------------------------------------
    // Compute the integral
    //
    //    I = \int_{x0}^{xj} (xj-x0).A(x) / |xj-x0| dx
    //
    // numerically by the midpoint rule, i.e.,
    //
    //    I ~ |xj-x0| * (xj-x0) . A(0.5*(xj+x0)) / |xj-x0|.
    //
    // -------------------------------------------------------------------
    // Project vector field onto the edge.
    // Instead of first computing the projection over the normalized edge
    // and then multiply it with the edge length, don't normalize the
    // edge vector.
    (this->*filler)(k, params, v);
    // We'd like to insert the 2x2 matrix
    //
    //     [   alpha                   , - alpha * exp(-IM * aInt) ]
    //     [ - alpha * exp(IM * aInt),   alpha                       ]
    //
    // at the indices   [ nodeIndices[0], nodeIndices[1] ] for every index pair
    // that shares and edge.
    // Do that now, just blockwise for real and imaginary part.
    v[0] *= alphaCache_[k];
    v[1] *= alphaCache_[k];
    v[2] *= alphaCache_[k];
    A(0, 0) = v[2];
    A(0, 1) = 0.0;
    A(0, 2) = v[0];
    A(0, 3) = v[1];
    A(1, 0) = 0.0;
    A(1, 1) = v[2];
    A(1, 2) = -v[1];
    A(1, 3) = v[0];
    A(2, 0) = v[0];
    A(2, 1) = -v[1];
    A(2, 2) = v[2];
    A(2, 3) = 0.0;
    A(3, 0) = v[1];
    A(3, 1) = v[0];
    A(3, 2) = 0.0;
    A(3, 3) = v[2];
    TEUCHOS_ASSERT_EQUALITY(
        0,
        keoMatrix.SumIntoGlobalValues(globalIndexCache_[k], A)
        );
    // -------------------------------------------------------------------
  }

  // calls FillComplete by default
  TEUCHOS_ASSERT_EQUALITY(0, keoMatrix.GlobalAssemble());
  return;
}
// =============================================================================
void
DKeoDP::
buildAlphaCache_(
    const Teuchos::Array<edge> & edges,
    const Teuchos::ArrayRCP<const double> &edgeCoefficients
    ) const
{
  // This routine serves the one and only purpose of caching the
  // thickness average. The cache is used in every call to this->fill().
  // This is somewhat problematic since the map of V is principally
  // not known here. Also, it is typically a nonoverlapping map whereas
  // some edges do sit on a processor boundary, so actually the values
  // of V are needed in an overlapping map.
  // Fair enough. Let's distribute the vales of V to an overlapping
  // map here.
  alphaCache_ = Teuchos::ArrayRCP<double>(edges.size());

  std::map<std::string, double> dummy;
  const Epetra_Vector thicknessValues = thickness_->getV(dummy);

  Teuchos::RCP<const Epetra_Map> overlapMap = mesh_->getNodesOverlapMap();
  // We need to make sure that thicknessValues are distributed on
  // the overlap map.
  // Make sure to use Import here instead of Export as the vector
  // that we want to build is overlapping, "larger". If the "smaller",
  // non-overlapping vector is exported, only the values on the overlap
  // would only be set on one processor.
  Epetra_Vector thicknessOverlap(*overlapMap);
  Epetra_Import importer(*overlapMap, thicknessValues.Map());
  TEUCHOS_ASSERT_EQUALITY(
      0,
      thicknessOverlap.Import(thicknessValues, importer, Insert)
      );

  Teuchos::Tuple<int, 2> gid;
  Teuchos::Tuple<int, 2> lid;
  for (Teuchos::Array<edge>::size_type k = 0;
       k < edges.size();
       k++) {
    // Get the ID of the edge endpoints in the map of
    // getV(). Well...
    gid[0] = mesh_->gid(std::get<0>(edges[k]));
    lid[0] = overlapMap->LID(gid[0]);
#ifndef NDEBUG
    TEUCHOS_TEST_FOR_EXCEPT_MSG(
        lid[0] < 0,
        "The global index " << gid[0]
        << " does not seem to be present on this node."
       );
#endif
    gid[1] = mesh_->gid(std::get<1>(edges[k]));
    lid[1] = overlapMap->LID(gid[1]);
#ifndef NDEBUG
    TEUCHOS_TEST_FOR_EXCEPT_MSG(
        lid[1] < 0,
        "The global index " << gid[1]
        << " does not seem to be present on this node."
       );
#endif
    // Update cache.
    alphaCache_[k] = edgeCoefficients[k]
      * 0.5 * (thicknessOverlap[lid[0]] + thicknessOverlap[lid[1]]);
  }

  alphaCacheUpToDate_ = true;

  return;
}
// =============================================================================
}  // namespace MatrixBuilder
}  // namespace Nosh
