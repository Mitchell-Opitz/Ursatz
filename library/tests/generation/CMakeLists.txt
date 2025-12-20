add_ursatz_test(intent)
add_ursatz_test(intent_compiler)
add_ursatz_test(constraint_extraction)
add_ursatz_test(conflict_detection)
add_ursatz_test(generation_plan)
add_ursatz_test(generator)
add_ursatz_test(generator_registry)
add_ursatz_test(generator_dispatch)
add_ursatz_test(generator_reproducibility)
add_ursatz_test(candidate)
add_ursatz_test(candidate_evaluator)
add_ursatz_test(generation_record)
add_ursatz_architecture_test(generation)

# Scoped memcheck coverage for the generator registry, dispatch and
# reproducibility test binaries only (not a repo-wide valgrind mechanism -- see
# tests/persistence/CMakeLists.txt for the same pattern). Skips silently if
# valgrind isn't installed.
find_program(URSATZ_GENERATION_VALGRIND valgrind)
if(URSATZ_GENERATION_VALGRIND)
  foreach(generation_test_name generator_registry generator_dispatch generator_reproducibility)
    add_test(NAME ${generation_test_name}_valgrind
             COMMAND ${URSATZ_GENERATION_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${generation_test_name}>)
  endforeach()
endif()
