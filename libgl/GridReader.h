/*
 * GridReader.h
 *
 *  Created on: Nov 27, 2009
 *      Author: Nico Schl�mer
 */

#ifndef GRIDREADER_H_
#define GRIDREADER_H_

#include "GridUniformVirtual.h"
#include <Tpetra_Vector.hpp>

namespace GridReader
{
  void
  read( const Teuchos::RCP<const Teuchos::Comm<int> > & Comm,
        const std::string                             & filePath,
              Teuchos::RCP<DoubleMultiVector>         & x,
              Teuchos::RCP<GridUniformVirtual>        & grid,
              Teuchos::ParameterList                  & params
      );

  void
  read( const Teuchos::RCP<const Teuchos::Comm<int> > & Comm,
        const std::string                             & filePath,
              Teuchos::RCP<ComplexMultiVector>        & x,
              Teuchos::RCP<GridUniformVirtual>        & grid,
              Teuchos::ParameterList                  & params
      );
};

#endif /* GRIDREADER_H_ */
