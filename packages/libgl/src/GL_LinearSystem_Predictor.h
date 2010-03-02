#ifndef GLPREDICTORSYSTEM_H
#define GLPREDICTORSYSTEM_H

#include "GL_LinearSystem_Bordered.h"
#include "AbstractStateWriter.h"

#include <Epetra_Comm.h>
#include <Epetra_Map.h>
#include <Epetra_Vector.h>

#include <Teuchos_RCP.hpp>

#include <Epetra_CrsMatrix.h>

#include <Teuchos_ParameterList.hpp>

#include <NOX_Epetra_Interface_Required.H> // NOX base class
#include <NOX_Epetra_Interface_Jacobian.H> // NOX base class
#include <NOX_Epetra_Interface_Preconditioner.H> // NOX base class
#include <LOCA_Epetra_Interface_Required.H> // LOCA base class
#include <NOX_Abstract_PrePostOperator.H>

#include <LOCA_Parameter_Vector.H>

#include <Tpetra_Map.hpp>
#include <Tpetra_Vector.hpp>

#include <Thyra_OperatorVectorTypes.hpp> // For Thyra::Ordinal

#include <NOX_Abstract_Group.H>

#include <LOCA_Stepper.H>

typedef Tpetra::Vector<double_complex, Thyra::Ordinal> ComplexVector;

namespace GL {
  namespace LinearSystem {

class Predictor:
        public LOCA::Epetra::Interface::Required,
        public NOX::Epetra::Interface::Jacobian,
        public NOX::Epetra::Interface::Preconditioner
{
public:

        Predictor( GinzburgLandau::GinzburgLandau &gl,
                   const Teuchos::RCP<const Epetra_Comm> eComm,
                   const Teuchos::RCP<ComplexVector> psi,
                   const Teuchos::RCP<ComplexVector> tangent,
                   const Teuchos::RCP<ComplexVector> predictor,
                   const std::string outputDir,
                   const std::string outputDataFileName,
                   const std::string outputFileFormat,
                    const std::string solutionFileNameBase,
                    const std::string nullvectorFileNameBase
                 );

        //! Destructor
        ~Predictor();

        //! Evaluate the Ginzburg--Landau functions at a given state defined
        //! by the input vector x.
        virtual bool
        computeF(const Epetra_Vector &x, Epetra_Vector &F,
             const NOX::Epetra::Interface::Required::FillType fillFlag = NOX::Epetra::Interface::Required::Residual);

        //! Evaluate the Jacobian matrix of the Ginzburg--Landau problem
        //! at a given state defined by the input vector x.
        virtual bool
        computeJacobian(const Epetra_Vector &x, Epetra_Operator &Jac);

        //! Dummy preconditioner function. So far does nothing but throwing
        //! an exception when called.
        virtual bool
        computePreconditioner(const Epetra_Vector &x, Epetra_Operator &Prec,
                        Teuchos::ParameterList* precParams = 0);

        //! Returns the current state. Not necessarily a solution to the problem.
        //! @return Reference-counted pointer to the current state.
        Teuchos::RCP<Epetra_Vector>
        getSolution() const;

        //! Returns the current Jacobian.
        //! @return Reference-counted pointer to the Jacobian.
        Teuchos::RCP<Epetra_CrsMatrix>
        getJacobian() const;

        Teuchos::RCP<Epetra_CrsMatrix>
        getPreconditioner() const;

        //! Set the problem parameters.
        virtual void
        setParameters(const LOCA::ParameterVector &p);

        //! Set directory to where all output gets written.
        //! @param directory Name of the directory.
        void
        setOutputDir(const string &directory);

        //! Print the solution x along with the continuation parameter conParam
        //! to a file. This function is called internally by LOCA to print
        //! solutions of each continuation step.
        virtual void
        printSolution(const Epetra_Vector &x, double conParam);

        void
        setLocaStepper( const Teuchos::RCP<const LOCA::Stepper> stepper );

        // This function is necessary to break the circular dependency with the
        // LOCA_Stepper object to allow for a clean termination
        void
        releaseLocaStepper();

//        //! Explicitly print the solution x along with the problem parameters
//        //! to the file fileName.
//        void
//        writeSolutionToFile( const Epetra_Vector &x,
//                             const std::string &filePath) const;
//
//        void
//        writeAbstractStateToFile( const Epetra_Vector &x,
//                                  GlPredictorSystemconst std::string &filePath) const;

//        Teuchos::RCP<Epetra_Vector>
//        getGlSystemVector( const Teuchos::RCP<const ComplexVector> psi ) const;

private:

    void
    PutMyValues( Teuchos::RCP<Epetra_CrsMatrix> mat,
                 int     row,
                 int     numEntries,
                 double* values,
                 int*    columns ) const;

    void
    PutGlobalValues( Teuchos::RCP<Epetra_CrsMatrix> mat,
                     int     row,
                     int     numEntries,
                     double* values,
                     int*    columns ) const;

        enum continuationType {
                ONEPARAMETER,
                TURNINGPOINT
        };

        Teuchos::RCP<Epetra_Map>
        createExtendedMap( const Epetra_BlockMap & sliceMap  );

        bool
        createJacobian(const Epetra_Vector &x);

        //! Print method for the continuation in one parameter.
        void
        printSolutionOneParameterContinuation( const Teuchos::RCP<const ComplexVector> & psi
                                             ) const;

        //! Print method for turning point continuation continuation.
        void
        printSolutionTurningPointContinuation( const Teuchos::RCP<const ComplexVector> & psi
                                         ) const;


        //! Write statistics about the current continuation step to the file
        //! \c outputDataFileName_ .
        void
        writeContinuationStats( const int conStep,
                                        const Teuchos::RCP<const ComplexVector> psi ) const;

    //! Translate an Epetra_Comm into a Teuchos::Comm<int>, no matter the Thyra::Ordinal.
    Teuchos::RCP<const Teuchos::Comm<int> >
    create_CommInt( const Teuchos::RCP<const Epetra_Comm> &epetraComm );


        bool firstTime_;

        const Teuchos::RCP<Bordered> glSystem_ ;

        const Teuchos::RCP<const Epetra_Comm> EComm_;
        Teuchos::RCP<const Epetra_Map> sliceMap_;
        Teuchos::RCP<const Epetra_Map> extendedMap_;
        Teuchos::RCP<Epetra_CrsMatrix> jacobian_;
        Teuchos::RCP<Epetra_CrsMatrix> preconditioner_;
        Teuchos::RCP<Epetra_Vector> initialSolution_;

        const Teuchos::RCP<ComplexVector> psi_;
        const Teuchos::RCP<ComplexVector> tangent_;
        const Teuchos::RCP<ComplexVector> predictor_;

};

  } // namespace LinearSystem
} // namespace GL

#endif // GLPREDICTORSYSTEM_H
