#--------------------------------------------------------
# The CMakeLists.txt for:               pCoTShoreContact
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# Publishes a static SA contact CoT to COT_OUTBOUND for
# pCoTBridge to forward to the TAK server. Used to mark
# the shoreside control station on the ATAK map.
#
# Links against lib_cot_geodesy (shared geodesy utilities)
# and mbutil (MOOS string utilities).
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTShoreContact.cpp
  CoTShoreContact_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTShoreContact ${SRC})

TARGET_LINK_LIBRARIES(pCoTShoreContact
  ${MOOS_LIBRARIES}
  mbutil
  ${SYSTEM_LIBS})
