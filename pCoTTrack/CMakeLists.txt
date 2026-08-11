#--------------------------------------------------------
# The CMakeLists.txt for:                       pCoTTrack
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# Inbound TAK SA consumer. Converts ATAK ground-unit PLI
# arriving on COT_INBOUND into NODE_REPORT so ATAK users
# appear as contacts in MOOS-IvP.
#
# Links against lib_cot_geodesy for LatLon→XY conversion.
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTTrack.cpp
  CoTTrack_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTTrack ${SRC})

TARGET_LINK_LIBRARIES(pCoTTrack
  ${MOOS_LIBRARIES}
  cot_geodesy
  mbutil
  ${SYSTEM_LIBS})
