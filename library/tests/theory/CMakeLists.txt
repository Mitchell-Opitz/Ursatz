add_ursatz_test(framework)
add_ursatz_test(style_model)
add_ursatz_test(style_model_build)
add_ursatz_test(style_model_blend)

# Reads src/theory/ to show StyleModel has no path to a Rule, so it runs from this directory.
add_executable(test_style_model_not_a_rule test_style_model_not_a_rule.c)
target_link_libraries(test_style_model_not_a_rule PRIVATE ursatz)
add_test(NAME style_model_not_a_rule COMMAND test_style_model_not_a_rule
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
