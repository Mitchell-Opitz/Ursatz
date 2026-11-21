# civetweb v1.16 (MIT, https://github.com/civetweb/civetweb, tag v1.16), vendored: src/civetweb.c (with the upstream fix for
# CVE-2025-55763 applied, see README.md), the src/*.inl files it includes, include/civetweb.h and LICENSE.md. Built without SSL, as the GUI's server needs none.
#
# USE_STACK_SIZE: civetweb builds its request threads with a 100 KB stack by default. The Composer API runs on those
# threads (R-219) and some of its entries need more (a random motive draws into several motives on the stack), so a
# request crashed the server. 32 MB is address space only; the pages are used when a call needs them.
find_package(Threads REQUIRED)

add_library(civetweb-c-library STATIC src/civetweb.c)
target_include_directories(civetweb-c-library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(civetweb-c-library PRIVATE
    NO_SSL
    USE_IPV6
    USE_STACK_SIZE=33554432
    $<$<CONFIG:Debug>:DEBUG>)
target_compile_options(civetweb-c-library PRIVATE -w)
target_link_libraries(civetweb-c-library PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
