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

add_executable(${PROJECT_NAME}-Tests
        Core/IoContextPoolTest.cpp
)

target_link_libraries(${PROJECT_NAME}-Tests PRIVATE ${PROJECT_NAME} GTest::gtest_main)

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
