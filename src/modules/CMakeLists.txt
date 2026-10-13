# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# Optional modules: every folder here with a CMakeLists.txt is added. A module decides itself whether it is built
# (its own option) and registers with
#   trinity_add_module(<library> <script loader function> [CONFIGS <conf.dist files>])
# The loader function registers the module's scripts (WorldScript, PlayerScript, ...) like an AddSC_ function.
# worldserver links the `modules` library: its AddModuleScripts() calls every registered loader and is empty when
# no module is built. CONFIGS are copied to worldserver.conf.d/.

function(trinity_add_module library loader)
  cmake_parse_arguments(MODULE "" "" "CONFIGS" ${ARGN})
  set_property(GLOBAL APPEND PROPERTY TRINITY_MODULE_LIBRARIES ${library})
  set_property(GLOBAL APPEND PROPERTY TRINITY_MODULE_LOADERS ${loader})
  set_property(GLOBAL APPEND PROPERTY TRINITY_MODULE_CONFIGS ${MODULE_CONFIGS})
endfunction()

file(GLOB module_dirs LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(module_dir ${module_dirs})
  if(IS_DIRECTORY ${module_dir} AND EXISTS ${module_dir}/CMakeLists.txt)
    add_subdirectory(${module_dir})
  endif()
endforeach()

get_property(module_libraries GLOBAL PROPERTY TRINITY_MODULE_LIBRARIES)
get_property(module_loaders GLOBAL PROPERTY TRINITY_MODULE_LOADERS)

set(TRINITY_MODULE_LOADER_DECLARATIONS "")
set(TRINITY_MODULE_LOADER_CALLS "")
foreach(loader ${module_loaders})
  string(APPEND TRINITY_MODULE_LOADER_DECLARATIONS "void ${loader}();\n")
  string(APPEND TRINITY_MODULE_LOADER_CALLS "    ${loader}();\n")
endforeach()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ModuleLoader.cpp.in.cmake ${CMAKE_CURRENT_BINARY_DIR}/ModuleLoader.cpp)

add_library(modules STATIC
  ${CMAKE_CURRENT_BINARY_DIR}/ModuleLoader.cpp)

target_link_libraries(modules
  PRIVATE
    trinity-core-interface
  PUBLIC
    ${module_libraries})

set_target_properties(modules
  PROPERTIES
    FOLDER "server")

if(module_loaders)
  message(STATUS "Modules: ${module_loaders}")
endif()
