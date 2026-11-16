ursatz_test_includes()

add_executable(test_form_bridge test_form_bridge.c)
target_link_libraries(test_form_bridge PRIVATE ursatz_chain)
add_test(NAME chain_form_bridge COMMAND test_form_bridge)

add_executable(test_chain_inputs test_chain_inputs.c)
target_link_libraries(test_chain_inputs PRIVATE ursatz_chain)
add_test(NAME chain_inputs COMMAND test_chain_inputs)

# The chain on four reference pieces against fixed expected results (captured while it was proven equal to the test glue it replaced).
add_executable(test_analysis_chain test_analysis_chain.c)
target_link_libraries(test_analysis_chain PRIVATE ursatz_chain)
target_compile_definitions(test_analysis_chain PRIVATE
    URSATZ_CORPUS_DIR="${PROJECT_SOURCE_DIR}/tests/fixtures/framework-v1-corpus")
add_test(NAME analysis_chain COMMAND test_analysis_chain)
# 13 s measured (serial, 10 s rule); runs in the nightly.
set_tests_properties(analysis_chain PROPERTIES LABELS slow)

# Memcheck for the bridges and the input builders. Skips silently if valgrind isn't installed.
find_program(URSATZ_CHAIN_VALGRIND valgrind)
if(URSATZ_CHAIN_VALGRIND)
  add_test(NAME chain_form_bridge_valgrind
           COMMAND ${URSATZ_CHAIN_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_form_bridge>)
  # the smallest fixture only, to keep CI time
  add_test(NAME analysis_chain_valgrind
           COMMAND ${URSATZ_CHAIN_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_analysis_chain>)
  set_tests_properties(analysis_chain_valgrind PROPERTIES ENVIRONMENT "URSATZ_CHAIN_PIECE=field1")
  # 14 s measured (serial, 10 s rule); runs in the nightly.
  set_tests_properties(analysis_chain_valgrind PROPERTIES LABELS slow)
  add_test(NAME chain_inputs_valgrind
           COMMAND ${URSATZ_CHAIN_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_chain_inputs>)
endif()

add_executable(test_kernel_owners test_kernel_owners.c)
target_link_libraries(test_kernel_owners PRIVATE ursatz_chain)
add_test(NAME chain_kernel_owners COMMAND test_kernel_owners)
if(URSATZ_CHAIN_VALGRIND)
  add_test(NAME chain_kernel_owners_valgrind
           COMMAND ${URSATZ_CHAIN_VALGRIND} --leak-check=full --error-exitcode=1
                   $<TARGET_FILE:test_kernel_owners>)
endif()
