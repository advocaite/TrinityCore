# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Spell test module: test characters without a client, and a log of what they cast, deal, gain and proc, driven with GM
# commands (usually over the SOAP console). See doc/Docker.md.
option(BUILD_SPELL_TEST "Build the spell test module (test characters without a client, and a spell event log)" 0)
if(NOT BUILD_SPELL_TEST)
  return()
endif()

add_library(spell-test STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/cs_spell_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SpellTestSession.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/SpellTestSession.h)

# game, not only game-interface: worldserver links the modules after game
target_link_libraries(spell-test
  PRIVATE
    trinity-core-interface
  PUBLIC
    game)

set_target_properties(spell-test
  PROPERTIES
    FOLDER "server")

trinity_add_module(spell-test AddSC_spell_test)
