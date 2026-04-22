function(libterm_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(LIBTERM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
            -Wshadow -Wstrict-prototypes -Wmissing-prototypes
            -Wpointer-arith -Wcast-align -Wwrite-strings
        )
        if(LIBTERM_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
