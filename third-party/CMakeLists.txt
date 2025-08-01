# Third-party Dependencies CMake Configuration

# =============================================================================
# LUA SCRIPTING ENGINE
# =============================================================================

if(KRYON_SCRIPT_LUA)
    message(STATUS "Building Lua scripting engine")
    
    # Lua source files (we'll embed Lua directly)
    set(LUA_SOURCES
        lua/lapi.c lua/lcode.c lua/lctype.c lua/ldebug.c lua/ldo.c lua/ldump.c
        lua/lfunc.c lua/lgc.c lua/llex.c lua/lmem.c lua/lobject.c lua/lopcodes.c
        lua/lparser.c lua/lstate.c lua/lstring.c lua/ltable.c lua/ltm.c
        lua/lundump.c lua/lvm.c lua/lzio.c lua/lauxlib.c lua/lbaselib.c
        lua/lcorolib.c lua/ldblib.c lua/liolib.c lua/lmathlib.c lua/loslib.c
        lua/lstrlib.c lua/ltablib.c lua/lutf8lib.c lua/loadlib.c lua/linit.c
    )
    
    add_library(lua STATIC ${LUA_SOURCES})
    target_include_directories(lua PUBLIC lua/)
    target_compile_definitions(lua PRIVATE LUA_COMPAT_5_3)
    
    # Platform-specific configurations
    if(UNIX)
        target_compile_definitions(lua PRIVATE LUA_USE_POSIX)
    endif()
    
    if(LINUX)
        target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
        target_link_libraries(lua m dl)
    endif()
endif()

# =============================================================================
# QUICKJS JAVASCRIPT ENGINE
# =============================================================================

if(KRYON_SCRIPT_JAVASCRIPT)
    message(STATUS "Building QuickJS JavaScript engine")
    
    # QuickJS configuration
    add_library(quickjs STATIC
        quickjs/quickjs.c
        quickjs/libregexp.c
        quickjs/libunicode.c
        quickjs/cutils.c
        quickjs/quickjs-libc.c
    )
    
    target_include_directories(quickjs PUBLIC quickjs/)
    target_compile_definitions(quickjs PRIVATE 
        _GNU_SOURCE
        CONFIG_VERSION="2023-12-09"
    )
    
    # Platform-specific settings
    if(WIN32)
        target_compile_definitions(quickjs PRIVATE _WIN32_WINNT=0x0600)
    endif()
endif()

# =============================================================================
# STB SINGLE-FILE LIBRARIES
# =============================================================================

message(STATUS "Building STB libraries")

# Create STB implementation file
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/stb_impl.c
"#define STB_IMAGE_IMPLEMENTATION
#include \"stb_image.h\"
#define STB_IMAGE_WRITE_IMPLEMENTATION  
#include \"stb_image_write.h\"
")

add_library(stb STATIC ${CMAKE_CURRENT_BINARY_DIR}/stb_impl.c)
target_include_directories(stb PUBLIC stb/)

# =============================================================================
# RAYLIB GRAPHICS LIBRARY
# =============================================================================

if(KRYON_RENDERER_RAYLIB)
    message(STATUS "Building Raylib renderer")
    
    # Raylib configuration
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(raylib)
    
    # Alias for consistency
    add_library(raylib_static ALIAS raylib)
endif()

# =============================================================================
# FREETYPE FONT RENDERING
# =============================================================================

if(KRYON_ENABLE_FONTS)
    message(STATUS "Building FreeType font library")
    
    # FreeType configuration
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    
    add_subdirectory(freetype)
endif()

# =============================================================================
# CJSON LIBRARY
# =============================================================================

message(STATUS "Building cJSON library")

add_library(cjson STATIC
    cjson/cJSON.c
    cjson/cJSON_Utils.c
)

target_include_directories(cjson PUBLIC cjson/)
target_compile_definitions(cjson PRIVATE CJSON_HIDE_SYMBOLS)

# =============================================================================
# CURL HTTP CLIENT (Optional)
# =============================================================================

if(KRYON_ENABLE_NETWORK AND NOT EMSCRIPTEN)
    message(STATUS "Building libcurl")
    
    # Curl configuration - minimal build
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_STATICLIB ON CACHE BOOL "" FORCE)
    set(HTTP_ONLY ON CACHE BOOL "" FORCE)
    set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(curl)
endif()

# =============================================================================
# UNITY TESTING FRAMEWORK
# =============================================================================

if(KRYON_BUILD_TESTS)
    message(STATUS "Building Unity test framework")
    
    add_library(unity STATIC
        unity/src/unity.c
    )
    
    target_include_directories(unity PUBLIC unity/src/)
    target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
endif()

# =============================================================================
# CMOCKA TESTING FRAMEWORK (Alternative)
# =============================================================================

if(KRYON_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmocka)
    message(STATUS "Building CMocka test framework")
    
    set(WITH_STATIC_LIB ON CACHE BOOL "" FORCE)
    set(WITH_CMOCKERY_SUPPORT OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(cmocka)
endif()

# =============================================================================
# CMAKE MODULES
# =============================================================================

# Add CMake modules to module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules)

# =============================================================================
# SUMMARY
# =============================================================================

message(STATUS "")
message(STATUS "=== Third-party Dependencies Summary ===")
message(STATUS "Lua: ${KRYON_SCRIPT_LUA}")
message(STATUS "QuickJS: ${KRYON_SCRIPT_JAVASCRIPT}")
message(STATUS "STB Libraries: ON")
message(STATUS "Raylib: ${KRYON_RENDERER_RAYLIB}")
message(STATUS "FreeType: ${KRYON_ENABLE_FONTS}")
message(STATUS "cJSON: ON")
message(STATUS "libcurl: ${KRYON_ENABLE_NETWORK}")
message(STATUS "Unity Testing: ${KRYON_BUILD_TESTS}")
message(STATUS "==========================================")
message(STATUS "")