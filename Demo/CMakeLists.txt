file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "*.cpp" "*.hpp")

add_executable(${PROJECT_NAME}-Demo ${SOURCES})

target_link_libraries(${PROJECT_NAME}-Demo PRIVATE ${PROJECT_NAME})

enable_asan(${PROJECT_NAME}-Demo)
enable_coverage(${PROJECT_NAME}-Demo)
enable_project_warnings(${PROJECT_NAME}-Demo)

target_include_directories(${PROJECT_NAME}-Demo PUBLIC "${CMAKE_SOURCE_DIR}/Source" "${CMAKE_SOURCE_DIR}/Demo")
