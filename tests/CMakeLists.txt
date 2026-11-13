# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

option(HEADLESS_TESTS "Build the headless packet-level client tests (tests/headless), run against a test realm" 0)

add_executable(tests)
# The source folder, for tests that read source files (the compiler trims __FILE__ in release builds, so it cannot be derived from there).
target_compile_definitions(tests PRIVATE TC_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

CollectAndAddSourceFiles(
  tests
  ${CMAKE_CURRENT_SOURCE_DIR}
  EXCLUDE
  ${CMAKE_CURRENT_SOURCE_DIR}/tools
  ${CMAKE_CURRENT_SOURCE_DIR}/headless)

# Tests of code that lives in the Playerbot V2 module's own sources (they call functions defined in the module's .cpp files, or include
# headers that exist only under src/modules/PlayerbotV2). A build without the module has neither, so they are not compiled there.
if(NOT BUILD_PLAYERBOT_V2)
  set(TESTS_MODULE_ONLY_SOURCES
    ClassicAddonHint.cpp
    ClassicTalentCode.cpp
    ClassicBotBridge.cpp
    ClassicBotConversation.cpp
    ClassicGuildAskText.cpp
    ClassicBotLocale.cpp
    ClassicCompanionLeash.cpp
    ClassicMeleeReach.cpp
    ClassicFalseCombatEscape.cpp
    ClassicFollowDrop.cpp
    ClassicBotSwim.cpp
    ClassicGroupControl.cpp
    ClassicGroupFight.cpp
    ClassicGroupOrderText.cpp
    ClassicCcDeny.cpp
    ClassicOwnerPicks.cpp
    ClassicAoeDeny.cpp
    ClassicCompanionDelete.cpp
    ClassicHumanLogin.cpp
    ClassicInstanceFullHold.cpp
    ClassicArmorSwapGate.cpp
    ClassicBotCustomizationRoll.cpp
    ClassicBotListedGroupPlan.cpp
    ClassicLfgRoleGap.cpp
    ClassicDungeonFinderPlan.cpp
    ClassicRaidConsumables.cpp
    ClassicRaidDispel.cpp
    ClassicRaidLoadout.cpp
    ClassicBisPools.cpp
    ClassicRaidFightScript.cpp
    ClassicRaidGeometry.cpp
    ClassicRaidRagnaros.cpp
    ClassicRaidResistPotions.cpp
    ClassicDrinkSay.cpp
    ClassicReactionTiming.cpp
    ClassicReactionWake.cpp
    ClassicBotSize.cpp
    ClassicPlacement.cpp
    ClassicWorldPvp.cpp)
  foreach(_test_source ${TESTS_MODULE_ONLY_SOURCES})
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/game/${_test_source} PROPERTIES HEADER_FILE_ONLY TRUE)
  endforeach()
endif()

target_link_libraries(tests
  PRIVATE
    trinity-core-interface
    game
    Catch2::Catch2WithMain)

target_include_directories(tests
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

# game.lib calls into the Playerbot V2 module (PlayerbotHooks), so a build with the module needs it on the test link line too,
# exactly as worldserver does (src/server/worldserver/CMakeLists.txt).
if(BUILD_PLAYERBOT_V2)
  target_compile_definitions(tests PRIVATE TESTS_HAVE_PLAYERBOT_V2)
  target_link_libraries(tests PRIVATE playerbot-v2)
endif()

# game.lib uses extractor_common (Road corridors) when the tools are built; it reaches the test link line through game.
# Tests that use it directly (tests/tools) have their own executable below.

# Windows: the test executable loads libmysql/OpenSSL at start, and catch_discover_tests runs it (at ctest time, DISCOVERY_MODE PRE_TEST,
# so the list always matches the exe that is tested) to list the
# tests, so the directories those DLLs live in must be on PATH for the discovery and for the test run. The DLLs sit next to their
# import libraries (or in ../bin for OpenSSL) and, for a deployed build, in the install prefix.
set(TESTS_DL_PATHS)
if(WIN32)
  set(_tests_dll_dirs ${CMAKE_INSTALL_PREFIX})
  foreach(_lib ${MYSQL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY} ${OPENSSL_SSL_LIBRARY})
    if(EXISTS "${_lib}")
      get_filename_component(_dir "${_lib}" DIRECTORY)
      list(APPEND _tests_dll_dirs "${_dir}" "${_dir}/../bin")
    endif()
  endforeach()
  if(OPENSSL_ROOT_DIR)
    list(APPEND _tests_dll_dirs "${OPENSSL_ROOT_DIR}/bin")
  endif()
  foreach(_dir ${_tests_dll_dirs})
    if(IS_DIRECTORY "${_dir}")
      get_filename_component(_dir "${_dir}" ABSOLUTE)
      list(APPEND TESTS_DL_PATHS "${_dir}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES TESTS_DL_PATHS)
endif()

if(TESTS_DL_PATHS)
  catch_discover_tests(tests DISCOVERY_MODE PRE_TEST DL_PATHS ${TESTS_DL_PATHS})
else()
  catch_discover_tests(tests DISCOVERY_MODE PRE_TEST)
endif()

set_target_properties(tests
    PROPERTIES
      FOLDER
        "tests")

# Tools-layer tests live in their own executable so pre-existing breakage in
# tests/game/ doesn't block tooling tests from linking. They need extractor_common, so they exist only with -DTOOLS=1.
if(TARGET extractor_common)
  add_subdirectory(tools)
else()
  message(STATUS "tests: tools_tests skipped (TOOLS is off, no extractor_common)")
endif()

if(HEADLESS_TESTS)
  add_subdirectory(headless)
endif()
