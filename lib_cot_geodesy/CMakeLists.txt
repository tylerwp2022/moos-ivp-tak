#--------------------------------------------------------
# The CMakeLists.txt for:                  lib_cot_geodesy
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# Shared geodesy utility for the pCoT* app family.
# pCoTGraphics and pCoTCommander link against this library.
#--------------------------------------------------------

OPTION(USE_MOOSGEODESY "Use MOOSGeodesy for accurate XY→LatLon" ON)

SET(SRC
  CoTGeodesy.cpp
)

ADD_LIBRARY(cot_geodesy STATIC ${SRC})

# Export the include directory so linking apps can find CoTGeodesy.h
TARGET_INCLUDE_DIRECTORIES(cot_geodesy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --------------------------------------------------------
# MOOSGeodesy discovery — same three-stage logic as pCoTBridge.
# If found: define USE_MOOSGEODESY so CoTGeodesy.cpp uses it.
# If not:   silently fall back to flat-earth NAV mode.
# --------------------------------------------------------
if(USE_MOOSGEODESY)

  find_package(MOOSGeodesy QUIET)

  if(MOOSGeodesy_FOUND)
    message(STATUS "lib_cot_geodesy: MOOSGeodesy via find_package")
    target_include_directories(cot_geodesy PUBLIC ${MOOSGeodesy_INCLUDE_DIRS})
    target_link_libraries(cot_geodesy ${MOOSGeodesy_LIBRARIES})
    target_compile_definitions(cot_geodesy PUBLIC USE_MOOSGEODESY)

  else()
    # Known paths on Tyler's machine
    set(GEODESY_INC
      "$ENV{HOME}/moos-ivp/MOOS_Jul2724/MOOSGeodesy/libMOOSGeodesy/include"
    )
    set(GEODESY_LIB
      "$ENV{HOME}/moos-ivp/build/MOOS/MOOSGeodesy/lib/libMOOSGeodesy.so"
    )
    set(GEODESY_H "${GEODESY_INC}/MOOS/libMOOSGeodesy/MOOSGeodesy.h")

    if(EXISTS "${GEODESY_H}" AND EXISTS "${GEODESY_LIB}")
      message(STATUS "lib_cot_geodesy: MOOSGeodesy found at known path")
      target_include_directories(cot_geodesy PUBLIC ${GEODESY_INC})
      target_link_libraries(cot_geodesy ${GEODESY_LIB})
      target_compile_definitions(cot_geodesy PUBLIC USE_MOOSGEODESY)
    else()
      message(WARNING
        "lib_cot_geodesy: MOOSGeodesy not found — NAV fallback mode active.\n"
        "  Run build-moos.sh first, or pass -DUSE_MOOSGEODESY=OFF to suppress.")
    endif()
  endif()

endif()
