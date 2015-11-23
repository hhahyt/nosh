// @HEADER
//
//    Query routines for a vector potential with explicitly given values.
//    Copyright (C) 2012  Nico Schlömer
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

#include "vector_field_explicit_values.hpp"

#include <map>
#include <string>

#include "mesh.hpp"

namespace nosh
{
namespace vector_field
{
// ============================================================================
explicit_values::
explicit_values(
    const nosh::mesh & mesh,
    const std::string & field_name,
    const double mu
    ) :
  mu_(mu),
  edgeProjectionCache_(mesh.my_edges().size())
{
#if 0
  // Initialize the cache.
  const std::vector<edge> edges = mesh.my_edges();

  const vector_fieldType & coords_field = mesh.get_node_field("coordinates");

  const vector_fieldType & dataField = mesh.get_node_field(field_name);

  // Loop over all edges and create the cache.
  for (std::size_t k = 0; k < edges.size(); k++) {
    // Approximate the value at the midpoint of the edge
    // by the average of the values at the adjacent nodes.
    Eigen::Vector3d av = 0.5 * (
        mesh.get_node_value(dataField, std::get<0>(edges[k]))
        + mesh.get_node_value(dataField, std::get<1>(edges[k]))
        );

    // Extract the nodal coordinates.
    Eigen::Vector3d my_edge =
      mesh.get_node_value(coords_field, std::get<1>(edges[k]))
      - mesh.get_node_value(coords_field, std::get<0>(edges[k]));

    edgeProjectionCache_[k] = av.dot(my_edge);
  }

// TODO resurrect this
//#ifndef NDEBUG
#if 0
  // Do a quick sanity check for the edgeProjectionCache_.  It happens too
  // often that the reader elements aren't specified correctly and stk_io
  // *silently* "reads" only zeros.  Use the fake logical "isNonzeroLocal"
  // since Teuchos::Comm<int> doesn't have logical any() or all() operations.
  bool isZeroLocal = true;
  for (std::size_t k = 0; k < edgeProjectionCache_.size(); k++) {
    if (fabs(edgeProjectionCache_[k]) > 1.0e-10) {
      isZeroLocal = false;
      break;
    }
  }
  bool isZeroGlobal;
  Teuchos::reduceAll(
      *mesh.getComm(),
      Teuchos::REDUCE_AND,
      1,
      &isZeroLocal,
      &isZeroGlobal
      );

  TEUCHOS_TEST_FOR_EXCEPT_MSG(
      isZeroGlobal,
      "Field \"" << field_name << "\" seems empty. Was it read correctly?"
      );
#endif
#endif
  return;
}
// ============================================================================
explicit_values::
~explicit_values()
{
}
// ============================================================================
void
explicit_values::
set_parameters(const std::map<std::string, double> & params)
{
  mu_ = params.at("mu");
  return;
}
// ============================================================================
const std::map<std::string, double>
explicit_values::
get_parameters() const
{
  return {{"mu", mu_}};
}
// ============================================================================
double
explicit_values::
get_edge_projection(const unsigned int edge_index) const
{
  return mu_ * edgeProjectionCache_[edge_index];
}
// ============================================================================
double
explicit_values::
get_d_edge_projection_dp(
    const unsigned int edge_index,
    const std::string & dParamName
    ) const
{
  if (dParamName.compare("mu") == 0)
    return edgeProjectionCache_[edge_index];
  else
    return 0.0;
}
// ============================================================================
} // namespace vector_field
} // namespace nosh
