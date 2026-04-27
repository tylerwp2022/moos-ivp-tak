#--------------------------------------------------------
# The CMakeLists.txt for:                       pCoTChat
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# No geodesy dependency — chat CoT uses raw lat/lon from
# NODE_REPORT directly without local XY conversion.
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTChat.cpp
  CoTChat_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTChat ${SRC})

TARGET_LINK_LIBRARIES(pCoTChat
  ${MOOS_LIBRARIES}
  mbutil
  ${SYSTEM_LIBS})
