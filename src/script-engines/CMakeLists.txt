# Kryon Script Engines Module
cmake_minimum_required(VERSION 3.10)

# Find Lua
find_package(PkgConfig REQUIRED)
pkg_check_modules(LUA QUIET lua5.4)

if(NOT LUA_FOUND)
    pkg_check_modules(LUA QUIET lua)
endif()

if(LUA_FOUND)
    message(STATUS "Found Lua: ${LUA_VERSION}")
    set(KRYON_SCRIPT_LUA ON)
    add_compile_definitions(HAVE_LUA)
else()
    message(WARNING "Lua not found - Lua scripting will be disabled")
    set(KRYON_SCRIPT_LUA OFF)
endif()

# Core VM abstraction (always included)
set(SCRIPT_SOURCES
    vm_core.c
)

# Add backend implementations
if(KRYON_SCRIPT_LUA)
    list(APPEND SCRIPT_SOURCES 
        lua/lua_vm_backend.c
        lua/lua_widget_api.c
    )
endif()

# Always build the library
add_library(kryon_script_engines STATIC ${SCRIPT_SOURCES})

target_include_directories(kryon_script_engines PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

if(KRYON_SCRIPT_LUA AND LUA_FOUND)
    target_include_directories(kryon_script_engines PRIVATE ${LUA_INCLUDE_DIRS})
    target_link_libraries(kryon_script_engines ${LUA_LIBRARIES})
    target_compile_options(kryon_script_engines PRIVATE ${LUA_CFLAGS_OTHER})
endif()

target_link_libraries(kryon_script_engines kryon_minimal_memory)