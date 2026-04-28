#--------------------------------------------------------
# The CMakeLists.txt for:                     pCoTContact
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# Vehicle SA contact CoT publisher. Tracks NODE_REPORT
# positions and publishes MIL-STD 2525C surface vessel
# contacts to COT_OUTBOUND for pCoTBridge to forward.
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTContact.cpp
  CoTContact_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTContact ${SRC})

TARGET_LINK_LIBRARIES(pCoTContact
  ${MOOS_LIBRARIES}
  mbutil
  ${SYSTEM_LIBS})
