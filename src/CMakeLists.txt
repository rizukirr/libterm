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

set(LIBTERM_ALL_SOURCES
    ${LIBTERM_SHARED_SOURCES}
    ${LIBTERM_PLATFORM_SOURCES}
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

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/libterm
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
