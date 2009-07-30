#include "ioXdmf.h"

#include <Teuchos_XMLParameterListWriter.hpp>
#include <EpetraExt_Utils.h> // for toString

#include <Epetra_MultiVector.h>
#include <EpetraExt_HDF5.h>

#include <Teuchos_StringInputSource.hpp>
#include <Teuchos_XMLParameterListReader.hpp>

#include <iostream>
#include <fstream>

#ifdef HAVE_MPI
#include <mpi.h>
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

// =============================================================================
// Constructor
IoXdmf::IoXdmf()
{
}
// =============================================================================



// =============================================================================
// Destructor
IoXdmf::~IoXdmf()
{
}
// =============================================================================



// =============================================================================
void IoXdmf::read( const std::string           &fileName,
                   std::vector<double_complex> *psi,
                   Teuchos::ParameterList      *problemParams )
{

  // Convert the file to a string, such that we can discard the headers and pass
  // the pure XML stuff to Teuchos.
  // This is a workaround.
  // TODO: Follow the Trilinos bug at
  //         https://software.sandia.gov/bugzilla/show_bug.cgi?id=4516
  //       and see what happens.

  // extract the directory for later
  string directory = fileName;
  directory.erase( directory.rfind("/") );


  // read the file contents to a string
  std::ifstream inFile( fileName.c_str() );
  if( !inFile ) {
      std::cerr << "IoXdmf::read\n"
                << "    Could not open input file \"" << fileName << "\". Abort."
                << std::endl;
      exit(EXIT_FAILURE);
  }

  // Read the file into a string, and discard exactly the first for lines,
  // which read
  //
  // ===================== *snip* =====================
  // <?xml version="1.0" ?>
  // <!DOCTYPE Xdmf SYSTEM "Xdmf.dtd" [
  // <!ENTITY HeavyData "solution.h5">
  // ]>
  // ===================== *snap* =====================
  //
  string buf, tmp;
  int row = 1;
  while(!inFile.eof()) {
      getline(inFile, tmp);
      if (row>=5) {
          buf += tmp;
          buf += "\n";
      }
      row++;
  }

  // pass string as XML input source
  Teuchos::StringInputSource xmlString(buf);

  // Extract the object from the filename.
  Teuchos::XMLObject xmlFileObject = xmlString.getObject();

  // find and read the parameter list
  const Teuchos::XMLObject* parameterListObject =
                                     xmlFind( &xmlFileObject, "ParameterList" );
  *problemParams = Teuchos::XMLParameterListReader().
                                        toParameterList( *parameterListObject );

  // create a dummy communicator
  // TODO: Get rid of this.
#ifdef HAVE_MPI
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif

  // create MultiVectors that will store the abs and arg data
  int Nx = problemParams->get<int>("Nx");
  Epetra_Map         StandardMap( Nx+1, 0, Comm );
  Epetra_MultiVector* absPsi = new Epetra_MultiVector(StandardMap,Nx+1);
  Epetra_MultiVector* argPsi = new Epetra_MultiVector(StandardMap,Nx+1);

  // gather the heavy abs(psi) data
  getHeavyData( xmlFileObject, Comm, &absPsi, directory, "abs(psi)", "abs" );

  // gather the heavy abs(psi) data
  getHeavyData( xmlFileObject, Comm, &argPsi, directory, "arg(psi)", "arg" );

  // build psi of the entries that we got
  psi->resize( (Nx+1)*(Nx+1) );
  int k = 0;
  for (int i=0; i<Nx+1; i++)
      for (int j=0; j<Nx+1; j++)
          (*psi)[k++] = polar2complex( (*absPsi)[i][j], (*argPsi)[i][j] );

  return;
}
// =============================================================================



// =============================================================================
// Construct a complex number out of its absolute value and its angle.
inline double_complex IoXdmf::polar2complex( double abs,
                                             double arg  )
{
  return double_complex( abs*cos(arg), abs*sin(arg)  );
}
// =============================================================================



