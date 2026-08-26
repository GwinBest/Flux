find_package(Boost CONFIG QUIET COMPONENTS asio uuid)

if (Boost_FOUND)
    set(BOOST_ASIO_TARGET Boost::asio)
    set(BOOST_UUID_TARGET Boost::uuid)
else ()
    include(FetchContent)

    set(BOOST_INCLUDE_LIBRARIES asio uuid)
    FetchContent_Declare(Boost
            URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            SYSTEM
    )
    FetchContent_MakeAvailable(Boost)
    set(BOOST_ASIO_TARGET boost_asio)
    set(BOOST_UUID_TARGET boost_uuid)
endif ()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "*.cpp"
        "*.hpp"
)

add_library(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC ${BOOST_ASIO_TARGET} ${BOOST_UUID_TARGET})

if (WIN32)
    target_link_libraries(${PROJECT_NAME} PUBLIC ws2_32 mswsock)
endif ()

if (ENABLE_CLANG_TIDY AND PROJECT_CXX_CLANG_TIDY)
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${PROJECT_CXX_CLANG_TIDY}")
endif ()

enable_asan(${PROJECT_NAME})
enable_coverage(${PROJECT_NAME})
enable_project_warnings(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
