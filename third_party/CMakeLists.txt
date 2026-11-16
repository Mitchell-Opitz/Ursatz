add_subdirectory(cjson)

# Vendored third-party code. ezxml is a private static target used by one module only; see the README
# next to it. the layer table (cmake/layer-table.txt) checks that nothing else includes it.
add_library(ursatz_ezxml STATIC ezxml/ezxml.c)
target_include_directories(ursatz_ezxml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ezxml)
target_compile_definitions(ursatz_ezxml PRIVATE EZXML_NOMMAP)
target_compile_options(ursatz_ezxml PRIVATE -w)
set_target_properties(ursatz_ezxml PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(URSATZ_GUI)
    add_subdirectory(civetweb)
endif()
