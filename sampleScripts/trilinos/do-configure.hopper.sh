#!/bin/bash

# Make sure CMake is loaded.
command -v cmake >/dev/null 2>&1 || { echo "cmake not found. Try 'module load cmake'." >&2; exit 1; }

# Serial netCDF is indeed sufficient if
#  -D SEACASExodus_ENABLE_MPI:BOOL=OFF
# is set.
# Use parallel netCDF as soon aa hopper provides shared libs for it.
echo ${CRAY_NETCDF_VERSION?Error: Need netCDF loaded (module load netcdf).}
#echo ${CRAY_NETCDF_HDF5PARALLEL_VERSION?Error: Need parallel netCDF loaded (module load netcdf-hdf5parallel).}

echo ${BOOST_DIR?Error: Need Boost loaded (module load boost/1.50).}

# Check which compiler is loadad.
if [ "$CRAY_PRGENVPGI" == "loaded" ]; then
  COMPILER_NAME="pgi"
  SCILIB_NAME="sci_pgi"
  BINUTILS="OFF"
  SKIP_C_FORTRAN="ON"
elif [ "$CRAY_PRGENVCRAY" == "loaded" ]; then
  echo "Using Cray compilers."
  COMPILER_NAME="cray"
  SCILIB_NAME="sci_cray"
  BINUTILS="OFF"
  SKIP_C_FORTRAN="OFF"
elif [ "$CRAY_PRGENVGNU" == "loaded" ]; then
  echo ${BINUTILS_DIR?Error: Need Binutils loaded (module load binutils).}
  COMPILER_NAME="gnu"
  SCILIB_NAME="sci_gnu"
  BINUTILS="ON"
  SKIP_C_FORTRAN="OFF"
elif [ "$CRAY_PRGENVINTEL" == "loaded" ]; then
  COMPILER_NAME="intel"
  SCILIB_NAME="sci_intel"
  BINUTILS="OFF"
  SKIP_C_FORTRAN="OFF"
elif [ "$CRAY_PRGENVPATHSCALE" == "loaded" ]; then
  COMPILER_NAME="pathscale"
  SCILIB_NAME="sci_pathscale"
  BINUTILS="OFF"
  SKIP_C_FORTRAN="OFF"
else
  echo "Unknown compiler suite selected. Abort."
  exit 1
fi

# Give the user some time to ctrl-c out. :)
echo "Using <$COMPILER_NAME> compilers."
sleep 5


# Use
# TPL_FIND_SHARED_LIBS:BOOL=OFF,
# Trilinos_LINK_SEARCH_START_STATIC:BOOL=ON
# to force Trilinos to find the static versions of the TPLs
# instead of their shared counterparts.

# On hopper, BLAS and LAPACK are implicitly provided by the compiler iff
# the cray-libsci module is loaded.
# We could set  TPL_{BLAS,LAPACK}_LIBRARIES to " " in this case, and
# ignore the compiler warnings
#
#   ignoring option '-l'; argument required.
#
# We can also just explicitly link the libraries again.
#
echo ${CRAY_LIBSCI_PREFIX_DIR?Error: Need libsci loaded (module load cray-libsci).}

# For building static libraries:
#  -D BUILD_SHARED_LIBS:BOOL=OFF \
#  -D TPL_FIND_SHARED_LIBS:BOOL=OFF \
#  -D Trilinos_LINK_SEARCH_START_STATIC:BOOL=ON \
#
# For dynamic linking on hopper, see
# <http://www.nersc.gov/users/computational-systems/hopper/programming/shared-and-dynamic-libraries/>.
#  -D BUILD_SHARED_LIBS:BOOL=ON \
#  -D TPL_FIND_SHARED_LIBS:BOOL=ON \
#  -D CMAKE_EXE_LINKER_FLAGS:STRING="-dynamic" \
#  -D CMAKE_SHARED_LINKER_FLAGS:STRING="-Wl,--no-undefined" \

cmake \
  -D CMAKE_INSTALL_PREFIX:PATH="$SCRATCH/trilinos/dev/${COMPILER_NAME}/" \
  -D CMAKE_BUILD_TYPE:STRING=Release \
  -D BUILD_SHARED_LIBS:BOOL=OFF \
  -D TPL_FIND_SHARED_LIBS:BOOL=OFF \
  -D Trilinos_LINK_SEARCH_START_STATIC:BOOL=ON \
  -D Trilinos_ENABLE_DEVELOPMENT_MODE:BOOL=OFF \
  -D Trilinos_ENABLE_TESTS:BOOL=ON \
  -D Trilinos_ENABLE_OpenMP:BOOL=ON \
  -D Trilinos_SKIP_FORTRANCINTERFACE_VERIFY_TEST:BOOL=${SKIP_C_FORTRAN} \
  -D Trilinos_ENABLE_TEUCHOS_TIME_MONITOR:BOOL=ON \
  -D Trilinos_ENABLE_Anasazi:BOOL=ON \
  -D Trilinos_ENABLE_Belos:BOOL=ON \
  -D Trilinos_ENABLE_ML:BOOL=ON \
  -D Trilinos_ENABLE_NOX:BOOL=ON \
      -D NOX_ENABLE_LOCA:BOOL=ON \
  -D Trilinos_ENABLE_Piro:BOOL=ON \
  -D Trilinos_ENABLE_STK:BOOL=ON \
  -D Trilinos_ENABLE_SEACASIoss:BOOL=ON \
    -D SEACASExodus_ENABLE_MPI:BOOL=OFF \
  -D TPL_ENABLE_BinUtils:BOOL=${BINUTILS} \
  -D TPL_ENABLE_MPI:BOOL=ON \
      -D MPI_C_COMPILER:FILEPATH="$ASYNCPE_DIR/bin/cc" \
      -D MPI_CXX_COMPILER:FILEPATH="$ASYNCPE_DIR/bin/CC" \
      -D MPI_Fortran_COMPILER:FILEPATH="$ASYNCPE_DIR/bin/ftn" \
      -D MPI_EXEC:FILEPATH="aprun" \
      -D MPI_EXEC_NUMPROCS_FLAG:STRING="-n" \
      -D MPI_EXEC_MAX_NUMPROCS:STRING="24" \
  -D TPL_ENABLE_Boost:BOOL=ON \
      -D Boost_LIBRARY_DIRS:PATH="$BOOST_DIR/libs" \
      -D Boost_INCLUDE_DIRS:PATH="$BOOST_DIR/include" \
  -D TPL_ENABLE_BLAS:BOOL=ON \
      -D BLAS_LIBRARY_DIRS:PATH="$CRAY_LIBSCI_PREFIX_DIR/lib" \
      -D BLAS_LIBRARY_NAMES:STRING="${SCILIB_NAME}" \
  -D TPL_ENABLE_LAPACK:BOOL=ON \
      -D LAPACK_LIBRARY_DIRS:PATH="$CRAY_LIBSCI_PREFIX_DIR/lib" \
      -D LAPACK_LIBRARY_NAMES:STRING="${SCILIB_NAME}" \
  -D TPL_ENABLE_Netcdf:BOOL=ON \
      -D Netcdf_INCLUDE_DIRS:PATH="$NETCDF_DIR/include" \
      -D Netcdf_LIBRARY_DIRS:PATH="$NETCDF_DIR/lib" \
  ${HOME}/software/trilinos/dev/
