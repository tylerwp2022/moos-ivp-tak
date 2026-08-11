#============================================================================
# CMakeLists.txt — moos-ivp-tak
#
# Top-level build file for the pCoT* TAK bridge app family.
#
# DEPENDENCIES:
#   - MOOS-IvP (https://oceanai.mit.edu/moos-ivp)
#     Set MOOS_IVP_BUILD_ROOT to the moos-ivp build directory.
#     Default: $ENV{HOME}/moos-ivp/build
#
#   - OpenSSL (for TLS support in pCoTBridge)
#     Ubuntu/Debian: sudo apt install libssl-dev
#
# BUILD:
#   mkdir build && cd build
#   cmake .. -DMOOS_IVP_BUILD_ROOT=~/moos-ivp/build
#   make -j$(nproc)
#   sudo make install      # installs to ~/moos-ivp/bin by default
#
#============================================================================

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(moos-ivp-tak)

#============================================================================
# Find MOOS-IvP
#============================================================================

SET(MOOS_IVP_BUILD_ROOT "$ENV{HOME}/moos-ivp/build"
    CACHE PATH "Path to the moos-ivp build directory")

SET(MOOS_IVP_SRC_ROOT "$ENV{HOME}/moos-ivp"
    CACHE PATH "Path to the moos-ivp source directory")

# moos-ivp libraries
FIND_LIBRARY(MOOS_LIB MOOS
  HINTS ${MOOS_IVP_BUILD_ROOT}/MOOS/libMOOS
        ${MOOS_IVP_BUILD_ROOT}/MOOS/libMOOS/MOOSLib)

FIND_LIBRARY(MBUTIL_LIB mbutil
  HINTS ${MOOS_IVP_BUILD_ROOT}/lib)

FIND_LIBRARY(APPCAST_LIB apputil
  HINTS ${MOOS_IVP_BUILD_ROOT}/lib)

IF(NOT MOOS_LIB)
  MESSAGE(FATAL_ERROR
    "MOOS library not found. Set MOOS_IVP_BUILD_ROOT to your moos-ivp build directory.\n"
    "  cmake .. -DMOOS_IVP_BUILD_ROOT=/path/to/moos-ivp/build")
ENDIF()

SET(MOOS_LIBRARIES ${MOOS_LIB} ${MBUTIL_LIB} ${APPCAST_LIB})

# moos-ivp include paths
INCLUDE_DIRECTORIES(
  ${MOOS_IVP_SRC_ROOT}/MOOS_Jul2724/MOOSCore/Core/libMOOS/include
  ${MOOS_IVP_SRC_ROOT}/MOOS_Jul2724/MOOSCore/Core/libMOOS/Thirdparty/AppCasting/include
  ${MOOS_IVP_SRC_ROOT}/ivp/src/lib_mbutil
  ${MOOS_IVP_SRC_ROOT}/ivp/src/lib_apputil
  ${MOOS_IVP_BUILD_ROOT}/MOOS/MOOSGeodesy/include
)

#============================================================================
# OpenSSL (required for pCoTBridge TLS)
#============================================================================

FIND_PACKAGE(OpenSSL REQUIRED)

#============================================================================
# Install destination — drop binaries into moos-ivp bin
#============================================================================

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${MOOS_IVP_SRC_ROOT}/bin)

#============================================================================
# Subprojects
#============================================================================

ADD_SUBDIRECTORY(lib_cot_geodesy)
ADD_SUBDIRECTORY(pCoTContact)
ADD_SUBDIRECTORY(pCoTTrack)
ADD_SUBDIRECTORY(pCoTBridge)
ADD_SUBDIRECTORY(pCoTCommander)
ADD_SUBDIRECTORY(pCoTGraphics)
ADD_SUBDIRECTORY(pCoTChat)
ADD_SUBDIRECTORY(pCoTShoreContact)
