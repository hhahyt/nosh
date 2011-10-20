// Workaround for icpc's error "Include mpi.h before stdio.h"
#include <Teuchos_config.h>
#ifdef HAVE_MPI
    #include <mpi.h>
#endif

#include <Epetra_Vector.h>
#include <Epetra_CrsMatrix.h>

#ifdef HAVE_MPI
#include <Epetra_MpiComm.h>
#else
#include <Epetra_SerialComm.h>
#endif

#include <Teuchos_TimeMonitor.hpp>

// =============================================================================
int main ( int argc, char *argv[] )
{
  // Initialize MPI
#ifdef HAVE_MPI
  MPI_Init( &argc, &argv );
#endif

    // Create a communicator for Epetra objects
#ifdef HAVE_MPI
    Teuchos::RCP<Epetra_MpiComm> eComm =
      Teuchos::rcp<Epetra_MpiComm> ( new Epetra_MpiComm ( MPI_COMM_WORLD ) );
#else
    Teuchos::RCP<Epetra_SerialComm>  eComm =
           Teuchos::rcp<Epetra_SerialComm> ( new Epetra_SerialComm() );
#endif

    int status = 0;

    try
    {
      // =======================================================================
      // Create map.
      // Do strong scaling tests, so keep numGlobalElements independent of
      // the number of processes.
      int maxSize = 9;
      for ( int k = 0; k != maxSize+1; k++ )
      {
          int numGlobalElements = pow( 10, k );
          // create map
          int indexBase = 0;
          Teuchos::RCP<Epetra_Map> map =
              Teuchos::rcp( new Epetra_Map ( numGlobalElements, indexBase, *eComm ) );
          // create vectors
          Teuchos::RCP<Epetra_Vector> u = Teuchos::rcp( new Epetra_Vector( *map ) );
          u->Random();
          Teuchos::RCP<Epetra_Vector> v = Teuchos::rcp( new Epetra_Vector( *map ) );
          v->Random();

          // create timer label
          std::stringstream label;
          label << "Vector::Dot 10^" << k;

          Teuchos::RCP<Teuchos::Time> dotTime =
              Teuchos::TimeMonitor::getNewTimer( label.str() );
          {
              Teuchos::TimeMonitor tm(*dotTime);
              double dot;
              TEUCHOS_ASSERT_EQUALITY( 0, u->Dot( *v, &dot ) );
          }
      }
      // =======================================================================
      // print timing data
      Teuchos::TimeMonitor::summarize();
      // -----------------------------------------------------------------------
    }
    catch ( std::exception & e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << e.what() << std::endl;
        status += 10;
    }
    catch ( std::string & e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << e << std::endl;
        status += 10;
    }
    catch ( const char * e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << e << std::endl;
        status += 10;
    }
    catch ( int e )
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << "Caught unknown exception code " << e <<  "." << std::endl;
        status += 10;
    }
    catch (...)
    {
        if ( eComm->MyPID() == 0 )
            std::cerr << "Caught unknown exception." << std::endl;
        status += 10;
    }

#ifdef HAVE_MPI
      MPI_Finalize();
#endif

    return status==0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
// =========================================================================