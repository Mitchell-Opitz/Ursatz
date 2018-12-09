add_executable(test_ursatz test_ursatz.c)
target_link_libraries(test_ursatz PRIVATE ursatz)
add_test(NAME ursatz_bootstrap COMMAND test_ursatz)

add_subdirectory(identity)
add_subdirectory(core)
add_subdirectory(time)