// =============================================================================
void IoXdmf::getHeavyData( const Teuchos::XMLObject &xmlFileObject,
                           const Epetra_Comm        &comm,
                           Epetra_MultiVector       **readVec,
                           const std::string        &fileDirectory,
                           const std::string        &xmlName,
                           const std::string        &hdf5GroupName )
{
  // ---------------------------------------------------------------------------
  // now go find where the heavy data is stored

  // find the abs(psi) data
  const Teuchos::XMLObject* absPsiObject = xmlAttributeFind ( &xmlFileObject,
                                                              "Attribute",
                                                              "Name",
                                                              xmlName         );

  // Make sure that is contains something of the form
  //
  // ===================== *snip* =====================
  //   <DataItem Dimensions="1 51 51" Format="HDF" NumberType="Float" Precision="4">
  //   solution.h5:/abs/Values
  //   </DataItem>
  // ===================== *snap* =====================
  //
  const Teuchos::XMLObject* dataItem = xmlFind ( absPsiObject,
                                                 "DataItem" );
  if ( !dataItem ) {
      std::cerr << "Found no \"DataItem\". Abort." << std::endl;
      exit(EXIT_FAILURE);
  }

  // get the file name
// TODO: numContentLines currently broken; don't use it until further notice.
//   int cLines=dataItem->numContentLines();
//   if ( cLines<1 ) {
//       std::cerr << "Found no file name in \"DataItem\". Abort." << std::endl;
//       exit(EXIT_FAILURE);
//   }

  // Read the first line which is supposed to contain the file and more
  // (see above).
  string dataLine;

  // getContentLine actually get content columns, so build up dataLine like
  // that
  for (int k=0; k<dataItem->numContentLines(); k++ )
      dataLine += dataItem->getContentLine(k);

  // trim whitespace
  std::string whiteSpaceCharacters = " \n\t";
  dataLine.erase( dataLine.find_last_not_of(whiteSpaceCharacters) + 1);  // from the right
  dataLine.erase( 0, dataLine.find_first_not_of(whiteSpaceCharacters) ); // from the left

  // extract file name
  int    colonPos  = dataLine.find(":");
  string dataFile  = dataLine.substr(0,colonPos);
  string dataGroup = dataLine.substr(colonPos+1,dataLine.size()-colonPos);

  EpetraExt::HDF5 hdf5Reader(comm);
  hdf5Reader.Open( fileDirectory+"/"+dataFile ); // directory from fileName
  if ( !hdf5Reader.IsContained(hdf5GroupName) ) {
      std::cerr << "Could not find tag \"" << hdf5GroupName << "\" in file "
                << fileDirectory+"/"+dataFile << ". Abort." << std::endl;
      exit(EXIT_FAILURE);
  }

  // get data sizes and compare with the input MultiVector
  int globalLength, numVectors;
  hdf5Reader.ReadMultiVectorProperties( hdf5GroupName,
                                        globalLength,
                                        numVectors     );

  if ( globalLength != (*readVec)->GlobalLength() ) {
      std::cerr << "Length of the input MultiVector (" << (*readVec)->GlobalLength()
                << ") does not coincide with the global length of the data in "
                << "file " << dataFile << ". Abort." << std::endl;
      exit(EXIT_FAILURE);
  }

  if ( numVectors != (*readVec)->NumVectors() ) {
      std::cerr << "Number of the input MultiVectors (" << (*readVec)->NumVectors()
                << ") does not coincide with the global number of vectors in "
                << "file " << dataFile << ". Abort." << std::endl;
      exit(EXIT_FAILURE);
  }

  // finally, read the vector
  hdf5Reader.Read( hdf5GroupName, *readVec );

  hdf5Reader.Close();
}
// =============================================================================



