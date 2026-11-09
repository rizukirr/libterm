cmake_minimum_required(VERSION 3.32)
project(libterm
    VERSION 0.1.3
    DESCRIPTION "Cross-platform terminal UI library (termbox2-compatible, POSIX + native Win32)"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dev-only pieces default ON only when libterm is the top-level project;
# subproject consumers (FetchContent / add_subdirectory) get just the static
# library unless they opt in explicitly.
option(LIBTERM_BUILD_SHARED   "Build shared library"       ${PROJECT_IS_TOP_LEVEL})
option(LIBTERM_BUILD_STATIC   "Build static library"       ON)
option(LIBTERM_BUILD_EXAMPLES "Build example programs"     ${PROJECT_IS_TOP_LEVEL})
option(LIBTERM_BUILD_BENCH    "Build benchmark programs"   ${PROJECT_IS_TOP_LEVEL})
option(LIBTERM_BUILD_TESTS    "Build unit tests"           ${PROJECT_IS_TOP_LEVEL})
option(LIBTERM_INSTALL        "Generate install rules"     ${PROJECT_IS_TOP_LEVEL})
option(LIBTERM_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
option(LIBTERM_BUILD_FUZZERS  "Build libFuzzer targets (clang only)" OFF)

include(cmake/CompilerWarnings.cmake)

add_subdirectory(src)

if(LIBTERM_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LIBTERM_BUILD_BENCH)
    add_subdirectory(bench)
endif()

if(LIBTERM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LIBTERM_INSTALL)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        cmake/LibtermConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfig.cmake
        INSTALL_DESTINATION lib/cmake/Libterm
    )
    # Pre-1.0 semver (see CHANGELOG): minor versions may break API, so a
    # requested 0.x must not accept a different 0.y. From 1.0 on, normal
    # same-major rules apply — computed, so no follow-up commit at 1.0.
    if(PROJECT_VERSION_MAJOR EQUAL 0)
        set(LIBTERM_VERSION_COMPAT SameMinorVersion)
    else()
        set(LIBTERM_VERSION_COMPAT SameMajorVersion)
    endif()
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ${LIBTERM_VERSION_COMPAT}
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LibtermConfigVersion.cmake
        DESTINATION lib/cmake/Libterm
    )
    # pkg-config (Meson / Autotools / Makefile consumers). The
    # ${pcfiledir}-relative prefix keeps the file working inside relocated
    # install-tree tarballs — same property the CMake package config has.
    configure_file(cmake/libterm.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/libterm.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libterm.pc
        DESTINATION lib/pkgconfig)
endif()
