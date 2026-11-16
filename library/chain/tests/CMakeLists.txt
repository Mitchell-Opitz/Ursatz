ursatz_test_includes()

ursatz_test_executable(test_form_bridge LIBS ursatz_chain)
ursatz_test(chain_form_bridge TARGET test_form_bridge VALGRIND)

ursatz_test(chain_inputs LIBS ursatz_chain VALGRIND)

# The chain on four reference pieces against fixed expected results (captured while it was proven equal to the test glue it replaced).
# 13 s measured (serial, 10 s rule); runs in the nightly.
ursatz_test(analysis_chain LIBS ursatz_chain ursatz_test_support
    DEFINITIONS URSATZ_CORPUS_DIR="${URSATZ_FIXTURES_DIR}/framework-v1-corpus" LABELS slow)

# The smallest fixture only, to keep the memcheck run short.
if(URSATZ_VALGRIND)
    ursatz_test(analysis_chain_valgrind
        COMMAND ${URSATZ_VALGRIND} ${URSATZ_VALGRIND_ARGS} $<TARGET_FILE:test_analysis_chain>
        ENVIRONMENT "URSATZ_CHAIN_PIECE=field1" LABELS slow valgrind)
endif()

ursatz_test_executable(test_kernel_owners LIBS ursatz_chain)
ursatz_test(chain_kernel_owners TARGET test_kernel_owners VALGRIND)
