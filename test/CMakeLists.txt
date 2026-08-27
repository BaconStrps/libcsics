
include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(TEST_CSICS ALIAS CSICS)

add_library(test_utils OBJECT test_utils.cpp io/compression_utils.cpp)

set(TESTS)
set(LIBS)

if (CSICS_BUILD_QUEUE)
    list(APPEND TESTS queue/spsc_queue_test.cpp)
    list(APPEND TESTS queue/spsc_message_queue_test.cpp)
endif()

if (CSICS_BUILD_IO)
    if (CSICS_USE_ZSTD)
        list(APPEND TESTS io/zstd_compression_test.cpp)
    endif()
    if (CSICS_USE_ZLIB)
        list(APPEND TESTS io/zlib_compression_test.cpp)
    endif()
    find_package(OpenSSL)
    if (OPENSSL_FOUND)
        list(APPEND TESTS io/base64_encoding_test.cpp)
        list(APPEND LIBS OpenSSL::Crypto)
    else()
        find_library(CRYPTO_LIB crypto)
        if (CRYPTO_LIB)
            list(APPEND TESTS io/base64_encoding_test.cpp)
            list(APPEND LIBS ${CRYPTO_LIB})
        endif()
    endif()
    if (CSICS_USE_MQTT)
        list(APPEND TESTS io/mqtt_test.cpp)
    endif()
endif()

if (CSICS_BUILD_RADIO)
    if(CSICS_USE_UHD)
        list(APPEND TESTS radio/uhd_test.cpp)
    endif()
endif()

if (CSICS_BUILD_SERIALIZATION)
    list(APPEND TESTS serialization/json_serialization_test.cpp)
endif()

if (CSICS_BUILD_EM)
    list(APPEND TESTS em/em_test.cpp)
endif()

if (CSICS_BUILD_LINALG)
    list(APPEND TESTS linalg/vec_lib_test.cpp)
    list(APPEND TESTS linalg/matrix_test.cpp)
endif()

if (CSICS_BUILD_GEO)
    list(APPEND TESTS geo/geo_basic_tests.cpp)
endif()

if (CSICS_BUILD_LVC)
    list(APPEND TESTS lvc/dis6_test.cpp)
    list(APPEND TESTS lvc/dis7_test.cpp)
endif()

if (CSICS_BUILD_SIM)
    list(APPEND TESTS sim/ecs_test.cpp)
endif()

add_executable(tests ${TESTS})
target_link_libraries(tests PRIVATE GTest::gtest_main GTest::gmock_main CSICS test_utils ${LIBS})

target_compile_options(tests PRIVATE ${CSICS_COMPILE_FLAGS})
target_link_options(tests PRIVATE ${CSICS_LINKER_FLAGS})
if (CMAKE_GENERATOR MATCHES "Xcode")
    set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_BUILD)
    add_test(NAME csics_tests COMMAND tests)
else()
include(GoogleTest)
gtest_discover_tests(tests)
endif()
message(STATUS "Available tests: ${TESTS}")

