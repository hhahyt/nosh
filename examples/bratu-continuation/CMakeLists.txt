INCLUDE_DIRECTORIES(
  ${PROJECT_SOURCE_DIR}/src/
  ${CMAKE_CURRENT_BINARY_DIR}  # for the output header
  )
INCLUDE_DIRECTORIES(
  SYSTEM
  ${Trilinos_INCLUDE_DIRS}
  ${Trilinos_TPL_INCLUDE_DIRS}
  ${Mikado_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIR}
  )

# generate header file
ADD_CUSTOM_COMMAND(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/bratu.hpp
  COMMAND ${PROJECT_SOURCE_DIR}/nfc/nosh-compile ${CMAKE_CURRENT_SOURCE_DIR}/bratu.py ${CMAKE_CURRENT_BINARY_DIR}/bratu.hpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bratu.py
  )

# copy over mesh files
ADD_CUSTOM_COMMAND(
   OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pacman.e
   COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_BINARY_DIR}/test/data/pacman.e ${CMAKE_CURRENT_BINARY_DIR}/pacman.e
   DEPENDS noshTestFetchData
   )

SET(MY_EXECUTABLE bratu-continuation)
ADD_EXECUTABLE(${MY_EXECUTABLE} bratu.cpp bratu.hpp pacman.e)
TARGET_LINK_LIBRARIES(
  ${MY_EXECUTABLE}
  nosh
  ${Mikado_LIBRARIES}
  )
