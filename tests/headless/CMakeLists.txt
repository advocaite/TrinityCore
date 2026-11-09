# Headless packet-level client tests (see doc/headless-test-client-plan.md). They need a running test realm, so they are not part of
# the `tests` executable and not registered with ctest: build with -DHEADLESS_TESTS=1, run scripts/run-headless.ps1 (or the executable
# with HEADLESS_* environment variables set); without them every scenario is skipped.

add_executable(headless_tests
  HeadlessClient.cpp
  HeadlessClient.h
  HeadlessScenarios.cpp)

target_link_libraries(headless_tests
  PRIVATE
    trinity-core-interface
    game
    Catch2::Catch2WithMain)

target_include_directories(headless_tests
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_SOURCE_DIR}/tests/common
    ${CMAKE_SOURCE_DIR}/tests/game/data)

set_target_properties(headless_tests PROPERTIES FOLDER "tests")
