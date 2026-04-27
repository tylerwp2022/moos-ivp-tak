#--------------------------------------------------------
# The CMakeLists.txt for:                      pCoTBridge
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTBridge.cpp
  CoTBridge_Info.cpp
  main.cpp
)

ADD_EXECUTABLE(pCoTBridge ${SRC})

# --------------------------------------------------------
# OpenSSL — required for TLS connection to port 8089.
# Plain TCP (port 8088) works without this but TLS will
# fail to compile if OpenSSL is not found.
# --------------------------------------------------------
find_package(OpenSSL REQUIRED)
message(STATUS "pCoTBridge: OpenSSL ${OPENSSL_VERSION} found")

TARGET_LINK_LIBRARIES(pCoTBridge
  ${MOOS_LIBRARIES}
  mbutil
  OpenSSL::SSL
  OpenSSL::Crypto
  ${SYSTEM_LIBS})
