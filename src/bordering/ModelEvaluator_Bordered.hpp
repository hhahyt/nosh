// @HEADER
//
//    Nosh bordered model evaluator.
//    Copyright (C) 2012  Nico Schlömer
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
#ifndef NOSH_MODELEVALUATOR_BORDERED_H
#define NOSH_MODELEVALUATOR_BORDERED_H
// -----------------------------------------------------------------------------
// includes
#include <string>

#include <Teuchos_RCP.hpp>

#include "ModelEvaluator_Virtual.hpp"
// -----------------------------------------------------------------------------
namespace Nosh
{
namespace ModelEvaluator
{
class Bordered : public Nosh::ModelEvaluator::Virtual
{
public:
  //! Constructor without initial guess.
  Bordered (
      const std::shared_ptr<const Nosh::ModelEvaluator::Virtual> & modelEval,
      const std::shared_ptr<const Tpetra::Vector<double,int,int>> & initialBordering,
      const double lambdaInit
      );

  // Destructor
  virtual
  ~Bordered();

  virtual
  Teuchos::RCP<const Tpetra::Map<int,int>>
  get_x_map() const;

  virtual
  Teuchos::RCP<const Tpetra::Map<int,int>>
  get_f_map() const;

  virtual
  Teuchos::RCP<const Tpetra::Vector<double,int,int>>
  get_x_init() const;

  virtual
  Teuchos::RCP<const Tpetra::Vector<double,int,int>>
  get_p_init(int l) const;

  virtual
  Teuchos::RCP<const Tpetra::Map<int,int>>
  get_p_map(int l) const;

  virtual
  Teuchos::RCP<const Teuchos::Array<std::string> >
  get_p_names(int l) const;

  virtual
  Teuchos::RCP<Tpetra::Operator<double,int,int>>
  create_W() const;

  virtual
  Teuchos::RCP<EpetraExt::ModelEvaluator::Preconditioner>
  create_WPrec() const;

  virtual
  InArgs
  createInArgs() const;

  virtual
  OutArgs
  createOutArgs() const;

  virtual
  void
  evalModel(
      const InArgs &inArgs,
      const OutArgs &outArgs
      ) const;

public:
  virtual
  double
  innerProduct(const Tpetra::Vector<double,int,int> &phi,
               const Tpetra::Vector<double,int,int> &psi
             ) const;

  virtual
  double
  gibbsEnergy(const Tpetra::Vector<double,int,int> &psi) const;

  virtual
  const std::shared_ptr<const Nosh::Mesh>
  getMesh() const;

protected:
private:
  const std::shared_ptr<const Nosh::ModelEvaluator::Virtual> innerModelEval_;
  const std::shared_ptr<const Tpetra::Vector<double,int,int>> initialBordering_;
  const double lambdaInit_;
};
} // namespace ModelEvaluator
} // namespace Nosh

#endif // NOSH_MODELEVALUATOR_BORDERED_H