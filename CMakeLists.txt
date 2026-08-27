cmake_minimum_required(VERSION 3.10)
project(csics VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CSICS_DEV "Enable development mode with debug settings" OFF)

if (CSICS_DEV)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
else()
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()


option(CSICS_BUILD_ALL "Build all modules" ON)
option(CSICS_BUILD_QUEUE "Build the queue module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_RADIO "Build the radio module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_IO "Build the IO module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_SERIALIZATION "Build the serialization module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_LINALG "Build the linear algebra module" ${CSICS_BUILD_ALL}) 
option(CSICS_BUILD_GEO "Build the geodesic module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_LVC "Build the LVC module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_SIM "Build the simulation module" ${CSICS_BUILD_ALL})
option(CSICS_BUILD_EM "Build the electromagnetics module" ${CSICS_BUILD_ALL})
option(CSICS_USE_UHD "Use the UHD library for USRP support" ${CSICS_BUILD_RADIO})
option(CSICS_USE_ZSTD "Use the ZSTD library for compression support" ${CSICS_BUILD_IO})
option(CSICS_USE_ZLIB "Use the ZLIB library for compression support" ${CSICS_BUILD_IO})
option(CSICS_USE_MQTT "Use the MQTT library for messaging support" ${CSICS_BUILD_IO})
option(CSICS_ENABLE_TESTS "Enable building tests" ${CSICS_BUILD_ALL})

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(CSICS_COMPILE_DEFINITIONS
        $<$<BOOL:${CSICS_DEV}>:CSICS_DEV>
        $<$<CONFIG:Debug>:CSICS_DEBUG>
        $<$<BOOL:${CSICS_BUILD_QUEUE}>:CSICS_BUILD_QUEUE>
        $<$<BOOL:${CSICS_BUILD_RADIO}>:CSICS_BUILD_RADIO>
        $<$<BOOL:${CSICS_BUILD_IO}>:CSICS_BUILD_IO>
        $<$<BOOL:${CSICS_BUILD_SERIALIZATION}>:CSICS_BUILD_SERIALIZATION>
        $<$<BOOL:${CSICS_BUILD_LINALG}>:CSICS_BUILD_LINALG>
        $<$<BOOL:${CSICS_BUILD_GEO}>:CSICS_BUILD_GEO>
        $<$<BOOL:${CSICS_BUILD_LVC}>:CSICS_BUILD_LVC>
        $<$<BOOL:${CSICS_BUILD_SIM}>:CSICS_BUILD_SIM>
        $<$<BOOL:${CSICS_BUILD_EM}>:CSICS_BUILD_EM>
        $<$<BOOL:${CSICS_USE_UHD}>:CSICS_USE_UHD>
        $<$<BOOL:${CSICS_USE_ZSTD}>:CSICS_USE_ZSTD>
        $<$<BOOL:${CSICS_USE_ZLIB}>:CSICS_USE_ZLIB>
        $<$<BOOL:${CSICS_USE_MQTT}>:CSICS_USE_MQTT>
)

include(cmake/component_requirements.cmake)
include(cmake/deps.cmake)

set(COMPONENTS CSICS::core)

set(CSICS_COMPILE_FLAGS
    $<$<IN_LIST:$<CXX_COMPILER_ID>,GNU;Clang>:-Wall -Wextra -Wpedantic -Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<AND:$<CONFIG:Debug>,$<IN_LIST:$<CXX_COMPILER_ID>,GNU;Clang>>: -g -O1 -fsanitize=address,undefined,leak -fsanitize-address-use-after-scope -fno-omit-frame-pointer>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/fsanitize=address,undefined>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:AppleClang>>: -g -O0>
)

set(CSICS_LINKER_FLAGS
    $<$<AND:$<CONFIG:Debug>,$<IN_LIST:$<CXX_COMPILER_ID>,GNU;Clang>>: -fsanitize=address,undefined,leak>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/fsanitize=address>
)

set(CSICS_DEBUG_LIBS
    $<$<AND:$<CONFIG:Debug>,$<IN_LIST:$<CXX_COMPILER_ID>,GNU;Clang>>: -fsanitize=address,undefined,leak>
    $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:MSVC>>:/fsanitize=address>
)

message(DEBUG "CSICS compile flags: ${CSICS_COMPILE_FLAGS}")

if (CSICS_BUILD_QUEUE)
    list(APPEND COMPONENTS CSICS::queue)
endif()

if (CSICS_BUILD_RADIO)
    list(APPEND COMPONENTS CSICS::radio)
endif()

if (CSICS_BUILD_IO)
    list(APPEND COMPONENTS CSICS::io)
endif()

if (CSICS_BUILD_SERIALIZATION)
    list(APPEND COMPONENTS CSICS::serialization)
endif()

if (CSICS_BUILD_GEO)
    list(APPEND COMPONENTS CSICS::geo)
endif()

if (CSICS_DEV)
    add_library(_include_anchors OBJECT include/csics/_dev_anchor.cpp)
    target_compile_definitions(_include_anchors PUBLIC ${CSICS_COMPILE_DEFINITIONS})
    target_compile_options(_include_anchors PUBLIC ${CSICS_COMPILE_FLAGS})
    target_link_options(_include_anchors PUBLIC ${CSICS_LINKER_FLAGS})
    target_include_directories(_include_anchors PUBLIC ${INCLUDE_DIR})
endif()

add_subdirectory(src)

add_library(CSICS INTERFACE)
target_include_directories(CSICS INTERFACE ${INCLUDE_DIR})
target_link_libraries(CSICS INTERFACE ${COMPONENTS} ${CSICS_DEBUG_LIBS})
target_compile_options(CSICS INTERFACE ${CSICS_COMPILE_FLAGS})
target_compile_definitions(CSICS INTERFACE ${CSICS_COMPILE_DEFINITIONS})
target_link_options(CSICS INTERFACE ${CSICS_LINKER_FLAGS})


message(STATUS "CSICS build configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Components: ${COMPONENTS}")
message(STATUS "  UHD support: ${CSICS_USE_UHD}")

if (CSICS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
