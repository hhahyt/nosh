/*
 * GridUniformSquare.h
 *
 *  Created on: Nov 30, 2009
 *      Author: Nico Schl�mer
 */

#ifndef GRIDUNIFORMSQUARE_H_
#define GRIDUNIFORMSQUARE_H_

#include "GridUniformVirtual.h"
#include "GridSquare.h"

class GridUniformSquare: public GridUniformVirtual, public GridSquare
{

public:

  //! Default constructor.
  GridUniformSquare( unsigned int nx = 0,
                     double       scaling = 1.0   );

  virtual
  ~GridUniformSquare();

  virtual void
  read( const Teuchos::RCP<const Teuchos::Comm<int> > & Comm,
        const std::string                             & filePath,
        Teuchos::RCP<DoubleMultiVector>               & x,
        Teuchos::ParameterList                        & params
      );

protected:
private:
};

#endif /* GRIDUNIFORMSQUARE_H_ */
