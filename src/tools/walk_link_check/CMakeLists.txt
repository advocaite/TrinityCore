# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# Offline check of candidate walk links between pieces of the bots' travel graph. Pure Detour API (no TC deps).

add_executable(walk_link_check
  walk_link_check.cpp)

target_link_libraries(walk_link_check
  PRIVATE
    Detour)

set_target_properties(walk_link_check
  PROPERTIES
    FOLDER "tools")

if(UNIX)
  install(TARGETS walk_link_check DESTINATION bin)
elseif(WIN32)
  install(TARGETS walk_link_check DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