// =============================================================================
void IoXdmf::write( const std::string                 &fileName,
                    const std::vector<double_complex> &psi,
                    const Teuchos::ParameterList      &problemParams,
                    StaggeredGrid::StaggeredGrid      &sGrid          )
{

  int    Nx = sGrid.getNx(),
         k,
         index[2];
  double h  = sGrid.getH();
  std::string   str;
  std::ofstream xdmfFile;

  // create the HDF5 file name, take off the suffix and replace it by .h5
  int dotPos = fileName.rfind(".");
  std::string hdf5FileName = fileName;
  hdf5FileName.replace(dotPos,fileName.size()-dotPos ,".h5");
  // strip off the directory names to get the base name
  dotPos = fileName.rfind("/");
  std::string hdf5BaseName = hdf5FileName;
  hdf5BaseName.erase(0,dotPos+1);

  // open the file
  xdmfFile.open( fileName.c_str() );

  // write the XDMF header
  xdmfFile << "<?xml version=\"1.0\" ?>\n"
           << "<!DOCTYPE Xdmf SYSTEM \"Xdmf.dtd\" [\n"
           << "<!ENTITY HeavyData \"" << hdf5BaseName << "\">\n"
           << "]>\n"
           << std::endl;

  // enclosing structure
  Teuchos::XMLObject xdmfContainer("Xdmf");

  // Append the problem parameters in XML form.
  // This needs to go *inside* the enclosing Xdmf tag, otherwise the file format
  // is not universally recognized.
  Teuchos::XMLObject xmlParameterList("");
  xmlParameterList = Teuchos::XMLParameterListWriter::XMLParameterListWriter()
                                                        .toXML( problemParams );
  xdmfContainer.addChild(xmlParameterList);

  // add the domain item
  Teuchos::XMLObject xmlDomain("Domain");
  xdmfContainer.addChild( xmlDomain );

  // add the grid item
  Teuchos::XMLObject xmlGrid("Grid");
  xmlGrid.addAttribute( "Name", "MyGrid" );
  xmlDomain.addChild( xmlGrid );

  // add topology
  Teuchos::XMLObject xmlTopology("Topology");
  xmlTopology.addAttribute( "TopologyType", "3DCORECTMESH" );
  str = "1 " + EpetraExt::toString(Nx+1) + " " + EpetraExt::toString(Nx+1);
  xmlTopology.addAttribute( "Dimensions", str );
  xmlGrid.addChild( xmlTopology );

  // merge the latter two into geometry
  Teuchos::XMLObject xmlGeometry("Geometry");
  xmlGeometry.addAttribute( "Type", "ORIGIN_DXDYDZ" );
  xmlGrid.addChild( xmlGeometry );

  // add actual geometry information to the geometry item
  // define origin
  Teuchos::XMLObject xmlOrigin("DataItem");
  xmlOrigin.addAttribute( "Name", "Origin" );
  xmlOrigin.addAttribute( "NumberType", "Float" );
  xmlOrigin.addAttribute( "Dimensions", "3" );
  xmlOrigin.addAttribute( "Format", "XML" );
  xmlOrigin.addContent( "0 0 0" );
  xmlGeometry.addChild( xmlOrigin );

  // define spacing
  Teuchos::XMLObject xmlSpacing("DataItem");
  xmlSpacing.addAttribute( "Name", "Spacing" );
  xmlSpacing.addAttribute( "NumberType", "Float" );
  xmlSpacing.addAttribute( "Dimensions", "3" );
  xmlSpacing.addAttribute( "Format", "XML" );
  str = "0 " + EpetraExt::toString(h) + " " + EpetraExt::toString(h);
  xmlSpacing.addContent( str );
  xmlGeometry.addChild( xmlSpacing );

  // tell me where the actual ABS(PSI) data sits
  Teuchos::XMLObject xmlAbs("Attribute");
  xmlAbs.addAttribute( "Active", "1" );
  xmlAbs.addAttribute( "AttributeType", "Scalar" );
  xmlAbs.addAttribute( "Center", "Node" );
  xmlAbs.addAttribute( "Name", "abs(psi)" );
  xmlGrid.addChild( xmlAbs );

  Teuchos::XMLObject xmlAbsData("DataItem");
  xmlAbsData.addAttribute( "NumberType", "Float" );
  xmlAbsData.addAttribute( "Precision", "4" );
  str = "1 " + EpetraExt::toString(Nx+1) + " " + EpetraExt::toString(Nx+1);
  xmlAbsData.addAttribute( "Dimensions", str );
  xmlAbsData.addAttribute( "Format", "HDF" );
  xmlAbsData.addContent( hdf5BaseName+":/abs/Values" );
  xmlAbs.addChild( xmlAbsData );


  // tell me where the actual ARG(PSI) data sits
  Teuchos::XMLObject xmlArg("Attribute");
  xmlArg.addAttribute( "Center", "Node" );
  xmlArg.addAttribute( "AttributeType", "Scalar" );
  xmlArg.addAttribute( "Name", "arg(psi)" );
  xmlGrid.addChild( xmlArg );

  Teuchos::XMLObject xmlArgData("DataItem");
  xmlArgData.addAttribute( "NumberType", "Float" );
  xmlArgData.addAttribute( "Precision", "4" );
  str = "1 " + EpetraExt::toString(Nx+1) + " " + EpetraExt::toString(Nx+1);
  xmlArgData.addAttribute( "Dimensions", str );
  xmlArgData.addAttribute( "Format", "HDF" );
  xmlArgData.addContent( hdf5BaseName+":/arg/Values" );
  xmlArg.addChild( xmlArgData );

  // write it all to the file
  xdmfFile << xdmfContainer;

  // close the file
  xdmfFile.close();
  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // write the HDF5 heavy data file
  // ---------------------------------------------------------------------------
  // create a vector with abs values
#ifdef HAVE_MPI
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif
//   int NumUnknowns = (SGrid.getNx()+1)*(SGrid.getNx()+1);
  Epetra_Map StandardMap( Nx+1,0,Comm);

  // fill absPsi and argPsi
  Epetra_MultiVector absPsi(StandardMap,Nx+1);
  Epetra_MultiVector argPsi(StandardMap,Nx+1);
  for (int i=0; i<Nx+1; i++) {
      index[0] = i;
      for (int j=0; j<Nx+1; j++) {
          index[1] = j;
          k = sGrid.i2k( index );
          absPsi.ReplaceGlobalValue( i, j, abs(psi[k]) );
          argPsi.ReplaceGlobalValue( i, j, arg(psi[k]) );
      }
  }

  EpetraExt::HDF5 myhdf5( Comm );
  myhdf5.Create( hdf5FileName );
  myhdf5.Write( "abs", absPsi );
  myhdf5.Write( "arg", argPsi );
  myhdf5.Close();
  // ---------------------------------------------------------------------------

}
// =============================================================================



