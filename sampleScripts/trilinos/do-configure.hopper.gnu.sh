#!/bin/bash

# Make sure the correct compiler is currently loaded.
if [ "$CRAY_PRGENVGNU" != "loaded" ]; then
echo "Incorrect compiler. Abort."
exit 1
fi

module load cmake
module load boost
module load binutils
module load netcdf

# Use TPL_FIND_SHARED_LIBS:BOOL=OFF to force Trilinos to find
# the static versions of the TPLs instead of their shared
# counterparts.

cmake \
  -D CMAKE_INSTALL_PREFIX:PATH="$SCRATCH/trilinos/dev/gnu/" \
  -D Trilinos_ENABLE_TEUCHOS_TIME_MONITOR:BOOL=ON \
  -D Trilinos_ENABLE_DEVELOPMENT_MODE:BOOL=OFF \
  -D BUILD_SHARED_LIBS:BOOL=OFF \
  -D TPL_FIND_SHARED_LIBS:BOOL=OFF \
  -D CMAKE_BUILD_TYPE=Release \
  -D TPL_ENABLE_MPI:BOOL=ON \
      -D MPI_C_COMPILER:FILEPATH="$ASYNCPE_DIR/bin/cc" \
      -D MPI_CXX_COMPILER:FILEPATH="$ASYNCPE_DIR/bin/CC" \
      -D MPI_Fortran_COMPILER:FILEPATH="$ASYNCPE_DIR/bin/ftn" \
  -D TPL_ENABLE_BLAS:BOOL=ON \
      -D BLAS_LIBRARY_DIRS:FILEPATH="$CRAY_LIBSCI_PREFIX_DIR/lib" \
      -D BLAS_LIBRARY_NAMES="sci_gnu" \
  -D TPL_ENABLE_LAPACK:BOOL=ON \
      -D LAPACK_LIBRARY_DIRS:FILEPATH="$CRAY_LIBSCI_PREFIX_DIR/lib" \
      -D LAPACK_LIBRARY_NAMES="sci_gnu" \
  -D TPL_ENABLE_Boost:BOOL=ON \
      -D Boost_LIBRARY_DIRS:PATH="$BOOST_DIR/libs" \
      -D Boost_INCLUDE_DIRS:PATH="$BOOST_DIR/include" \
  -D TPL_ENABLE_Netcdf:BOOL=ON \
      -D Netcdf_LIBRARY_DIRS:FILEPATH="$NETCDF_DIR/lib" \
      -D Netcdf_INCLUDE_DIRS:FILEPATH="$NETCDF_DIR/include" \
      -D Netcdf_LIBRARY_NAMES="netcdf" \
  -D Trilinos_ENABLE_NOX:BOOL=ON \
      -D NOX_ENABLE_LOCA:BOOL=ON \
  -D Trilinos_ENABLE_Piro:BOOL=ON \
  -D Trilinos_ENABLE_ML:BOOL=ON \
  -D Trilinos_ENABLE_Belos:BOOL=ON \
  -D Trilinos_ENABLE_STK:BOOL=ON \
  -D Trilinos_ENABLE_Anasazi:BOOL=ON \
  -D Trilinos_ENABLE_SEACASIoss:BOOL=ON \
  ../../source
