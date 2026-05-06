set(LIBTERM_SHARED_SOURCES
    shared/api.c
    shared/buffer.c
    shared/cell.c
    shared/errors.c
    shared/input.c
    shared/output.c
    shared/utf8.c
)

if(WIN32)
    set(LIBTERM_PLATFORM_SOURCES
        platform/windows/plat_init.c
        platform/windows/plat_input.c
        platform/windows/plat_keys.c
        platform/windows/plat_output.c
        platform/windows/plat_resize.c
    )
else()
    set(LIBTERM_PLATFORM_SOURCES
        platform/posix/plat_init.c
        platform/posix/plat_input.c
        platform/posix/plat_output.c
        platform/posix/plat_resize.c
        platform/posix/plat_select.c
    )
endif()

# --- SIMD ISA selection ---
set(LIBTERM_SIMD "auto" CACHE STRING
    "SIMD ISA: auto|scalar|avx2|avx512|neon|sve|rvv")
set_property(CACHE LIBTERM_SIMD PROPERTY STRINGS
    auto scalar avx2 avx512 neon sve rvv)

if(LIBTERM_SIMD STREQUAL "auto")
    include(CheckCCompilerFlag)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        if(MSVC)
            check_c_compiler_flag("/arch:AVX2" LIBTERM_SIMD_AVX2)
        else()
            check_c_compiler_flag("-mavx2" LIBTERM_SIMD_AVX2)
        endif()
        if(LIBTERM_SIMD_AVX2)
            set(LIBTERM_SIMD_RESOLVED "avx2")
        else()
            set(LIBTERM_SIMD_RESOLVED "scalar")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        set(LIBTERM_SIMD_RESOLVED "neon")
    else()
        set(LIBTERM_SIMD_RESOLVED "scalar")
    endif()
else()
    set(LIBTERM_SIMD_RESOLVED ${LIBTERM_SIMD})
endif()

set(LIBTERM_SIMD_FLAGS "")
if(LIBTERM_SIMD_RESOLVED STREQUAL "avx2")
    if(MSVC)
        set(LIBTERM_SIMD_FLAGS "/arch:AVX2")
    else()
        set(LIBTERM_SIMD_FLAGS "-mavx2")
    endif()
elseif(LIBTERM_SIMD_RESOLVED STREQUAL "avx512")
    if(MSVC)
        set(LIBTERM_SIMD_FLAGS "/arch:AVX512")
    else()
        set(LIBTERM_SIMD_FLAGS "-mavx512f;-mavx512bw")
    endif()
elseif(LIBTERM_SIMD_RESOLVED STREQUAL "sve")
    set(LIBTERM_SIMD_FLAGS "-march=armv8-a+sve")
elseif(LIBTERM_SIMD_RESOLVED STREQUAL "rvv")
    set(LIBTERM_SIMD_FLAGS "-march=rv64gcv")
endif()

set(LIBTERM_SIMD_TU "intrinsics/${LIBTERM_SIMD_RESOLVED}.c")
message(STATUS "libterm SIMD: ${LIBTERM_SIMD_RESOLVED} (flags: ${LIBTERM_SIMD_FLAGS})")

set(LIBTERM_ALL_SOURCES
    ${LIBTERM_SHARED_SOURCES}
    ${LIBTERM_PLATFORM_SOURCES}
    ${LIBTERM_SIMD_TU}
)

set(LIBTERM_PRIVATE_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(LIBTERM_PUBLIC_INCLUDES
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# --- static ---
if(LIBTERM_BUILD_STATIC)
    add_library(libterm_static STATIC ${LIBTERM_ALL_SOURCES})
    set_target_properties(libterm_static PROPERTIES
        OUTPUT_NAME libterm
        C_VISIBILITY_PRESET hidden
    )
    target_compile_definitions(libterm_static PRIVATE LIBTERM_BUILDING)
    target_include_directories(libterm_static
        PUBLIC  ${LIBTERM_PUBLIC_INCLUDES}
        PRIVATE ${LIBTERM_PRIVATE_INCLUDES}
    )
    libterm_set_warnings(libterm_static)
    add_library(Libterm::static ALIAS libterm_static)
endif()

# --- shared ---
if(LIBTERM_BUILD_SHARED)
    add_library(libterm_shared SHARED ${LIBTERM_ALL_SOURCES})
    set_target_properties(libterm_shared PROPERTIES
        OUTPUT_NAME libterm
        C_VISIBILITY_PRESET hidden
        VERSION   ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_compile_definitions(libterm_shared
        PRIVATE LIBTERM_BUILDING
        PUBLIC  LIBTERM_SHARED
    )
    target_include_directories(libterm_shared
        PUBLIC  ${LIBTERM_PUBLIC_INCLUDES}
        PRIVATE ${LIBTERM_PRIVATE_INCLUDES}
    )
    libterm_set_warnings(libterm_shared)
    add_library(Libterm::shared ALIAS libterm_shared)
endif()

# default alias: prefer static if built, else shared
if(LIBTERM_BUILD_STATIC)
    add_library(Libterm::libterm ALIAS libterm_static)
elseif(LIBTERM_BUILD_SHARED)
    add_library(Libterm::libterm ALIAS libterm_shared)
endif()

# Apply per-TU compile flags to the SIMD source.
if(LIBTERM_SIMD_FLAGS)
    set_source_files_properties(${LIBTERM_SIMD_TU} PROPERTIES
        COMPILE_OPTIONS "${LIBTERM_SIMD_FLAGS}")
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/libterm
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
