add_executable(test_architecture_chain test_architecture_chain.c)
add_test(NAME architecture_chain COMMAND test_architecture_chain
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
