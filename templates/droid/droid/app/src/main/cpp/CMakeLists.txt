cmake_minimum_required(VERSION 3.22)
project(__NAME__ C)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
if(NOT DEFINED KRYON_DIR OR NOT DEFINED ZIRAN_DIR OR NOT DEFINED RAYLIB_DIR)
    message(FATAL_ERROR
        "kryon build --profile android passes -DKRYON_DIR, -DZIRAN_DIR and "
        "-DRAYLIB_DIR; a manual gradle run passes them as -P properties")
endif()
get_filename_component(KRYON_DIR "${KRYON_DIR}" ABSOLUTE BASE_DIR "${REPO_ROOT}")
get_filename_component(ZIRAN_DIR "${ZIRAN_DIR}" ABSOLUTE BASE_DIR "${REPO_ROOT}")
get_filename_component(RAYLIB_DIR "${RAYLIB_DIR}" ABSOLUTE BASE_DIR "${REPO_ROOT}")
include("${KRYON_DIR}/cmake/KryonAndroid.cmake")

find_program(HOST_PYTHON NAMES python3 REQUIRED)

set(ZIRAN_BIN "${ZIRAN_DIR}/build/bin/ziran")
set(ZI2C_BIN "${ZIRAN_DIR}/build/bin/zi2c")
if(NOT EXISTS "${ZIRAN_BIN}" OR NOT EXISTS "${ZI2C_BIN}")
    message(FATAL_ERROR
        "The pinned Ziran toolchain in ${ZIRAN_DIR} is not built; "
        "run kryon build --profile android, which builds it first")
endif()

set(APP_ZIRAN_DEFINES --define ANDROID_BUILD)
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(APP_ZIRAN_ARCH __aarch64__)
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(APP_ZIRAN_ARCH __arm__)
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(APP_ZIRAN_ARCH __x86_64__)
elseif(ANDROID_ABI STREQUAL "x86")
    set(APP_ZIRAN_ARCH __i386__)
else()
    message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
endif()
list(APPEND APP_ZIRAN_DEFINES --define ${APP_ZIRAN_ARCH})

set(APP_ZIRAN_IR_DIR "${CMAKE_BINARY_DIR}/ziran/ir")
set(APP_ZIRAN_GEN_DIR "${CMAKE_BINARY_DIR}/ziran/generated")
set(APP_ZIRAN_MANIFEST "${CMAKE_BINARY_DIR}/ziran/generated-sources.cmake")
file(GLOB_RECURSE APP_ZIRAN_SOURCES CONFIGURE_DEPENDS "${REPO_ROOT}/src/*.zi")
file(GLOB_RECURSE APP_LIBRARY_ZIRAN_SOURCES CONFIGURE_DEPENDS
    "${KRYON_DIR}/src/ui/*.zi" "${KRYON_DIR}/src/backend/*.zi"
    "${ZIRAN_DIR}/std/*.zi")
execute_process(COMMAND "${HOST_PYTHON}"
    "${KRYON_DIR}/scripts/generate_android_sources.py"
    --ziran "${ZIRAN_BIN}" --zi2c "${ZI2C_BIN}"
    --root "${REPO_ROOT}" --kryon "${KRYON_DIR}"
    --ir "${APP_ZIRAN_IR_DIR}" --output "${APP_ZIRAN_GEN_DIR}"
    --manifest "${APP_ZIRAN_MANIFEST}" ${APP_ZIRAN_DEFINES}
    WORKING_DIRECTORY "${REPO_ROOT}" COMMAND_ERROR_IS_FATAL ANY)
include("${APP_ZIRAN_MANIFEST}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${APP_ZIRAN_SOURCES} ${APP_LIBRARY_ZIRAN_SOURCES})

set(APP_BUNDLE_RUNTIME_DIR "${CMAKE_BINARY_DIR}/bundle-runtime")
execute_process(COMMAND "${HOST_PYTHON}"
    "${KRYON_DIR}/scripts/build_android_runtime.py"
    --source "${ZIRAN_DIR}" --output "${APP_BUNDLE_RUNTIME_DIR}"
    --cc "${CMAKE_C_COMPILER} --target=${CMAKE_C_COMPILER_TARGET} --sysroot=${CMAKE_SYSROOT}"
    --ar "${CMAKE_AR}" --objcopy "${CMAKE_OBJCOPY}"
    "--flags=-O2 -fPIC"
    COMMAND_ERROR_IS_FATAL ANY)

kryon_android_raylib("${RAYLIB_DIR}")

add_library(main SHARED ${APP_ZIRAN_GENERATED_SOURCES})
kryon_android_app(main)
kryon_android_host(main ${APP_ZIRAN_GENERATED_SOURCES})

target_include_directories(main PRIVATE "${ZIRAN_DIR}/include")
# Generated module names such as math.h must not shadow libc's <math.h> in
# raylib and other native dependencies. Zi imports use quoted includes.
foreach(directory ${APP_ZIRAN_INCLUDE_DIRS})
    target_compile_options(main PRIVATE "-iquote${directory}")
endforeach()

target_link_libraries(main z "${APP_BUNDLE_RUNTIME_DIR}/libziran.a")
