/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2010 Nico Schl\"omer

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

#include "Ginla_Operator_BCOuter.h"

#include <Teuchos_Array.hpp>

#include "Recti_Grid_Uniform.h"
#include "Ginla_MagneticVectorPotential_Centered.h"

// complex unit
const double_complex I ( 0,1 );

// =============================================================================
Ginla::Operator::BCOuter::
BCOuter ( Teuchos::RCP<Recti::Grid::Uniform>                  & grid,
          Teuchos::RCP<Ginla::MagneticVectorPotential::Centered> & A
        ) :
        Ginla::Operator::Virtual ( grid, A )
{
}
// =============================================================================
Ginla::Operator::BCOuter::~BCOuter()
{
}
// =============================================================================
double_complex
Ginla::Operator::BCOuter::getEntry ( const int k ) const
{
    double_complex res;
    double_complex psiK, psiKRight, psiKLeft, psiKAbove, psiKBelow;
    double ARight, ALeft, AAbove, ABelow;

    double h = grid_->getUniformH();

    Teuchos::RCP<DoubleTuple> xRight = Teuchos::rcp ( new DoubleTuple() );
    Teuchos::RCP<DoubleTuple> xLeft  = Teuchos::rcp ( new DoubleTuple() );
    Teuchos::RCP<DoubleTuple> xAbove = Teuchos::rcp ( new DoubleTuple() );
    Teuchos::RCP<DoubleTuple> xBelow = Teuchos::rcp ( new DoubleTuple() );

    Recti::Grid::Abstract::nodeType nt = grid_->getNodeType ( k );

    // Get a view of the whole vector.
    // Remember: This only works with one core.
    Teuchos::ArrayRCP<const double_complex> psiView = psi_->get1dView();

    psiK = psiView[k];

    switch ( nt )
    {
    case Recti::Grid::Abstract::INTERIOR:
        // TODO Gets the local index. ==> Only works on one core.
        psiKLeft  = psiView[ grid_->getKLeft ( k ) ];
        psiKRight = psiView[ grid_->getKRight ( k ) ];
        psiKBelow = psiView[ grid_->getKBelow ( k ) ];
        psiKAbove = psiView[ grid_->getKAbove ( k ) ];

        xLeft  = grid_->getXLeft ( k );
        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );
        AAbove = A_->getAy ( *xAbove );

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
        res = ( psiK* ( -4.0 )
                + psiKLeft*  exp ( I*ALeft *h ) + psiKRight* exp ( -I*ARight*h )
                + psiKBelow* exp ( I*ABelow*h ) + psiKAbove* exp ( -I*AAbove*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );

        break;

    case Recti::Grid::Abstract::BOUNDARY_BOTTOMLEFTCONVEX:
        // -------------------------------------------------------------------
        psiKRight = psiView[ grid_->getKRight ( k ) ];
        psiKAbove = psiView[ grid_->getKAbove ( k ) ];

        xRight = grid_->getXRight ( k );
        xAbove = grid_->getXAbove ( k );

        ARight = A_->getAx ( *xRight );
        AAbove = A_->getAy ( *xAbove );

        res = ( - psiK      * 2.0
                + psiKRight * exp ( -I*ARight*h )
                + psiKAbove * exp ( -I*AAbove*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_BOTTOMRIGHTCONVEX:
        // -----------------------------------------------------------------------
        psiKLeft  = psiView[ grid_->getKLeft ( k ) ];
        psiKAbove = psiView[ grid_->getKAbove ( k ) ];

        xLeft  = grid_->getXLeft ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        AAbove = A_->getAy ( *xAbove );

        res = ( psiK* ( -2.0 )
                + psiKLeft * exp ( I*ALeft *h )
                + psiKAbove* exp ( -I*AAbove*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -----------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_TOPRIGHTCONVEX:
        // -----------------------------------------------------------------------
        psiKLeft  = psiView[ grid_->getKLeft ( k ) ];
        psiKBelow = psiView[ grid_->getKBelow ( k ) ];

        xLeft  = grid_->getXLeft ( k );
        xBelow = grid_->getXBelow ( k );

        ALeft  = A_->getAx ( *xLeft );
        ABelow = A_->getAy ( *xBelow );

        res = ( psiK* ( -2.0 )
                + psiKLeft * exp ( I*ALeft *h )
                + psiKBelow* exp ( I*ABelow*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -----------------------------------------------------------------------

        break;

    case Recti::Grid::Abstract::BOUNDARY_TOPLEFTCONVEX:
        // -----------------------------------------------------------------------
        psiKRight = psiView[ grid_->getKRight ( k ) ];
        psiKBelow = psiView[ grid_->getKBelow ( k ) ];

        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );

        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );

        res = ( psiK* ( -2.0 )
                + psiKRight* exp ( -I*ARight*h )
                + psiKBelow* exp ( I*ABelow*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -----------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_BOTTOM:
        // -------------------------------------------------------------------
        psiKLeft  = psiView[ grid_->getKLeft ( k ) ];
        psiKRight = psiView[ grid_->getKRight ( k ) ];
        psiKAbove = psiView[ grid_->getKAbove ( k ) ];

        xLeft  = grid_->getXLeft ( k );
        xRight = grid_->getXRight ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        ARight = A_->getAx ( *xRight );
        AAbove = A_->getAy ( *xAbove );

        res = ( psiK* ( -3.0 )
                + psiKLeft*  exp ( I*ALeft *h ) + psiKRight* exp ( -I*ARight*h )
                + psiKAbove* exp ( -I*AAbove*h ) ) / ( h*h )
              + psiK * ( 1.0-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_RIGHT:
        // -------------------------------------------------------------------
        psiKLeft  = psiView[ grid_->getKLeft ( k ) ];
        psiKBelow = psiView[ grid_->getKBelow ( k ) ];
        psiKAbove = psiView[ grid_->getKAbove ( k ) ];

        xLeft  = grid_->getXLeft ( k );
        xBelow = grid_->getXBelow ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        ABelow = A_->getAy ( *xBelow );
        AAbove = A_->getAy ( *xAbove );

        res = ( psiK* ( -3.0 )
                + psiKLeft*  exp ( I*ALeft *h )
                + psiKBelow* exp ( I*ABelow*h ) + psiKAbove* exp ( -I*AAbove*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_TOP:
        // -------------------------------------------------------------------
        psiKLeft  = psiView[ grid_->getKLeft ( k ) ];
        psiKRight = psiView[ grid_->getKRight ( k ) ];
        psiKBelow = psiView[ grid_->getKBelow ( k ) ];

        xLeft  = grid_->getXLeft ( k );
        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );

        ALeft  = A_->getAx ( *xLeft );
        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );

        res = ( psiK* ( -3.0 )
                + psiKLeft*  exp ( I*ALeft *h ) + psiKRight* exp ( -I*ARight*h )
                + psiKBelow* exp ( I*ABelow*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_LEFT:
        // -------------------------------------------------------------------
        psiKRight = psiView[ grid_->getKRight ( k ) ];
        psiKBelow = psiView[ grid_->getKBelow ( k ) ];
        psiKAbove = psiView[ grid_->getKAbove ( k ) ];

        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );
        xAbove = grid_->getXAbove ( k );

        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );
        AAbove = A_->getAy ( *xAbove );

        res = ( psiK* ( -3.0 )
                + psiKRight* exp ( -I*ARight*h )
                + psiKBelow* exp ( I*ABelow*h ) + psiKAbove* exp ( -I*AAbove*h ) ) / ( h*h )
              + psiK * ( 1-norm ( psiK ) );
        res *= exp ( I*chi_ );
        // -------------------------------------------------------------------
        break;

    default:
        TEST_FOR_EXCEPTION ( true,
                             std::logic_error,
                             "Illegal node type \"" << nt << "\"." );
    }

    // return the result
    return res;
}
// =============================================================================
void
Ginla::Operator::BCOuter::getJacobianRow ( const int                        k,
                                    Teuchos::Array<int>            & columnIndicesPsi,
                                    Teuchos::Array<double_complex> & valuesPsi,
                                    Teuchos::Array<int>            & columnIndicesPsiConj,
                                    Teuchos::Array<double_complex> & valuesPsiConj
                                  ) const
{
    int kLeft, kRight, kBelow, kAbove;
    int numEntriesPsi, numEntriesPsiConj;
    double ARight, ALeft, AAbove, ABelow;

    double h = grid_->getUniformH();

    Teuchos::RCP<DoubleTuple> xRight = Teuchos::rcp ( new DoubleTuple() );
    Teuchos::RCP<DoubleTuple> xLeft  = Teuchos::rcp ( new DoubleTuple() );
    Teuchos::RCP<DoubleTuple> xAbove = Teuchos::rcp ( new DoubleTuple() );
    Teuchos::RCP<DoubleTuple> xBelow = Teuchos::rcp ( new DoubleTuple() );

    Teuchos::ArrayRCP<const double_complex> psiView = psi_->get1dView();

    Recti::Grid::Abstract::nodeType nt = grid_->getNodeType ( k );
    switch ( nt )
    {
    case Recti::Grid::Abstract::INTERIOR:
        // ---------------------------------------------------------------------
        kRight = grid_->getKRight ( k );
        kLeft  = grid_->getKLeft ( k );
        kAbove = grid_->getKAbove ( k );
        kBelow = grid_->getKBelow ( k );

        numEntriesPsi = 5;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kLeft;
        columnIndicesPsi[2] = kRight;
        columnIndicesPsi[3] = kBelow;
        columnIndicesPsi[4] = kAbove;

        xLeft  = grid_->getXLeft ( k );
        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );
        xAbove = grid_->getXAbove ( k );
        ALeft  = A_->getAx ( *xLeft );
        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );
        AAbove = A_->getAy ( *xAbove );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = - 4.0            / ( h*h )
                       + ( 1 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ALeft *h ) / ( h*h );
        valuesPsi[2] = exp ( -I*ARight*h ) / ( h*h );
        valuesPsi[3] = exp ( I*ABelow*h ) / ( h*h );
        valuesPsi[4] = exp ( -I*AAbove*h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];

        break;
    case Recti::Grid::Abstract::BOUNDARY_BOTTOMLEFTCONVEX:
        // -------------------------------------------------------------------
        kRight = grid_->getKRight ( k );
        kAbove = grid_->getKAbove ( k );

        numEntriesPsi = 3;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kRight;
        columnIndicesPsi[2] = kAbove;

        xRight = grid_->getXRight ( k );
        xAbove = grid_->getXAbove ( k );

        ARight = A_->getAx ( *xRight );
        AAbove = A_->getAy ( *xAbove );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = - 2.0 / ( h*h )
                       + ( 1 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( -I*ARight*h ) / ( h*h );
        valuesPsi[2] = exp ( -I*AAbove*h ) / ( h*h );


        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );

        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_BOTTOMRIGHTCONVEX:
        // -----------------------------------------------------------------------
        kLeft  = grid_->getKLeft ( k );
        kAbove = grid_->getKAbove ( k );

        numEntriesPsi = 3;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kLeft;
        columnIndicesPsi[2] = kAbove;


        xLeft  = grid_->getXLeft ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        AAbove = A_->getAy ( *xAbove );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = -2.0 / ( h*h )
                       + ( 1 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ALeft *h ) / ( h*h );
        valuesPsi[2] = exp ( -I*AAbove*h ) / ( h*h );


        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -----------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_TOPRIGHTCONVEX:
        // -----------------------------------------------------------------------
        kLeft  = grid_->getKLeft ( k );
        kBelow = grid_->getKBelow ( k );

        numEntriesPsi = 3;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kLeft;
        columnIndicesPsi[2] = kBelow;

        xLeft  = grid_->getXLeft ( k );
        xBelow = grid_->getXBelow ( k );

        ALeft  = A_->getAx ( *xLeft );
        ABelow = A_->getAy ( *xBelow );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = -2.0 / ( h*h )
                       + ( 1 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ALeft *h ) / ( h*h );
        valuesPsi[2] = exp ( I*ABelow*h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;
        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -----------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_TOPLEFTCONVEX:
        // -----------------------------------------------------------------------
        kRight = grid_->getKRight ( k );
        kBelow = grid_->getKBelow ( k );

        numEntriesPsi = 3;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kRight;
        columnIndicesPsi[2] = kBelow;

        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );

        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = -2.0 / ( h*h )
                       + ( 1 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( -I*ARight*h ) / ( h*h );
        valuesPsi[2] = exp ( I*ABelow*h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -----------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_BOTTOM:
        // -------------------------------------------------------------------
        kLeft  = grid_->getKLeft ( k );
        kRight = grid_->getKRight ( k );
        kAbove = grid_->getKAbove ( k );

        numEntriesPsi = 4;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kLeft;
        columnIndicesPsi[2] = kRight;
        columnIndicesPsi[3] = kAbove;

        xLeft  = grid_->getXLeft ( k );
        xRight = grid_->getXRight ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        ARight = A_->getAx ( *xRight );
        AAbove = A_->getAy ( *xAbove );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = - 3.0            / ( h*h )
                       + ( 1.0 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ALeft *h ) / ( h*h );
        valuesPsi[2] = exp ( -I*ARight*h ) / ( h*h );
        valuesPsi[3] = exp ( -I*AAbove*h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_RIGHT:
        // -------------------------------------------------------------------
        kBelow = grid_->getKBelow ( k );
        kAbove = grid_->getKAbove ( k );
        kLeft  = grid_->getKLeft ( k );

        numEntriesPsi = 4;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kBelow;
        columnIndicesPsi[2] = kAbove;
        columnIndicesPsi[3] = kLeft;

        xLeft  = grid_->getXLeft ( k );
        xBelow = grid_->getXBelow ( k );
        xAbove = grid_->getXAbove ( k );

        ALeft  = A_->getAx ( *xLeft );
        ABelow = A_->getAy ( *xBelow );
        AAbove = A_->getAy ( *xAbove );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = - 3.0            / ( h*h )
                       + ( 1.0 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ABelow*h ) / ( h*h );
        valuesPsi[2] = exp ( -I*AAbove*h ) / ( h*h );
        valuesPsi[3] = exp ( I*ALeft *h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_TOP:
        // -------------------------------------------------------------------
        kBelow = grid_->getKBelow ( k );
        kRight = grid_->getKRight ( k );
        kLeft  = grid_->getKLeft ( k );

        numEntriesPsi = 4;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kBelow;
        columnIndicesPsi[2] = kLeft;
        columnIndicesPsi[3] = kRight;

        xLeft  = grid_->getXLeft ( k );
        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );

        ALeft  = A_->getAx ( *xLeft );
        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = - 3.0             / ( h*h )
                       + ( 1.0 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ABelow*h ) / ( h*h );
        valuesPsi[2] = exp ( I*ALeft *h ) / ( h*h );
        valuesPsi[3] = exp ( -I*ARight*h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -------------------------------------------------------------------
        break;

    case Recti::Grid::Abstract::BOUNDARY_LEFT:
        // -------------------------------------------------------------------
        kBelow = grid_->getKBelow ( k );
        kAbove = grid_->getKAbove ( k );
        kRight = grid_->getKRight ( k );

        numEntriesPsi = 4;
        columnIndicesPsi.resize ( numEntriesPsi );
        columnIndicesPsi[0] = k;
        columnIndicesPsi[1] = kBelow;
        columnIndicesPsi[2] = kAbove;
        columnIndicesPsi[3] = kRight;

        xRight = grid_->getXRight ( k );
        xBelow = grid_->getXBelow ( k );
        xAbove = grid_->getXAbove ( k );

        ARight = A_->getAx ( *xRight );
        ABelow = A_->getAy ( *xBelow );
        AAbove = A_->getAy ( *xAbove );

        valuesPsi.resize ( numEntriesPsi );
        valuesPsi[0] = - 3.0            / ( h*h )
                       + ( 1.0 - 2.0*norm ( psiView[k] ) );
        valuesPsi[1] = exp ( I*ABelow*h ) / ( h*h );
        valuesPsi[2] = exp ( -I*AAbove*h ) / ( h*h );
        valuesPsi[3] = exp ( -I*ARight*h ) / ( h*h );

        numEntriesPsiConj = 1;
        columnIndicesPsiConj.resize ( numEntriesPsiConj );
        columnIndicesPsiConj[0] = k;

        valuesPsiConj.resize ( numEntriesPsiConj );
        valuesPsiConj[0] = -psiView[k]*psiView[k];
        valuesPsiConj[0] *= exp ( I*chi_*2.0 );
        // -------------------------------------------------------------------
        break;

    default:
        TEST_FOR_EXCEPTION ( true,
                             std::logic_error,
                             "Illegal note type \"" << nt << "\"." );
    }

}
// =============================================================================
