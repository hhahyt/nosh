/*
 * GridVirtual.h
 *
 *  Created on: Nov 25, 2009
 *      Author: Nico Schl�mer
 */

#ifndef GRIDVIRTUAL_H_
#define GRIDVIRTUAL_H_

#include <Teuchos_RCP.hpp>
#include <Teuchos_Array.hpp>
#include <Teuchos_Tuple.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Tpetra_MultiVector.hpp>

typedef Tpetra::MultiVector<double> DoubleMultiVector;

class GridVirtual
{
public:
    GridVirtual( double                   scaling = 0.0,
                 Teuchos::Tuple<double,2> h = Teuchos::tuple(0.0,0.0),
                 double                   gridDomainArea = 0.0,
                 unsigned int             numGridPoints = 0,
                 unsigned int             numBoundaryPoints = 0
               );

    virtual
    ~GridVirtual();

    double
    getScaling() const; //!< Returns the scaling factor \f$alpha\f$ of the domain.

    void
    setScaling( const double alpha );

    unsigned int
    getNumGridPoints() const; //!< Returns the number of grid points.

    //! Returns the number of grid points on the boundary.
    unsigned int
    getNumBoundaryPoints() const;

    Teuchos::Tuple<double,2>
    getH() const; //!< Returns mesh sizes \f$h\f$.

    //! Returns the measure of the discretized domain.
    double
    getGridDomainArea() const;

public:
    /*! Indicates whether a node sits in a corner of the domain, on an edge,
     or strictly inside it. */
    enum nodeType
    {
      BOTTOMLEFTCONVEX,
      BOTTOMLEFTCONCAVE,
      BOTTOMRIGHTCONVEX,
      BOTTOMRIGHTCONCAVE,
      TOPLEFTCONVEX,
      TOPLEFTCONCAVE,
      TOPRIGHTCONVEX,
      TOPRIGHTCONCAVE,
      BOTTOM,
      RIGHT,
      TOP,
      LEFT,
      INTERIOR
    };

public:

    virtual nodeType
    getBoundaryNodeType( unsigned int l ) const = 0;

    /*! For a given node number k, returns the the area of the surrounding cell. */
    virtual double
    cellArea(unsigned int k) const = 0;

    virtual Teuchos::RCP<Teuchos::Array<double> >
    getXLeft(unsigned int k) const = 0; //!< Returns the value of \f$x\f$ left of point i.

    virtual Teuchos::RCP<Teuchos::Array<double> >
    getXRight(unsigned int k) const = 0; //!< Returns the value of \f$x\f$ right of point i.

    virtual Teuchos::RCP<Teuchos::Array<double> >
    getXBelow(unsigned int k) const = 0 ; //!< Returns the value of \f$x\f$ below point i.

    virtual Teuchos::RCP<Teuchos::Array<double> >
    getXAbove(unsigned int k) const = 0; //!< Returns the value of \f$x\f$ above point i.

    virtual unsigned int
    getKLeft(unsigned int k) const = 0; //!< Returns the running index \c k of the node left of \c i.

    virtual unsigned int
    getKRight(unsigned int k) const = 0; //!< Returns the running index \c k of the node right of \c i.

    virtual unsigned int
    getKBelow(unsigned int k) const = 0; //!< Returns the running index \c k of the node below \c i.

    virtual unsigned int
    getKAbove(unsigned int k) const = 0; //!< Returns the running index \c k of the node above \c i.

    //! Returns the global index \ck of the \cl-th boundary node. Subsequent nodes \c l, \cl+1
    //! sit next to each other.
    virtual unsigned int
    boundaryIndex2globalIndex( unsigned int l ) const = 0;

    virtual void
    writeWithGrid( const DoubleMultiVector      & x,
                   const Teuchos::ParameterList & params,
                   const std::string            & filePath
                 ) const = 0;

    virtual void
    read( const Teuchos::RCP<const Teuchos::Comm<int> > & Comm,
          const std::string                             & filePath,
          Teuchos::RCP<DoubleMultiVector>               & x,
          Teuchos::ParameterList                        & params
        ) = 0;

  protected:
    double scaling_; //! scaling factor
    Teuchos::Tuple<double,2> h_;
    double gridDomainArea_;
    unsigned int numGridPoints_;
    unsigned int numBoundaryPoints_;

  private:
  };

#endif /* GRIDVIRTUAL_H_ */
