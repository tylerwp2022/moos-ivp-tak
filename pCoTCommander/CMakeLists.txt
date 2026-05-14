#--------------------------------------------------------
# The CMakeLists.txt for:                  pCoTCommander
# Author(s):                               Tyler Errico
# Org:           West Point Robotics Research Center
#
# REV: May 13, 2026 -- handler-class refactor
#
# Links against lib_cot_geodesy for LatLon↔XY conversion.
# lib_cot_geodesy propagates USE_MOOSGEODESY automatically
# via target_compile_definitions(PUBLIC) — no need to
# re-detect MOOSGeodesy here.
#
# Per-command logic lives in handlers/common/ and
# handlers/aquaticus/ as small classes inheriting
# CoTCommandHandler. Adding a new handler means three new
# entries in this file: the .cpp in SRC, the include path
# (already covered by target_include_directories below),
# and the factory wiring (CommandHandlerFactory.cpp).
#--------------------------------------------------------

if(${WIN32})
  SET(SYSTEM_LIBS wsock32)
else()
  SET(SYSTEM_LIBS m pthread)
endif()

SET(SRC
  CoTCommander.cpp
  CoTCommander_Info.cpp
  CoTUtils.cpp
  CommandHandlerFactory.cpp
  main.cpp

  # common/ handlers (mission-agnostic)
  handlers/common/DeployHandler.cpp
  handlers/common/ReturnHandler.cpp
  handlers/common/StationHandler.cpp
  handlers/common/PauseHandler.cpp
  handlers/common/AtakHandler.cpp
  handlers/common/ResumeHandler.cpp
  handlers/common/AvoidHandler.cpp
  handlers/common/OpregHandler.cpp
  handlers/common/StatusHandler.cpp
  handlers/common/WaypointHandler.cpp
  handlers/common/WptModeHandler.cpp
  handlers/common/HelpHandler.cpp

  # aquaticus/ handlers (Aquaticus CTF-specific)
  handlers/aquaticus/AttackHandler.cpp
  handlers/aquaticus/DefendHandler.cpp
  handlers/aquaticus/PlayHandler.cpp
  handlers/aquaticus/StopHandler.cpp
  handlers/aquaticus/UntagHandler.cpp
  handlers/aquaticus/RetryHandler.cpp
  handlers/aquaticus/FlagPursuitHandler.cpp
)

ADD_EXECUTABLE(pCoTCommander ${SRC})

# Add the pCoTCommander/ source dir to the include path so
# handlers in handlers/common/ and handlers/aquaticus/ can
# resolve top-level includes like #include "ParsedCoT.h"
# and #include "CommanderContext.h" without spelling out
# the relative path "../../ParsedCoT.h".
target_include_directories(pCoTCommander PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR})

# lib_cot_geodesy provides CoTGeodesy.h via its PUBLIC include path.
# No need to manually add the include directory here.
TARGET_LINK_LIBRARIES(pCoTCommander
  ${MOOS_LIBRARIES}
  cot_geodesy
  mbutil
  ${SYSTEM_LIBS})
