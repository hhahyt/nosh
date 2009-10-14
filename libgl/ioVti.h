#ifndef IOVTI_H
#define IOVTI_H

#include "ioVirtual.h"

#include <Teuchos_XMLObject.hpp>

#include <Tpetra_MultiVector.hpp>

class IoVti: public IoVirtual
{
  public:

     //! Default constructor.
     IoVti( std::string fname );

     //! Destructor
     virtual ~IoVti();

     //! Reads the order parameter \f$\psi\f$ and the problem parameter list
     //! from a VTI file into the arguments.
     virtual void read( const Teuchos::RCP<Tpetra::MultiVector<double_complex,int> > psi,
			Teuchos::RCP<Teuchos::Comm<int> >                            comm, // TODO: remove this
                        Teuchos::ParameterList                                       *problemParams );

     //! Writes the  order parameter \f$\psi\f$ and the problem parameter list
     //! into an XML-style VTI file.
     //! The data is written such that the lexicographical ordering of the
     //! nodes is preserved and the resulting file contains a state \f$\psi\f
     //! that can be viewed using standard tools.
     virtual void write( const Tpetra::MultiVector<double_complex,int> &psi,
                         const Teuchos::ParameterList                  &problemParams,
                         StaggeredGrid                                 &sGrid          );

  private:

     const Teuchos::XMLObject* xmlFind ( const Teuchos::XMLObject *xmlObj,
                                         const std::string        tag );

     const Teuchos::XMLObject* xmlAttributeFind ( const Teuchos::XMLObject *xmlObj,
                                                  const std::string        tag,
                                                  const std::string        attribute,
                                                  const std::string        value      );

};
#endif // IOVTI_H