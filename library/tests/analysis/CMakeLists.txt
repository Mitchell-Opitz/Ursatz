add_executable(test_architecture_analysis test_architecture_analysis.c)
add_test(NAME architecture_analysis COMMAND test_architecture_analysis
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})