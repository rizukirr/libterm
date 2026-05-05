set(LIBTERM_LINK_TARGET libterm_static)
if(NOT LIBTERM_BUILD_STATIC)
    set(LIBTERM_LINK_TARGET libterm_shared)
endif()

function(libterm_add_example name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE ${LIBTERM_LINK_TARGET})
endfunction()

libterm_add_example(hello)
libterm_add_example(keyboard)
libterm_add_example(paint)
libterm_add_example(bench_present)
