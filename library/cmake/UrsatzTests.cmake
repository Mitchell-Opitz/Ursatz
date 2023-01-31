# Registers the calling directory's tests/ subdirectory when URSATZ_BUILD_TESTS
# is on. Used by the root build and by every frameworks/<name>/CMakeLists.txt.
macro(ursatz_add_tests)
    if(URSATZ_BUILD_TESTS)
        add_subdirectory(tests)
    endif()
endmacro()
