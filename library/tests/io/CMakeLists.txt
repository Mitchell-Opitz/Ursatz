add_ursatz_test(smf_reader)
add_ursatz_test(midi_event_decoder)
add_ursatz_test(midi_importer)
add_ursatz_architecture_test(io)

# Scoped memcheck coverage for this module's own test binaries only
# (not a repo-wide valgrind mechanism -- see docs/modules/io.md). Skips
# silently if valgrind isn't installed.
find_program(URSATZ_IO_VALGRIND valgrind)
if(URSATZ_IO_VALGRIND)
  foreach(io_test_name smf_reader midi_event_decoder midi_importer)
    add_test(NAME ${io_test_name}_valgrind
             COMMAND ${URSATZ_IO_VALGRIND} --leak-check=full --error-exitcode=1
                     $<TARGET_FILE:test_${io_test_name}>)
  endforeach()
endif()
