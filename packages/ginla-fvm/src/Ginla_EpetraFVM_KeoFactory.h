/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2010  Nico Schl\"omer

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

#ifndef GINLA_EPETRAFVM_KEOFACTORY_H
#define GINLA_EPETRAFVM_KEOFACTORY_H
// =============================================================================
#include <Epetra_Operator.h>
#include <Teuchos_RCP.hpp>
#include <Teuchos_Tuple.hpp>
#include <Epetra_FECrsGraph.h>
#include <Epetra_FECrsMatrix.h>
#include <Epetra_LinearProblem.h>

#include "Ginla_MagneticVectorPotential_Virtual.h"
#include <Amesos.h>
// =============================================================================
// forward declarations
namespace Ginla {
  namespace EpetraFVM {
    class StkMesh;
  }
}
// =============================================================================
namespace Ginla {
namespace EpetraFVM {
// =============================================================================
class KeoFactory
{
public:
    KeoFactory( const Teuchos::RCP<Ginla::EpetraFVM::StkMesh>               & mesh,
                const Teuchos::RCP<Ginla::MagneticVectorPotential::Virtual> & mvp
              );

    // Destructor.
    ~KeoFactory();

    void
    buildKeo( Epetra_FECrsMatrix & keoMatrix,
              const Teuchos::RCP<const LOCA::ParameterVector> & mvpParams,
              const Teuchos::Tuple<double,3> & scaling
            ) const;

    const Epetra_FECrsGraph
    buildKeoGraph() const;

protected:
private:
    const Teuchos::RCP<Ginla::EpetraFVM::StkMesh> mesh_;
    const Teuchos::RCP<Ginla::MagneticVectorPotential::Virtual> mvp_;
};
// =============================================================================
} // namespace FVM
} // namespace Ginla

#endif // GINLA_EPETRAFVM_KEOFACTORY_H