// =============================================================================
// Inside an XML object, this function looks for a specific tag and returns
// a pointer to it.
const Teuchos::XMLObject* IoXdmf::xmlFind ( const Teuchos::XMLObject *xmlObj,
                                            const std::string        tag )
{
  const Teuchos::XMLObject* xmlOut=NULL;

  if ( !xmlObj->getTag().compare(tag) ) // strings are equal
      xmlOut = xmlObj;
  else
      for (int k=0; k<xmlObj->numChildren(); k++) {
          xmlOut = xmlFind ( &(xmlObj->getChild(k)), tag ); // recursive call
          if (xmlOut) break; // not the null pointer => return
      }

  return xmlOut;
}
// =============================================================================




// =============================================================================
const Teuchos::XMLObject* IoXdmf::xmlAttributeFind ( const Teuchos::XMLObject *xmlObj,
                                                     const std::string        tag,
                                                     const std::string        attribute,
                                                     const std::string        value      )
{
  const Teuchos::XMLObject* xmlOut=NULL;

// std::cout << "Looking for: " << tag << " " << attribute << " " <<  value << std::endl;
// std::cout << "Found:       " << xmlObj->getTag() << " " << xmlObj->hasAttribute(attribute) << std::endl;
// if ( xmlObj->hasAttribute(attribute) )
//      std::cout << "1" << std::endl;
//      std::cout << xmlObj->getAttribute(attribute) << std::endl;

  if (    !xmlObj->getTag().compare(tag)
       && xmlObj->hasAttribute(attribute)
       && xmlObj->getAttribute(attribute).compare(value) )
      xmlOut = xmlObj;
  else
      for (int k=0; k<xmlObj->numChildren(); k++) {
          xmlOut = xmlAttributeFind ( &(xmlObj->getChild(k)), // recursive call
                                      tag,
                                      attribute,
                                      value                    );
          if (xmlOut) break; // not the null pointer => return
      }

  return xmlOut;
}
// =============================================================================