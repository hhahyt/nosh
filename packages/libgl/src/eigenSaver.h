#include <Teuchos_RCP.hpp>

#include <LOCA_SaveEigenData_AbstractStrategy.H>
#include <LOCA_Parameter_SublistParser.H>

#include <Teuchos_ParameterList.hpp>

#include "glSystem.h"

class EigenSaver : public LOCA::SaveEigenData::AbstractStrategy
{

 public:

// Actually suggested interface:
//    EigenSaver(
//      const Teuchos::RCP<LOCA::GlobalData>& global_data,
//      const Teuchos::RCP<LOCA::Parameter::SublistParser>& topParams,
//      const Teuchos::RCP<Teuchos::ParameterList>& eigenParams      );

   // Constructor
   EigenSaver( const Teuchos::RCP<Teuchos::ParameterList> eigenParams,
	           const std::string outputDir,
	           const std::string eigenvaluesFileName,
	           const std::string contFileBaseName,
	           const std::string eigenstateFileNameAppendix,
               const Teuchos::RCP<AbstractStateWriter> glSys );

   virtual ~EigenSaver();

   virtual NOX::Abstract::Group::ReturnType
   save ( Teuchos::RCP<std::vector<double> >       &evals_r,
          Teuchos::RCP<std::vector<double> >       &evals_i,
          Teuchos::RCP<NOX::Abstract::MultiVector> &evecs_r,
          Teuchos::RCP<NOX::Abstract::MultiVector> &evecs_i  );

   void
   setLocaStepper( const Teuchos::RCP<LOCA::Stepper> locaStepper );

	// This function is necessary to break the circular dependency with the
	// LOCA_Stepper object to allow for a clean termination
	void
	releaseLocaStepper();

 protected:
  private:
    Teuchos::RCP<Teuchos::ParameterList> eigenParamList_;
    std::string outputDir_;
    std::string eigenvaluesFilePath_;
    std::string contFileBaseName_;
    std::string eigenstateFileNameAppendix_;
    Teuchos::RCP<AbstractStateWriter> stateWriter_;
    Teuchos::RCP<LOCA::Stepper> locaStepper_;

    //! The minimum number of stable eigenvalues that is to be computed in each step.
    unsigned int numComputeStableEigenvalues_;

    //! Maximum number of eigenvalues that are stored in \c eigenvaluesFilePath_.
    unsigned int maxEigenvaluesSave_;

    void
    saveEigenstate ( const std::string                         fileName,
                     const Teuchos::RCP<NOX::Abstract::Vector> &evec_r  );

};