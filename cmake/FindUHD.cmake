
if (MSVC)

    message(STATUS "Using vendored UHD.")
    set(UHD_ROOT ${CMAKE_BINARY_DIR}/vendor/msvc/uhd)
    file(
        ARCHIVE_EXTRACT
        INPUT ${CMAKE_SOURCE_DIR}/vendor/msvc/uhd_4.9.0.0.zip
        DESTINATION ${UHD_ROOT}
    ) 
    
    find_package(Boost)
    if (Boost_FOUND)
        set(UHD_LIBRARIES ${UHD_ROOT}/lib/uhd.lib ${Boost_LIBRARY_DIR})
        set(UHD_INCLUDE_DIRS ${UHD_ROOT}/include ${Boost_INCLUDE_DIR})
        set(UHD_FOUND TRUE)
    else()
        message(WARNING "Boost not installed. Please install Boost to use UHD.")
        set(UHD_FOUND FALSE)
    endif()
endif()
