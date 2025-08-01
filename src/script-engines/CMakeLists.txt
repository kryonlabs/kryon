# Kryon Script Engines Module

set(SCRIPT_SOURCES
    common/script_interface.c
    bridge/dom_bridge.c
)

if(KRYON_SCRIPT_LUA)
    list(APPEND SCRIPT_SOURCES 
        lua/lua_engine.c 
        lua/lua_bindings.c
        lua/lua_widget_api.c  # New widget transformation API
    )
endif()

if(KRYON_SCRIPT_JAVASCRIPT)
    list(APPEND SCRIPT_SOURCES javascript/js_engine.c javascript/js_bindings.c)
endif()

add_library(kryon_script_engines OBJECT ${SCRIPT_SOURCES})

target_include_directories(kryon_script_engines PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/internal
)

target_compile_definitions(kryon_script_engines PRIVATE KRYON_SCRIPT_ENGINES_MODULE=1)