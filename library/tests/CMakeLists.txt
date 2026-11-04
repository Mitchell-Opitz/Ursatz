ursatz_test_includes()

function(add_ursatz_test name)
  add_executable(test_${name} test_${name}.c)
  target_link_libraries(test_${name} PRIVATE ursatz)
  add_test(NAME ${name} COMMAND test_${name})
endfunction()

function(add_ursatz_architecture_test module)
  add_executable(test_architecture_${module} test_architecture_${module}.c)
  add_test(NAME architecture_${module} COMMAND test_architecture_${module}
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

add_ursatz_architecture_test(third_party)

# Test sources follow the same layer rules as the code they test; SOURCE_DIR is the repo root.
add_executable(test_architecture_tests test_architecture_tests.c)
target_compile_definitions(test_architecture_tests PRIVATE SOURCE_DIR="${PROJECT_SOURCE_DIR}")
add_test(NAME architecture_tests COMMAND test_architecture_tests)

add_executable(test_ursatz test_ursatz.c)
target_link_libraries(test_ursatz PRIVATE ursatz)
add_test(NAME ursatz_bootstrap COMMAND test_ursatz)

add_subdirectory(identity)
add_subdirectory(core)
add_subdirectory(time)
add_subdirectory(pitch)
add_subdirectory(events)
add_subdirectory(containers)
add_subdirectory(structure)
add_subdirectory(context)
add_subdirectory(semantics)
add_subdirectory(relationship)
add_subdirectory(interpretation)
add_subdirectory(theory)
add_subdirectory(rules)
add_subdirectory(analysis)
add_subdirectory(generation)
add_subdirectory(io)
add_subdirectory(persistence)
add_subdirectory(corpus)
add_subdirectory(corpus_stats)
add_subdirectory(query)
add_subdirectory(export)
add_subdirectory(integration)
add_subdirectory(accuracy)

# Frameworks and the chain never compile kernel sources (R-241).
add_test(NAME architecture_framework_sources
         COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -P ${PROJECT_SOURCE_DIR}/cmake/check_framework_sources.cmake)
