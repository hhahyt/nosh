/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

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

#ifndef VIO_MESH_READER_H
#define VIO_MESH_READER_H

#include "VIO_Reader_Abstract.h"

#include <string>
#include <Teuchos_RCP.hpp>

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

// forward declarations
namespace VIO {
  namespace Mesh {
      class Mesh;
  }
}

namespace VIO {
 namespace Mesh {
class Reader:
   public VIO::Reader::Abstract
{
  public:
      Reader ( const std::string & filePath
             );
      
      ~Reader ();

      void
      read ( Teuchos::RCP<Mesh> & mesh
           );
           
  protected:
  private:
    
    void
    processVtkData_( const vtkSmartPointer<vtkUnstructuredGrid> & vtkMesh,
                     Teuchos::RCP<Mesh>                         & mesh
                   );
    
  private:
};
}
}

#endif // VIO_MESH_READER_H
