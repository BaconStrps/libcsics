
set(
    SOURCES 
    RadioRx.cpp 
    Radio.cpp
)
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

add_library(radio STATIC ${SOURCES})
target_include_directories(radio PUBLIC ${INCLUDE_DIR})
target_link_libraries(radio PUBLIC ${LIBRARIES})
target_compile_options(radio PRIVATE ${COMPILE_OPTIONS})
target_compile_definitions(radio PUBLIC ${DEFINITIONS})

message(DEBUG "Radio module sources: ${SOURCES}")
message(DEBUG "Radio module libraries: ${LIBRARIES}")
message(DEBUG "Radio module definitions: ${DEFINITIONS}")
    
