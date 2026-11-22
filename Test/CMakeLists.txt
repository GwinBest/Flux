include(FetchContent)

set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

FetchContent_Declare(googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        SYSTEM
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(googletest)

if (MSVC AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GoogleTest 1.17 enables /WX internally. Clang 21+ diagnoses its char8_t
    # printer; keep that warning non-fatal only while building GoogleTest.
    # https://github.com/google/googletest/issues/4948
    check_cxx_compiler_flag("-Wno-error=character-conversion" GTEST_HAS_NO_ERROR_CHARACTER_CONVERSION)
    if (GTEST_HAS_NO_ERROR_CHARACTER_CONVERSION)
        target_compile_options(gtest PRIVATE -Wno-error=character-conversion)
        target_compile_options(gtest_main PRIVATE -Wno-error=character-conversion)
    endif ()
endif ()

add_executable(${PROJECT_NAME}-Tests
        Server/ServerTest.cpp
        Networking/AcceptorTest.cpp
        Networking/ConnectionManagerTest.cpp
        Networking/ConnectionTest.cpp
        Networking/ConnectionSendQueueTest.cpp
        Networking/ConnectionRequestSizeTest.cpp
        Networking/ConnectionIdleTimeoutTest.cpp
        Networking/IoContextPoolTest.cpp
        Networking/KeepAliveTest.cpp
        Protocol/Http/HttpSessionTest.cpp
        Protocol/Http/HttpRouterTest.cpp
        Protocol/Http/HttpServerTest.cpp
        Protocol/Http/HttpFieldTest.cpp
        Protocol/Http/HttpMethodTest.cpp
        Protocol/Http/HttpStatusTest.cpp
        Protocol/Http/HttpRequestTest.cpp
        Protocol/Http/HttpResponseTest.cpp
        Utils/AsyncTest.cpp
        Utils/ErrorTest.cpp
        Utils/LogTest.cpp
        Utils/SystemResourcesTest.cpp
)

target_include_directories(${PROJECT_NAME}-Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(${PROJECT_NAME}-Tests PRIVATE ${PROJECT_NAME} GTest::gtest_main spdlog::spdlog)

enable_sanitizers(${PROJECT_NAME}-Tests)
enable_coverage(${PROJECT_NAME}-Tests)
enable_project_warnings(${PROJECT_NAME}-Tests)

include(GoogleTest)

gtest_discover_tests(${PROJECT_NAME}-Tests
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 10
        PROPERTIES TIMEOUT 15 LABELS unit
)

