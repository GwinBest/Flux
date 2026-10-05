include(FetchContent)

enable_coverage(${PROJECT_NAME})

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
        ServerTest.cpp
        Core/AcceptorTest.cpp
        Core/ConnectionManagerTest.cpp
        Core/ConnectionTest.cpp
        Core/ConnectionIdleTimeoutTest.cpp
        Core/IoContextPoolTest.cpp
        Utils/ErrorTest.cpp
        Utils/LogTest.cpp
        Utils/SystemResourcesTest.cpp
)

target_link_libraries(${PROJECT_NAME}-Tests PRIVATE ${PROJECT_NAME} GTest::gtest_main spdlog::spdlog)

enable_asan(${PROJECT_NAME}-Tests)
enable_coverage(${PROJECT_NAME}-Tests)
enable_project_warnings(${PROJECT_NAME}-Tests)

if (ENABLE_CLANG_TIDY AND PROJECT_CXX_CLANG_TIDY)
    set_target_properties(${PROJECT_NAME}-Tests PROPERTIES CXX_CLANG_TIDY "${PROJECT_CXX_CLANG_TIDY}")
endif ()

include(GoogleTest)
if (WIN32)
    get_filename_component(COMPILER_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set_property(TARGET ${PROJECT_NAME}-Tests PROPERTY TEST_LAUNCHER
            "${CMAKE_COMMAND};-E;env;PATH=${COMPILER_RUNTIME_DIR};--"
    )
endif ()

gtest_discover_tests(${PROJECT_NAME}-Tests
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 10
        PROPERTIES TIMEOUT 15 LABELS unit
)

# ConfigureKeepAlive has internal linkage. Its tests include Acceptor.cpp in a
# separate executable so production code needs no test hooks or public API changes.
add_executable(${PROJECT_NAME}-KeepAliveTests Core/KeepAliveTest.cpp)
target_link_libraries(${PROJECT_NAME}-KeepAliveTests
        PRIVATE ${PROJECT_NAME} GTest::gtest_main spdlog::spdlog
)
enable_asan(${PROJECT_NAME}-KeepAliveTests)
enable_coverage(${PROJECT_NAME}-KeepAliveTests)
enable_project_warnings(${PROJECT_NAME}-KeepAliveTests)
if (ENABLE_CLANG_TIDY AND PROJECT_CXX_CLANG_TIDY)
    set_target_properties(${PROJECT_NAME}-KeepAliveTests
            PROPERTIES CXX_CLANG_TIDY "${PROJECT_CXX_CLANG_TIDY}"
    )
endif ()
if (WIN32)
    set_property(TARGET ${PROJECT_NAME}-KeepAliveTests PROPERTY TEST_LAUNCHER
            "${CMAKE_COMMAND};-E;env;PATH=${COMPILER_RUNTIME_DIR};--"
    )
endif ()
gtest_discover_tests(${PROJECT_NAME}-KeepAliveTests
        DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT 10
        PROPERTIES TIMEOUT 15 LABELS unit
)
