#--------------------------------------------------------
# The CMakeLists.txt for:                  pCoTCommander
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# Links against lib_cot_geodesy for LatLon↔XY conversion.
# lib_cot_geodesy propagates USE_MOOSGEODESY automatically
# via target_compile_definitions(PUBLIC) — no need to
# re-detect MOOSGeodesy here.
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTCommander.cpp
  CoTCommander_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTCommander ${SRC})

# lib_cot_geodesy provides CoTGeodesy.h via its PUBLIC include path.
# No need to manually add the include directory here.
TARGET_LINK_LIBRARIES(pCoTCommander
  ${MOOS_LIBRARIES}
  cot_geodesy
  mbutil
  ${SYSTEM_LIBS})
