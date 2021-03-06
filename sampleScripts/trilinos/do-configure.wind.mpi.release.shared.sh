#!/bin/sh

CC=mpicc \
CXX=mpic++ \
cmake \
  -D CMAKE_INSTALL_PREFIX:PATH=/opt/trilinos/dev/openmpi/1.4.3/gcc/4.6.3/release/shared \
  -D Trilinos_ENABLE_DEVELOPMENT_MODE:BOOL=OFF \
  -D CMAKE_BUILD_TYPE:STRING=Release \
  -D BUILD_SHARED_LIBS:BOOL=ON \
  -D TPL_FIND_SHARED_LIBS:BOOL=ON \
  -D TPL_ENABLE_MPI:BOOL=ON \
  -D Trilinos_ENABLE_TEUCHOS_TIME_MONITOR:BOOL=ON \
  -D Trilinos_ENABLE_NOX:BOOL=ON \
      -D NOX_ENABLE_LOCA:BOOL=ON \
  -D Trilinos_ENABLE_Piro:BOOL=ON \
  -D Trilinos_ENABLE_ML:BOOL=ON \
  -D Belos_ENABLE_TESTS:BOOL=ON \
  -D Trilinos_ENABLE_Anasazi:BOOL=ON \
  -D Trilinos_ENABLE_STK:BOOL=ON \
  -D Trilinos_ENABLE_SEACASIoss:BOOL=ON \
  -D SEACASExodus_ENABLE_MPI:BOOL=OFF \
  -D TPL_ENABLE_Netcdf:BOOL=ON \
      -D Netcdf_LIBRARY_DIRS:PATH=/opt/netcdf/4.2.1.1/lib/ \
      -D TPL_Netcdf_INCLUDE_DIRS:PATH=/opt/netcdf/4.2.1.1/include \
  ../../publicTrilinos

#  -D CMAKE_SHARED_LINKER_FLAGS:STRING="-Wl,--no-undefined" \
