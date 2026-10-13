# Optional modules

A module is a folder in `src/modules/` with its own `CMakeLists.txt`. It is built into the worldserver without changing
core files: the core only offers ScriptMgr hooks, and the module registers scripts for the ones it needs. With no module
folder (or every module switched off), `AddModuleScripts()` is empty and the server is the same as without this system.

## Layout

```
src/modules/MyModule/
    CMakeLists.txt
    MyModuleScripts.cpp     # AddSC_my_module(): registers the module's scripts
    conf/my_module.conf.dist
```

## CMakeLists.txt

```cmake
option(BUILD_MY_MODULE "Build the MyModule module" 0)
if(NOT BUILD_MY_MODULE)
  return()
endif()

CollectSourceFiles(${CMAKE_CURRENT_SOURCE_DIR} PRIVATE_SOURCES)
add_library(my-module STATIC ${PRIVATE_SOURCES})
target_link_libraries(my-module
  PRIVATE
    trinity-core-interface
  PUBLIC
    game-interface)

trinity_add_module(my-module AddSC_my_module
  CONFIGS
    ${CMAKE_CURRENT_SOURCE_DIR}/conf/my_module.conf.dist)
```

`trinity_add_module(<library> <loader> [CONFIGS <files>])` links the library into the worldserver, calls the loader
function from `AddModuleScripts()` (after the core scripts) and copies the config files to `worldserver.conf.d/`. Like
`worldserver.conf.dist`, a copied `.conf.dist` is only read after it is renamed to `.conf`; the file must start with a
section header such as `[my_module]`.

## Scripts

The loader registers scripts the same way as an `AddSC_` function in `src/server/scripts`:

```cpp
class my_module_world : public WorldScript
{
public:
    my_module_world() : WorldScript("my_module_world") { }
    void OnWorldInitialized() override { /* load data, before the realm is online */ }
    void OnBeforeMapsUpdate(uint32 diff) override { /* every world tick, before the maps update */ }
};

void AddSC_my_module()
{
    new my_module_world();
}
```

Hooks marked "map thread" in `ScriptMgr.h` can run on several map update threads at once; a module that keeps shared
state in them has to lock it.

## Database

Modules apply their own SQL (for example a migration step in `OnWorldInitialized`, or files the server owner applies by
hand); the core database updater only handles `sql/updates` and `sql/custom`.
