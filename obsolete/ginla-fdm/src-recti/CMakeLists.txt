SET ( recti_SRCS
      Recti_Domain_Abstract.cpp
      Recti_Domain_Ellipse.cpp
      Recti_Domain_Circle.cpp
      Recti_Domain_Polygon.cpp
      Recti_Domain_Rectangle.cpp
      Recti_Domain_Square.cpp
      Recti_Domain_Factory.cpp
      Recti_Grid_Abstract.cpp
      Recti_Grid_General.cpp
      Recti_Grid_Uniform.cpp
      Recti_Grid_UniformAbstract.cpp
      Recti_Grid_Square.cpp
      Recti_Grid_UniformSquare.cpp
      Recti_Grid_Reader.cpp
    )

FIND_PACKAGE( Trilinos REQUIRED )
INCLUDE_DIRECTORIES( ${Trilinos_INCLUDE_DIRS} )

FIND_PACKAGE( VTK REQUIRED )
INCLUDE_DIRECTORIES( ${VTK_INCLUDE_DIRS} )

# workaround for Teuchos not appending Boost INCLUDES
FIND_PACKAGE( Boost REQUIRED )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIRS} )

INCLUDE_DIRECTORIES( ${ginzburg-landau_SOURCE_DIR}/packages/ginla-fdm/src-vio
                     ${ginzburg-landau_SOURCE_DIR}/packages/ginla-fdm/src )

ADD_LIBRARY ( recti
              ${recti_SRCS} )
