// @HEADER
//
//    Virtual class for matrix constructors.
//    Copyright (C) 2012  Nico Schl\"omer
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
#ifndef NOSH_PARAMETEROBJECT
#define NOSH_PARAMETEROBJECT

#include <map>
#include <string>

#include <Tpetra_CrsMatrix.hpp>
#include <Teuchos_RCP.hpp>

// forward declarations
namespace Nosh
{
class Mesh;
}

namespace Nosh
{
class ParameterObject
{
public:
  ParameterObject();

  // Destructor.
  virtual
  ~ParameterObject();

  //! Fill the matrix with the parameter entries as given in params.
  //! Includes some caching logic for params.
  virtual
  void
  setParameters(const std::map<std::string, double> &params) final;

  //! Get parameter map with their initial values.
  virtual
  const std::map<std::string, double>
  getParameters() const = 0;

protected:
  //! Fill the matrix with the parameter entries as given in params.
  virtual
  void
  refill_(const std::map<std::string, double> &params) = 0;

private:
  std::map<std::string, double> buildParameters_;
};
} // namespace Nosh

#endif // NOSH_PARAMETEROBJECT
