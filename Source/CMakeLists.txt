find_package(Boost CONFIG QUIET COMPONENTS asio)
find_package(spdlog CONFIG QUIET)

if (Boost_FOUND)
    set(BOOST_ASIO_TARGET Boost::asio)
else ()
    include(FetchContent)

    set(BOOST_INCLUDE_LIBRARIES asio)
    FetchContent_Declare(Boost
            URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
            URL_HASH SHA256=9BED76128D4E46755DBE818487788C6FCEB6F72B378F4DAA49B7E1E600D9088D
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            SYSTEM
    )
    FetchContent_MakeAvailable(Boost)
    set(BOOST_ASIO_TARGET boost_asio)
endif ()

if (spdlog_FOUND)
    set(SPDLOG_TARGET spdlog::spdlog)
else ()
    include(FetchContent)

    FetchContent_Declare(spdlog
            URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
            URL_HASH SHA256=D8862955C6D74E5846B3F580B1605D2428B11D97A410D86E2FB13E857CD3A744
            DOWNLOAD_EXTRACT_TIMESTAMP ON
            SYSTEM
    )
    FetchContent_MakeAvailable(spdlog)
    set(SPDLOG_TARGET spdlog::spdlog)
endif ()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        "*.cpp"
        "*.hpp"
)

add_library(${PROJECT_NAME} ${SOURCES})

if (MINGW)
    target_compile_definitions(${PROJECT_NAME} PUBLIC BOOST_ASIO_HAS_PTHREADS=1)
endif ()

target_link_libraries(${PROJECT_NAME}
        PUBLIC ${BOOST_ASIO_TARGET}
        PRIVATE ${SPDLOG_TARGET}
)

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
