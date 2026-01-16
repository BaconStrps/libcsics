
set(SOURCES RadioRx.cpp Radio.cpp)
set(LIBRARIES queue)
set(DEFINITIONS)

set(COMPILE_OPTIONS
    $<$<IN_LIST:$<CXX_COMPILER_ID>,GNU;Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

if (CSICS_USE_UHD)
    set(SOURCES ${SOURCES} usrp/USRPRadioRx.cpp)
    set(LIBRARIES ${LIBRARIES} uhd)
    set(DEFINITIONS ${DEFINITIONS} CSICS_USE_UHD)
endif()

add_library(rx STATIC ${SOURCES})
target_include_directories(rx PUBLIC ${INCLUDE_DIR})
target_link_libraries(rx PUBLIC ${LIBRARIES})
target_compile_options(rx PRIVATE ${COMPILE_OPTIONS})
target_compile_definitions(rx PUBLIC ${DEFINITIONS})

add_library(radio INTERFACE)
target_include_directories(radio INTERFACE ${INCLUDE_DIR})
target_link_libraries(radio INTERFACE rx)
message(DEBUG "Radio module sources: ${SOURCES}")
message(DEBUG "Radio module libraries: ${LIBRARIES}")
message(DEBUG "Radio module definitions: ${DEFINITIONS}")
    
