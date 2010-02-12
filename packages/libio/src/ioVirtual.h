#ifndef IOVIRTUAL_H
#define IOVIRTUAL_H

#include <Teuchos_ParameterList.hpp>

#include <Tpetra_MultiVector.hpp>

#include <Teuchos_Comm.hpp>
#include <Teuchos_Tuple.hpp>

#include<Epetra_MultiVector.h>

#include <Thyra_OperatorVectorTypes.hpp> // For Thyra::Ordinal

#include <boost/filesystem.hpp>

typedef Tpetra::Vector<double,Thyra::Ordinal> DoubleVector;
typedef Tpetra::MultiVector<double,Thyra::Ordinal> DoubleMultiVector;
typedef Tpetra::Vector<std::complex<double>,Thyra::Ordinal> ComplexVector;
typedef Tpetra::MultiVector<std::complex<double>,Thyra::Ordinal> ComplexMultiVector;

class IoVirtual
{
public:

    //! Default constructor.
    IoVirtual ( const boost::filesystem::path fileName );

    //! Destructor
    virtual ~IoVirtual();

    //! Virtual function for reading the order parameter \f$\psi\f$ and the
    //! parameter list from a given file.
    virtual void
    read ( const Teuchos::RCP<const Teuchos::Comm<int> > & tComm,
           Teuchos::RCP<DoubleMultiVector>               & x,
           Teuchos::ParameterList                        & problemParams
         ) const = 0; // pure virtual

    //! Virtual function for reading the order parameter \f$\psi\f$ and the
    //! parameter list from a given file.
    virtual void
    read ( const Teuchos::RCP<const Teuchos::Comm<int> > & tComm,
           Teuchos::RCP<ComplexMultiVector>        & x,
           Teuchos::ParameterList                  & problemParams
         ) const = 0; // pure virtual

    //! Virtual function for writing the order parameter \f$\psi\f$ and the
    //! parameter list to a given file.
    virtual void
    write ( const Epetra_MultiVector              & x,
            const Teuchos::Tuple<unsigned int,2>  & Nx,
            const Teuchos::Tuple<double,2>        & h,
            const Teuchos::Array<int>             & kBoundingBox,
            const Teuchos::ParameterList          & problemParams,
            const double                          & dummyValue
          ) = 0; // pure virtual
    
    virtual void
    write ( const Epetra_MultiVector                        & x,
            const Teuchos::Array<Teuchos::Tuple<double,2> > & loc,
            const Teuchos::ParameterList                    & problemParams
          ) = 0; // pure virtual
          
    virtual void
    write ( const Epetra_MultiVector             & x,
            const Teuchos::Tuple<unsigned int,2> & Nx,
            const Teuchos::Tuple<double,2>       & h,
            const Teuchos::ParameterList         & problemParams
          ) = 0; // pure virtual

    virtual void
    write ( const DoubleMultiVector              & x,
            const Teuchos::Tuple<unsigned int,2> & Nx,
            const Teuchos::Tuple<double,2>       & h,
            const Teuchos::ParameterList         & problemParams
          ) = 0; // pure virtual

    virtual void
    write ( const ComplexMultiVector             & x,
            const Teuchos::Tuple<unsigned int,2> & Nx,
            const Teuchos::Tuple<double,2>       & h,
            const Teuchos::ParameterList         & problemParams
          ) = 0; // pure virtual
          
    virtual void
    write ( const ComplexMultiVector              & x,
            const Teuchos::Tuple<unsigned int,2>  & Nx,
            const Teuchos::Tuple<double,2>        & h,
            const Teuchos::Array<int>             & kBoundingBox,
            const Teuchos::ParameterList          & problemParams,
            const double                          & dummyValue
          ) = 0; // pure virtual

    virtual void
    write ( const DoubleMultiVector              & x,
            const Teuchos::Tuple<unsigned int,2> & Nx,
            const Teuchos::Tuple<double,2>       & h
          ) = 0; // pure virtual

    virtual void
    write ( const ComplexMultiVector             & x,
            const Teuchos::Tuple<unsigned int,2> & Nx,
            const Teuchos::Tuple<double,2>       & h
          ) = 0; // pure virtual

protected:
    //! File name for the I/O.
    boost::filesystem::path fileName_;

};
#endif // IOVIRTUAL_H
