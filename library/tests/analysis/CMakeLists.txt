add_executable(test_analyzer test_analyzer.c)
target_link_libraries(test_analyzer PRIVATE ursatz)
add_test(NAME analyzer COMMAND test_analyzer)

add_executable(test_architecture_analysis test_architecture_analysis.c)
add_test(NAME architecture_analysis COMMAND test_architecture_analysis
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})