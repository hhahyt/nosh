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
// =============================================================================
#include "Ginla_EpetraFVM_StkMeshReader.h"
#include "Ginla_EpetraFVM_StkMesh.h"

#include <Epetra_Map.h>
#include <Epetra_Vector.h>
#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/Field.hpp>
#include <stk_mesh/base/FieldData.hpp>
#include <stk_mesh/base/GetEntities.hpp>

#include <stk_io/IossBridge.hpp>
#include <stk_io/MeshReadWriteUtils.hpp>
#include <Ionit_Initializer.h>
// =============================================================================
Ginla::EpetraFVM::StkMeshReader::
StkMeshReader( const std::string & fileName ):
fileName_( fileName )
{
}
// =============================================================================
Ginla::EpetraFVM::StkMeshReader::
~StkMeshReader()
{
}
// =============================================================================
void
Ginla::EpetraFVM::StkMeshReader::
read( const Epetra_Comm                       & comm,
      Teuchos::RCP<Epetra_Vector>             & psi,
      Teuchos::RCP<Epetra_MultiVector>        & mvp,
      Teuchos::RCP<Epetra_Vector>             & thickness,
      Teuchos::RCP<Ginla::EpetraFVM::StkMesh> & mesh,
      Teuchos::ParameterList                  & parameterList
    )
{
  // Take two different fields with one component
  // instead of one field with two components. This works around
  // Ioss's inability to properly read psi_R, psi_Z as a complex variable.
  // (It can handle data_X, data_Y, data_Z though.)
  const unsigned int neq = 1;

  Teuchos::RCP<stk::mesh::fem::FEMMetaData> metaData =
      Teuchos::rcp( new stk::mesh::fem::FEMMetaData() );

  int numDim = 3;
  if (! metaData->is_FEM_initialized())
      metaData->FEM_initialize(numDim);

//   // attach fem data
//   size_t spatial_dimension = 3;
//   stk::mesh::DefaultFEM fem( *metaData, spatial_dimension );

  unsigned int field_data_chunk_size = 1001;
  Teuchos::RCP<stk::mesh::BulkData> bulkData =
      Teuchos::rcp( new stk::mesh::BulkData( stk::mesh::fem::FEMMetaData::get_meta_data(*metaData),
                                             MPI_COMM_WORLD,
                                             field_data_chunk_size
                                           )
                  );

  Teuchos::RCP<VectorFieldType> coordinatesField =
       Teuchos::rcpFromRef( metaData->declare_field< VectorFieldType >( "coordinates" ) );
  stk::io::set_field_role(*coordinatesField, Ioss::Field::ATTRIBUTE);

  // real part
  Teuchos::RCP<ScalarFieldType> psir_field =
      Teuchos::rcpFromRef( metaData->declare_field< ScalarFieldType >( "psi_R" ) );
  stk::mesh::put_field( *psir_field , metaData->node_rank() , metaData->universal_part() );
  stk::io::set_field_role(*psir_field, Ioss::Field::TRANSIENT);

  // imaginary part
  Teuchos::RCP<ScalarFieldType> psii_field =
      Teuchos::rcpFromRef( metaData->declare_field< ScalarFieldType >( "psi_Z" ) );
  stk::mesh::put_field( *psii_field , metaData->node_rank() , metaData->universal_part() );
  stk::io::set_field_role(*psii_field, Ioss::Field::TRANSIENT);

  // Magnetic vector potential.
  // Declare those fields as TRANSIENT to make sure they are written out to the
  // exodus file. Note, however, that this creates a large data overhead as the
  // same data is written out in each step although the data don't change.
  //
  // On 05/11/2011 02:44 PM, Gregory Sjaardema wrote:
  // For now, the B and C fields will have to be declared as TRANSIENT fields
  // since they are nodal fields on the universal set which currently doesn't
  // support attributes.  One of the stories I was supposed to work on for
  // this sprint was increasing the exodus capabilities supported by Ioss
  // (used underneath stk_io) and attributes on nodeblocks is one of the
  // things supported by exodus, but not by Ioss.  However, I got bogged down
  // by some big debugging and didn't finish that story.  Hopefully, it will
  // be done in June at which time you could use attribute fields on the
  // universal set...
  Teuchos::RCP<ScalarFieldType> mvpXField =
       Teuchos::rcpFromRef( metaData->declare_field< ScalarFieldType >( "AX" ) );
  stk::mesh::put_field( *mvpXField , metaData->node_rank() , metaData->universal_part() );
  stk::io::set_field_role(*mvpXField, Ioss::Field::TRANSIENT);

  Teuchos::RCP<ScalarFieldType> mvpYField =
       Teuchos::rcpFromRef( metaData->declare_field< ScalarFieldType >( "AY" ) );
  stk::mesh::put_field( *mvpYField , metaData->node_rank() , metaData->universal_part() );
  stk::io::set_field_role(*mvpYField, Ioss::Field::TRANSIENT);

  Teuchos::RCP<ScalarFieldType> mvpZField =
       Teuchos::rcpFromRef( metaData->declare_field< ScalarFieldType >( "AZ" ) );
  stk::mesh::put_field( *mvpZField , metaData->node_rank() , metaData->universal_part() );
  stk::io::set_field_role(*mvpZField, Ioss::Field::TRANSIENT);

  // Thickness fields. Same as above.
  Teuchos::RCP<ScalarFieldType> thicknessField =
       Teuchos::rcpFromRef( metaData->declare_field< ScalarFieldType >( "thickness" ) );
  stk::mesh::put_field( *thicknessField , metaData->node_rank() , metaData->universal_part() );
  stk::io::set_field_role(*thicknessField, Ioss::Field::TRANSIENT);

  Teuchos::RCP<stk::io::MeshData> meshData =
      Teuchos::rcp( new stk::io::MeshData() );

  Ioss::Init::Initializer io;

  stk::io::create_input_mesh( "exodusii",
                              fileName_,
                              MPI_COMM_WORLD,
                              *metaData,
                              *meshData
                            );

  stk::io::define_input_fields( *meshData,
                                *metaData
                              );

//  stk::io::put_io_part_attribute( metaData->universal_part() );

  metaData->commit();

  stk::io::populate_bulk_data( *bulkData,
                               *meshData
                             );

//  // add parameter
//  meshData->m_region->field_add( Ioss::Field( "mu",
//                                              Ioss::Field::REAL,
//                                              "scalar",
//                                              Ioss::Field::REDUCTION,
//                                              1
//                                            )
//                               );

  bulkData->modification_end();


  // Restart index to read solution from exodus file.
//   int index = -1; // Default to no restart
  int index = 1; // restart from the first step
  if ( comm.MyPID() == 0 )
      if ( index<1 )
        std::cout << "Restart Index not set. Not reading solution from exodus (" << index << ")"<< endl;
      else
        std::cout << "Restart Index set, reading solution time step: " << index << endl;

  stk::io::process_input_request( *meshData,
                                  *bulkData,
                                  index
                                );

  // coordinatesField = Teuchos::rcpFromRef( metaData->get_field<VectorFieldType>( std::string("coordinates") ) );

  // create the mesh with these specifications
  mesh = Teuchos::rcp( new Ginla::EpetraFVM::StkMesh( comm, metaData, bulkData, coordinatesField ) );

  // create the state
  psi       = this->createPsi_( mesh, psir_field, psii_field );
  mvp       = this->createMvp_( mesh, mvpXField, mvpYField, mvpZField );
  thickness = this->createThickness_( mesh, thicknessField );

  // These are vain attempts to find out whether thicknessField is actually empty.
//     const stk::mesh::FieldBase::RestrictionVector & restrictions = thicknessField->restrictions();
//     TEUCHOS_ASSERT( !restrictions.empty() );
//     std::cout << "max_size " << thicknessField->max_size(metaData->node_rank()) << std::endl;

  // Check of the thickness data is of any value. If not: ditch it.
  double norminf;
  thickness->NormInf( &norminf );
  if ( norminf < 1.0e-15 )
      thickness->PutScalar( 1.0 );

  // Add some dummy data.
  // TODO Replace by proper values.
  parameterList.set<double>( "mu", 0.0 );
  parameterList.set<double>( "scaling", 1.0 );

  return;
}
// =============================================================================
Teuchos::RCP<Epetra_Vector>
Ginla::EpetraFVM::StkMeshReader::
createPsi_( const Teuchos::RCP<const Ginla::EpetraFVM::StkMesh> & mesh,
            const Teuchos::RCP<ScalarFieldType>                 & psir_field,
            const Teuchos::RCP<ScalarFieldType>                 & psii_field
          ) const
{
    // Get owned nodes.
    const std::vector<stk::mesh::Entity*> & ownedNodes = mesh->getOwnedNodes();

    // Create vector with this respective map.
    Teuchos::RCP<Epetra_Vector> psi = Teuchos::rcp( new Epetra_Vector( *mesh->getComplexMap() ) );

    // Fill the vector with data from the file
    int ind;
    for ( int k=0; k<ownedNodes.size(); k++ )
    {
        // real part
        double* psirVal = stk::mesh::field_data( *psir_field, *ownedNodes[k] );
        ind = 2*k;
        psi->ReplaceMyValues( 1, psirVal, &ind );

        // imaginary part
        double* psiiVal = stk::mesh::field_data( *psii_field, *ownedNodes[k] );
        ind = 2*k+1;
        psi->ReplaceMyValues( 1, psiiVal, &ind );
    }

    return psi;
}
// =============================================================================
Teuchos::RCP<Epetra_Vector>
Ginla::EpetraFVM::StkMeshReader::
createThickness_( const Teuchos::RCP<const Ginla::EpetraFVM::StkMesh> & mesh,
                  const Teuchos::RCP<ScalarFieldType>                 & thickness_field
                ) const
{
    // Get owned nodes.
    const std::vector<stk::mesh::Entity*> & ownedNodes = mesh->getOwnedNodes();

    // Create vector with this respective map.
    Teuchos::RCP<Epetra_Vector> thickness = Teuchos::rcp( new Epetra_Vector( *mesh->getNodesMap() ) );

    TEUCHOS_ASSERT( !thickness_field.is_null() );

    // Fill the vector with data from the file
    for ( int k=0; k<ownedNodes.size(); k++ )
    {
        double* thicknessVal = stk::mesh::field_data( *thickness_field, *ownedNodes[k] );
        // Check if the field is actually there.
        if (thicknessVal == NULL)
        {
            std::cerr << "WARNING: Thickness value for node " << k << " not found.\n"
                      << "Probably there is no thickness field given with the state. Using default."
                      << std::endl;
            return Teuchos::null;
        }
        thickness->ReplaceMyValues( 1, thicknessVal, &k );
    }

    return thickness;
}
// =============================================================================
Teuchos::RCP<Epetra_MultiVector>
Ginla::EpetraFVM::StkMeshReader::
createMvp_( const Teuchos::RCP<const Ginla::EpetraFVM::StkMesh> & mesh,
            const Teuchos::RCP<const ScalarFieldType>           & mvpXField,
            const Teuchos::RCP<const ScalarFieldType>           & mvpYField,
            const Teuchos::RCP<const ScalarFieldType>           & mvpZField
          ) const
{
    // Get owned nodes.
    const std::vector<stk::mesh::Entity*> & ownedNodes = mesh->getOwnedNodes();

    // Create vector with this respective map.
    int numComponents = 3;
    Teuchos::RCP<Epetra_MultiVector> mvp = Teuchos::rcp( new Epetra_MultiVector( *mesh->getNodesMap(), numComponents ) );

    TEUCHOS_ASSERT( !mvpXField.is_null() );
    TEUCHOS_ASSERT( !mvpYField.is_null() );
    TEUCHOS_ASSERT( !mvpZField.is_null() );

    // Fill the vector with data from the file
    for ( int k=0; k<ownedNodes.size(); k++ )
    {
        double* mvpXVal = stk::mesh::field_data( *mvpXField, *ownedNodes[k] );
        double* mvpYVal = stk::mesh::field_data( *mvpYField, *ownedNodes[k] );
        double* mvpZVal = stk::mesh::field_data( *mvpZField, *ownedNodes[k] );
        mvp->ReplaceMyValue( k, 0, *mvpXVal );
        mvp->ReplaceMyValue( k, 1, *mvpYVal );
        mvp->ReplaceMyValue( k, 2, *mvpZVal );
//        std::cout << mvpXVal[0] << " " << mvpYVal[0] << " " << mvpZVal[0] << std::endl;
    }

    return mvp;
}
// =============================================================================
//
// Helper functions
//
void
Ginla::EpetraFVM::
StkMeshRead ( const Epetra_Comm                       & comm,
              const std::string                       & fileName,
              Teuchos::RCP<Epetra_Vector>             & psi,
              Teuchos::RCP<Epetra_MultiVector>        & mvp,
              Teuchos::RCP<Epetra_Vector>             & thickness,
              Teuchos::RCP<Ginla::EpetraFVM::StkMesh> & mesh,
              Teuchos::ParameterList                  & parameterList
            )
{
    StkMeshReader reader( fileName );

    reader.read( comm, psi, mvp, thickness, mesh, parameterList );

    return;
}
// =============================================================================
