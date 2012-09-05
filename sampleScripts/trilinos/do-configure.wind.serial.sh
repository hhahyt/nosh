cmake \
  -D CMAKE_INSTALL_PREFIX:PATH=/opt/trilinos/dev/master/gcc/4.6.3/release \
  -D CMAKE_BUILD_TYPE=Debug \
  -D CMAKE_CXX_FLAGS="-Werror=format-security" \
  -D Trilinos_ENABLE_ALL_PACKAGES:BOOL=ON \
  -D Trilinos_ENABLE_SECONDARY_STABLE_CODE:BOOL=ON \
  -D Trilinos_ENABLE_PyTrilinos:BOOL=OFF \
  -D TPL_ENABLE_Matio:BOOL=OFF \
  -D TPL_ENABLE_MPI:BOOL=OFF \
  -D Trilinos_ENABLE_TEUCHOS_TIME_MONITOR=ON \
  -D Trilinos_ENABLE_NOX:BOOL=ON \
      -D NOX_ENABLE_LOCA:BOOL=ON \
  -D Trilinos_ENABLE_Piro:BOOL=ON \
  -D Trilinos_ENABLE_ML:BOOL=ON \
  -D Trilinos_ENABLE_Anasazi:BOOL=ON \
  -D Trilinos_ENABLE_STK:BOOL=ON \
  -D Trilinos_ENABLE_SEACASIoss:BOOL=ON \
  -D Trilinos_ENABLE_SEACASNemslice:BOOL=ON \
  -D Trilinos_ENABLE_SEACASNemspread:BOOL=ON \
  -D TPL_ENABLE_Netcdf:BOOL=ON \
      -D Netcdf_LIBRARY_DIRS:PATH=/opt/netcdf/4.2/lib/ \
      -D TPL_Netcdf_INCLUDE_DIRS:PATH=/opt/netcdf/4.2/include \
  ../../source
