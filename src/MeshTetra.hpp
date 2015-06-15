// @HEADER
//
//    Mesh class with compatibility to stk_mesh.
//    Copyright (C) 2010--2012  Nico Schlömer
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
#ifndef NOSH_MESHTETRA_HPP
#define NOSH_MESHTETRA_HPP
// =============================================================================
//// includes
#include <vector>
#include <set>

//#include <Teuchos_DefaultComm.hpp>
//#ifdef NOSH_TEUCHOS_TIME_MONITOR
//#include <Teuchos_Time.hpp>
//#endif
#include <Tpetra_Vector.hpp>

//#include <stk_mesh/base/Entity.hpp>
//#include <stk_mesh/base/CoordinateSystems.hpp>
//#include <stk_mesh/base/MetaData.hpp>
//#include <stk_io/StkMeshIoBroker.hpp>
//#include <stk_mesh/base/FieldTraits.hpp>

#include <Eigen/Dense>

#include "Mesh.hpp"

namespace Nosh
{

class MeshTetra:
  public Mesh
{

public:
  MeshTetra(
      const std::shared_ptr<const Teuchos::Comm<int>> & comm,
      const std::shared_ptr<stk::io::StkMeshIoBroker> & broker
      );

  virtual
  ~MeshTetra();

  virtual
  std::shared_ptr<const Tpetra::Vector<double,int,int>>
  getControlVolumes() const
  {
    return controlVolumes_;
  }

  double
  getDomainVolume() const
  {
    return controlVolumes_->norm1();
  }

  std::vector<double>
  getEdgeCoefficients() const
  {
    return edgeCoefficients_;
  }

private:

  std::vector<double>
  computeEdgeCoefficients_() const;

  double
  computeCovolume_(
      const Eigen::Vector3d &cc,
      const Eigen::Vector3d &x0,
      const Eigen::Vector3d &x1,
      const Eigen::Vector3d &other0,
      const Eigen::Vector3d &other1
      ) const;

  std::set<unsigned int>
  getOtherIndices_(unsigned int e0, unsigned int e1) const;

  double
  getCellVolume_(
      const Eigen::Vector3d &edge0,
      const Eigen::Vector3d &edge1,
      const Eigen::Vector3d &edge2
      ) const;

  Eigen::Vector3d
  computeCellCircumcenter_(
    const std::vector<Eigen::Vector3d> &nodes
    ) const;

  Eigen::VectorXd
  getEdgeCoefficientsNumerically_(
    const std::vector<Eigen::Vector3d> edges
    ) const;

  //! Compute the volume of the (Voronoi) control cells for each point.
  std::shared_ptr<Tpetra::Vector<double,int,int>>
  computeControlVolumes_() const;

  void
  computeControlVolumesT_(Tpetra::Vector<double,int,int> & cvOverlap) const;

  double
  getTetrahedronVolume_(
      const Eigen::Vector3d &edge0,
      const Eigen::Vector3d &edge1,
      const Eigen::Vector3d &edge2
      ) const;

  Eigen::Vector3d
  computeTetrahedronCircumcenter_(
      const std::vector<Eigen::Vector3d> &nodes
      ) const;

  double
  computeCovolume3d_(
      const Eigen::Vector3d &cc,
      const Eigen::Vector3d &x0,
      const Eigen::Vector3d &x1,
      const Eigen::Vector3d &other0,
      const Eigen::Vector3d &other1
      ) const;

private:
  const std::shared_ptr<const Tpetra::Vector<double,int,int>> controlVolumes_;
  const std::vector<double> edgeCoefficients_;

};

} // namespace Nosh
// =============================================================================
#endif // NOSH_MESHTETRA_HPP
