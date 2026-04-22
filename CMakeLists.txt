cmake_minimum_required(VERSION 3.32)
project(libterm
    VERSION 0.1.0
    DESCRIPTION "Cross-platform terminal UI library (termbox2-compatible, POSIX + native Win32)"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(LIBTERM_BUILD_SHARED   "Build shared library"       ON)
option(LIBTERM_BUILD_STATIC   "Build static library"       ON)
option(LIBTERM_BUILD_EXAMPLES "Build example programs"     ON)
option(LIBTERM_BUILD_TESTS    "Build unit tests"           ON)
option(LIBTERM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

include(cmake/CompilerWarnings.cmake)

add_subdirectory(src)

if(LIBTERM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LIBTERM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/LibtermConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfig.cmake
    INSTALL_DESTINATION lib/cmake/Libterm
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfigVersion.cmake
    DESTINATION lib/cmake/Libterm
)
