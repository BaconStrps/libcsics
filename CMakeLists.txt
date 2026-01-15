cmake_minimum_required(VERSION 3.10)
project(csics VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()


option(CSICS_BUILD_QUEUE "Build the queue module" ON)
option(CSICS_BUILD_RADIO "Build the radio module" ON)
option(CSICS_USE_UHD "Use the UHD library for USRP support" ON)

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(COMPONENTS CSICS::core)

if (CSICS_USE_UHD)
    message(STATUS "UHD support enabled")
    include(cmake/uhd.cmake)
endif()

if (CSICS_BUILD_QUEUE)
    list(APPEND COMPONENTS CSICS::queue)
endif()
if (CSICS_BUILD_RADIO)
    list(APPEND COMPONENTS CSICS::radio)
endif()

add_subdirectory(src)

add_library(CSICS INTERFACE)
target_include_directories(CSICS INTERFACE ${INCLUDE_DIR})
target_link_libraries(CSICS INTERFACE ${COMPONENTS})

message(STATUS "CSICS build configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Components: ${COMPONENTS}")
message(STATUS "  UHD support: ${CSICS_USE_UHD}")

