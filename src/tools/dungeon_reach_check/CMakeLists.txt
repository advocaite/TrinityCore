# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# Offline check that the bots can walk each dungeon boss to boss on the navmesh. Pure Detour API (no TC deps).

add_executable(dungeon_reach_check
  dungeon_reach_check.cpp)

target_link_libraries(dungeon_reach_check
  PRIVATE
    Detour)

set_target_properties(dungeon_reach_check
  PROPERTIES
    FOLDER "tools")

if(UNIX)
  install(TARGETS dungeon_reach_check DESTINATION bin)
elseif(WIN32)
  install(TARGETS dungeon_reach_check DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
