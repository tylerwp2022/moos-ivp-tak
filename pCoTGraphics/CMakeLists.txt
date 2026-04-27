#--------------------------------------------------------
# The CMakeLists.txt for:                    pCoTGraphics
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# Links against lib_cot_geodesy for XY→LatLon conversion.
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTGraphics.cpp
  CoTGraphics_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTGraphics ${SRC})

TARGET_LINK_LIBRARIES(pCoTGraphics
  ${MOOS_LIBRARIES}
  cot_geodesy
  mbutil
  ${SYSTEM_LIBS})
